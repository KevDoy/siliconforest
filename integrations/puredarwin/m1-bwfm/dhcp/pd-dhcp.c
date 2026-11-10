/*
 * pd-dhcp - minimal DHCPv4 client for the PureDarwin RAM root.
 *
 * Copyright (c) 2026 darwin-lab. SPDX-License-Identifier: ISC
 *
 * DISCOVER/OFFER/REQUEST/ACK over BPF on one Ethernet-like interface, then
 * hands the lease to /bin/netsetup, which sets the address, default route and
 * resolver. No renewal: this is for bring-up sessions, not a lease daemon.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DHCP_MAGIC 0x63825363u

struct lease {
    uint32_t addr, mask, router, dns, server, secs;
    int type;
};

static uint16_t csum(const uint8_t *p, size_t n)
{
    uint32_t s = 0;
    for (; n > 1; p += 2, n -= 2)
        s += (uint32_t)((p[0] << 8) | p[1]);
    if (n)
        s += (uint32_t)(p[0] << 8);
    while (s >> 16)
        s = (s & 0xffff) + (s >> 16);
    return (uint16_t)~s;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static uint32_t get32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static int get_mac(const char *ifname, uint8_t mac[6])
{
    struct ifaddrs *ifa, *p;
    int ok = -1;

    if (getifaddrs(&ifa) != 0)
        return -1;
    for (p = ifa; p != NULL; p = p->ifa_next) {
        if (p->ifa_addr != NULL && p->ifa_addr->sa_family == AF_LINK && strcmp(p->ifa_name, ifname) == 0) {
            struct sockaddr_dl *dl = (struct sockaddr_dl *)(void *)p->ifa_addr;
            if (dl->sdl_alen == 6) {
                memcpy(mac, LLADDR(dl), 6);
                ok = 0;
            }
        }
    }
    freeifaddrs(ifa);
    return ok;
}

static int bpf_open(const char *ifname, u_int *blen)
{
    struct ifreq ifr;
    struct timeval tv = { 1, 0 };
    u_int one = 1;
    char path[32];
    int fd = -1;

    for (int i = 0; i < 32 && fd < 0; i++) {
        snprintf(path, sizeof(path), "/dev/bpf%d", i);
        fd = open(path, O_RDWR);
    }
    if (fd < 0)
        return -1;
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, BIOCGBLEN, blen) < 0 || ioctl(fd, BIOCSETIF, &ifr) < 0 ||
        ioctl(fd, BIOCIMMEDIATE, &one) < 0 || ioctl(fd, BIOCSHDRCMPLT, &one) < 0 ||
        ioctl(fd, BIOCSRTIMEOUT, &tv) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* BPF refuses to send on an interface that is not IFF_UP ("Network is down"). */
static int if_up(const char *ifname)
{
    struct ifreq ifr;
    int s = socket(AF_INET, SOCK_DGRAM, 0), rc = -1;

    if (s < 0)
        return -1;
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0) {
        if (ifr.ifr_flags & IFF_UP) {
            rc = 0;
        } else {
            ifr.ifr_flags |= IFF_UP;
            rc = ioctl(s, SIOCSIFFLAGS, &ifr);
        }
    }
    close(s);
    return rc;
}

/* Builds an Ethernet/IP/UDP/BOOTP frame. Returns its length. */
static size_t build(uint8_t *f, const uint8_t mac[6], uint32_t xid, int type, const struct lease *l)
{
    uint8_t *ip = f + 14, *udp = ip + 20, *b = udp + 8, *o;
    size_t blen, ulen;

    memset(f, 0, 600);
    memset(f, 0xff, 6);
    memcpy(f + 6, mac, 6);
    f[12] = 0x08; f[13] = 0x00;

    b[0] = 1; b[1] = 1; b[2] = 6;               /* BOOTREQUEST, Ethernet */
    put32(b + 4, xid);
    b[10] = 0x80;                               /* broadcast flag */
    memcpy(b + 28, mac, 6);
    put32(b + 236, DHCP_MAGIC);
    o = b + 240;
    *o++ = 53; *o++ = 1; *o++ = (uint8_t)type;
    *o++ = 61; *o++ = 7; *o++ = 1; memcpy(o, mac, 6); o += 6;
    if (type == 3) {
        *o++ = 50; *o++ = 4; put32(o, l->addr); o += 4;
        *o++ = 54; *o++ = 4; put32(o, l->server); o += 4;
    }
    *o++ = 55; *o++ = 4; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 51;
    *o++ = 255;
    blen = (size_t)(o - b);
    if (blen < 300)
        blen = 300;
    ulen = 8 + blen;

    udp[0] = 0; udp[1] = 68; udp[2] = 0; udp[3] = 67;
    udp[4] = (uint8_t)(ulen >> 8); udp[5] = (uint8_t)ulen;      /* checksum 0: not used */

    ip[0] = 0x45; ip[2] = (uint8_t)((20 + ulen) >> 8); ip[3] = (uint8_t)(20 + ulen);
    ip[8] = 64; ip[9] = 17;
    put32(ip + 16, 0xffffffffu);
    {
        uint16_t c = csum(ip, 20);
        ip[10] = (uint8_t)(c >> 8); ip[11] = (uint8_t)c;
    }
    return 14 + 20 + ulen;
}

/* Parses one captured frame; fills l and returns the DHCP message type, or 0. */
static int parse(const uint8_t *f, size_t n, const uint8_t mac[6], uint32_t xid, struct lease *l)
{
    const uint8_t *ip, *udp, *b, *o, *end;
    size_t ihl;

    if (n < 14 + 20 + 8 + 240 || f[12] != 0x08 || f[13] != 0x00)
        return 0;
    ip = f + 14;
    ihl = (size_t)(ip[0] & 0xf) * 4;
    if ((ip[0] >> 4) != 4 || ip[9] != 17 || n < 14 + ihl + 8 + 240)
        return 0;
    udp = ip + ihl;
    if (udp[2] != 0 || udp[3] != 68)
        return 0;
    b = udp + 8;
    end = f + n;
    if (b[0] != 2 || get32(b + 4) != xid || memcmp(b + 28, mac, 6) != 0 || get32(b + 236) != DHCP_MAGIC)
        return 0;
    memset(l, 0, sizeof(*l));
    l->addr = get32(b + 16);
    for (o = b + 240; o < end && *o != 255;) {
        uint8_t code = *o++, len;
        if (code == 0)
            continue;
        if (o >= end)
            break;
        len = *o++;
        if (o + len > end)
            break;
        if (code == 53 && len >= 1) l->type = o[0];
        if (code == 1 && len >= 4) l->mask = get32(o);
        if (code == 3 && len >= 4) l->router = get32(o);
        if (code == 6 && len >= 4) l->dns = get32(o);
        if (code == 54 && len >= 4) l->server = get32(o);
        if (code == 51 && len >= 4) l->secs = get32(o);
        o += len;
    }
    return l->type;
}

static int wait_for(int fd, uint8_t *buf, u_int blen, const uint8_t mac[6], uint32_t xid, int want, struct lease *l, int seconds)
{
    time_t end = time(NULL) + seconds;

    while (time(NULL) <= end) {
        ssize_t n = read(fd, buf, blen);
        uint8_t *p = buf;

        if (n <= 0)
            continue;
        while (p < buf + n) {
            struct bpf_hdr *h = (struct bpf_hdr *)(void *)p;
            int t = parse(p + h->bh_hdrlen, h->bh_caplen, mac, xid, l);

            if (t == want)
                return 0;
            if (t == 6) {
                fprintf(stderr, "pd-dhcp: server sent NAK\n");
                return -1;
            }
            p += BPF_WORDALIGN(h->bh_hdrlen + h->bh_caplen);
        }
    }
    return -1;
}

static const char *ipstr(uint32_t a, char *s)
{
    snprintf(s, 16, "%u.%u.%u.%u", a >> 24, (a >> 16) & 255, (a >> 8) & 255, a & 255);
    return s;
}

int main(int argc, char **argv)
{
    int uponly = argc > 1 && strcmp(argv[1], "--up") == 0;
    const char *ifname = argc > 1 + uponly ? argv[1 + uponly] : "en0";
    uint8_t mac[6], frame[600], *buf;
    struct lease offer, ack;
    char a[16], m[16], r[16], d[16], s[16];
    uint32_t xid;
    u_int blen = 0;
    int fd, got = 0;

    if (if_up(ifname) != 0)
        fprintf(stderr, "pd-dhcp: cannot mark %s up: %s\n", ifname, strerror(errno));
    if (uponly) {
        printf("pd-dhcp: %s marked up\n", ifname);
        return 0;
    }
    if (get_mac(ifname, mac) != 0) {
        fprintf(stderr, "pd-dhcp: no link address for %s\n", ifname);
        return 1;
    }
    fd = bpf_open(ifname, &blen);
    if (fd < 0 || (buf = malloc(blen)) == NULL) {
        fprintf(stderr, "pd-dhcp: cannot open bpf on %s: %s\n", ifname, strerror(errno));
        return 1;
    }
    xid = ((uint32_t)mac[2] << 24 | (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | mac[5]) ^ (uint32_t)time(NULL) ^ ((uint32_t)getpid() << 8);
    printf("pd-dhcp: %s %02x:%02x:%02x:%02x:%02x:%02x xid %08x\n", ifname, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], xid);

    for (int try = 0; try < 6 && !got; try++) {
        size_t n = build(frame, mac, xid, 1, NULL);

        if (write(fd, frame, n) != (ssize_t)n) {
            fprintf(stderr, "pd-dhcp: discover write: %s\n", strerror(errno));
            sleep(1);
            continue;
        }
        printf("pd-dhcp: DISCOVER sent (try %d)\n", try + 1);
        if (wait_for(fd, buf, blen, mac, xid, 2, &offer, 4) != 0)
            continue;
        printf("pd-dhcp: OFFER %s from %s\n", ipstr(offer.addr, a), ipstr(offer.server, s));
        n = build(frame, mac, xid, 3, &offer);
        if (write(fd, frame, n) != (ssize_t)n)
            continue;
        if (wait_for(fd, buf, blen, mac, xid, 5, &ack, 4) == 0)
            got = 1;
    }
    close(fd);
    if (!got) {
        fprintf(stderr, "pd-dhcp: no lease\n");
        return 1;
    }
    if (ack.mask == 0)
        ack.mask = 0xffffff00u;
    printf("PD_DHCP_LEASE addr=%s mask=%s router=%s dns=%s lease=%us\n", ipstr(ack.addr, a), ipstr(ack.mask, m),
           ipstr(ack.router, r), ipstr(ack.dns ? ack.dns : ack.router, d), ack.secs);
    fflush(stdout);
    {
        /* netsetup defaults the broadcast address to a fixed test network; pass the real one */
        char bc[16];
        ipstr(ack.addr | ~ack.mask, bc);
        execl("/bin/netsetup", "netsetup", ifname, a, m, r, d, bc, (char *)NULL);
    }
    fprintf(stderr, "pd-dhcp: exec netsetup: %s\n", strerror(errno));
    return 1;
}
