// SPDX-License-Identifier: MIT
// what PDBwfm.cpp needs from bwfm(4) without seeing its softc, the device tree answers bwfm
// asks for, and OpenBSD's bus_dma on the DART: per-buffer mapping (pd_dart_map.h), or the
// bounce pool (pd_dart_pool.h) with PD_BWFM_DMA_BOUNCE. Derived from PDQwx's pd_qwx_glue.c.

#include "pd_bwfm_compat.h"
#include <net80211/ieee80211_var.h>

#include "bwfmvar.h"
#include "pd_bwfm_glue.h"
#include "pd_dart_pool.h"
#include "pd_dart_map.h"

// PD_BWFM_DMA_BOUNCE brings back the fixed bounce pool, the default maps every buffer
#ifdef PD_BWFM_DMA_BOUNCE
#define pd_bwfm_dma_init()	pd_dart_pool_init()
#define pd_bwfm_dma_check(why)	pd_dart_pool_check(why)
#define PD_BWFM_DMA_WHAT	"DMA pool"
#else
#define pd_bwfm_dma_init()	pd_dart_init()
#define pd_bwfm_dma_check(why)	pd_dart_check(why)
#define PD_BWFM_DMA_WHAT	"DART mapper"
#endif

extern const struct cfattach bwfm_pci_ca;

// the attach hook bwfm defers to after the root mount, one per device
struct pd_bwfm_hook {
	struct device	*dev;
	void		(*fn)(struct device *);
};
static struct pd_bwfm_hook pd_bwfm_hooks[2];

void
pd_bwfm_mountroot(struct device *dev, void (*fn)(struct device *))
{
	for (int i = 0; i < (int)nitems(pd_bwfm_hooks); i++) {
		if (pd_bwfm_hooks[i].dev == NULL || pd_bwfm_hooks[i].dev == dev) {
			pd_bwfm_hooks[i].dev = dev;
			pd_bwfm_hooks[i].fn = fn;
			return;
		}
	}
	printf("%s: no room for the attach hook\n", dev->dv_xname);
}

static void
(*pd_bwfm_take_hook(struct device *dev))(struct device *)
{
	for (int i = 0; i < (int)nitems(pd_bwfm_hooks); i++) {
		if (pd_bwfm_hooks[i].dev == dev) {
			void (*fn)(struct device *) = pd_bwfm_hooks[i].fn;

			pd_bwfm_hooks[i].dev = NULL;
			pd_bwfm_hooks[i].fn = NULL;
			return fn;
		}
	}
	return NULL;
}

struct device *
pd_bwfm_alloc(const char *name)
{
	struct device *dev = malloc(bwfm_pci_ca.ca_devsize, M_DEVBUF, M_WAITOK | M_ZERO);

	if (dev == NULL)
		return NULL;
	strlcpy(dev->dv_xname, name, sizeof(dev->dv_xname));
	return dev;
}

void
pd_bwfm_free(struct device *dev)
{
	free(dev, M_DEVBUF, bwfm_pci_ca.ca_devsize);
}

int
pd_bwfm_match(struct pci_attach_args *pa)
{
	return bwfm_pci_ca.ca_match(NULL, NULL, pa);
}

// the pci half: both BARs mapped, the interrupt established, net80211 attached, no firmware yet
int
pd_bwfm_attach(struct device *dev, struct pci_attach_args *pa)
{
	pd80211_lock();
	bwfm_pci_ca.ca_attach(NULL, dev, pa);
	pd80211_unlock();

	// bwfm_pci_attach registers its hook only once everything is set up
	for (int i = 0; i < (int)nitems(pd_bwfm_hooks); i++) {
		if (pd_bwfm_hooks[i].dev == dev)
			return 0;
	}
	return ENXIO;
}

static void	pd_bwfm_dma_report(const char *);

// the firmware half, run once the root file system is there. bwfm sleeps for every control
// message, which gives up the stack lock, so the lock is held around the whole hook
int
pd_bwfm_attach_hook(struct device *dev)
{
	void (*fn)(struct device *) = pd_bwfm_take_hook(dev);
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;

	if (fn == NULL)
		return ENXIO;
	if (pd_bwfm_dma_init() != 0) {
		printf("%s: the " PD_BWFM_DMA_WHAT " did not come up\n", dev->dv_xname);
		return ENOMEM;
	}
	pd80211_lock();
	// a bring-up that fails halts the chip and frees what it took, see bwfm_attachhook
	fn(dev);
	if (!sc->sc_initialized)
		pd_bwfm_dma_check("bring-up failed");
	pd80211_unlock();
	pd_bwfm_dma_report(dev->dv_xname);
	return sc->sc_initialized ? 0 : EIO;
}

// the lifecycle after the bring-up. Each of these takes the stack lock and waits for an init
// or stop that is under way (bwfm_lifecycle_enter), so they may come from any thread

// why the last control message to the firmware failed, for the stage logs
const char *
pd_bwfm_ctlwhy(struct device *dev)
{
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;

	return sc->sc_bus_ops != NULL && sc->sc_bus_ops->bs_ctlwhy != NULL ?
	    sc->sc_bus_ops->bs_ctlwhy(sc) : "?";
}

int
pd_bwfm_running(struct device *dev)
{
	return (((struct bwfm_softc *)dev)->sc_ic.ic_if.if_flags & IFF_RUNNING) != 0;
}

// The radio goes down for good until pd_bwfm_up or the next interface up: interface stopped
// (scan aborted, disassociated, flow rings deleted), with inform the firmware told (D3
// inform), then the chip masked and reset and every ring, buffer and DMA mapping released.
// Returns 0 when the firmware acknowledged D3, an errno when it did not or was not asked.
int
pd_bwfm_down(struct device *dev, int inform)
{
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;
	int error;

	pd80211_lock();
	error = bwfm_radio_down(sc, inform);
	pd80211_unlock();
	// the DART is behind the port: not read once that is down
	if (!sc->sc_bus_gone)
		pd_bwfm_dma_check("radio down");
	pd_bwfm_dma_report(dev->dv_xname);
	return inform ? error : ENXIO;
}

// the interface comes back if it is configured up: the firmware is loaded again when it is
// not running. Returns 0 when the interface runs afterwards
int
pd_bwfm_up(struct device *dev)
{
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int running;

	pd80211_lock();
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP)
		bwfm_init(ifp);
	running = (ifp->if_flags & IFF_RUNNING) != 0;
	pd80211_unlock();
	pd_bwfm_dma_report(dev->dv_xname);
	return running ? 0 : EIO;
}

// after the port was powered down or the system slept: the DART translates again. Only
// with the port up. The bounce pool has no such path
int
pd_bwfm_dma_resume(void)
{
#ifdef PD_BWFM_DMA_BOUNCE
	printf(PD_BWFM_NAME ": the DMA pool cannot be programmed again after a power loss\n");
	return ENOTSUP;
#else
	return pd_dart_resume();
#endif
}

// The port is about to lose power (gone 1: a firmware that still runs is halted first), is
// found dead (gone 2: nothing at all is touched) or is back (gone 0). While gone neither the
// bus nor this file touches a register of the chip or the DART
void
pd_bwfm_set_gone(struct device *dev, int gone)
{
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;

	pd80211_lock();
	bwfm_lifecycle_enter(sc);
	if (gone == 2)
		sc->sc_bus_gone = 1;
	if (sc->sc_bus_ops->bs_gone != NULL)
		sc->sc_bus_ops->bs_gone(sc, gone);
	sc->sc_bus_gone = gone != 0;
	bwfm_lifecycle_exit(sc);
	pd80211_unlock();
}

// IO80211Controller's hooks point at the controller: once it detached its interface no frame
// and no link change may go there. pd80211_hook allocated the block and nothing frees it
void
pd_bwfm_unhook(struct device *dev)
{
	struct ifnet *ifp = &((struct bwfm_softc *)dev)->sc_ic.ic_if;
	void *hooks;

	pd80211_lock();
	hooks = ifp->if_pdowner;
	ifp->if_pdinput = NULL;
	ifp->if_pdlinkchange = NULL;
	ifp->if_pdowner = NULL;
	pd80211_unlock();
	if (hooks != NULL)
		free(hooks, M_DEVBUF, 0);
}

// The softc goes away after this. In order: interface stopped and bus halted if they still
// were up (pd_bwfm_down did that already in the driver's stop), control messages out of the
// softc, task thread drained, net80211 detached, blobs freed. Works after a bring-up that
// never ran or failed: every step looks at what exists. Returns non-zero when a control
// message or the task thread did not get out in time: the softc must then not be freed.
int
pd_bwfm_detach(struct device *dev)
{
	struct bwfm_softc *sc = (struct bwfm_softc *)dev;
	int busy;

	pd80211_lock();
	// one piece: no init slips in between the halt and the detach
	bwfm_lifecycle_enter(sc);
	bwfm_radio_down(sc, 0);
	busy = bwfm_pci_ca.ca_detach(dev, 0);
	bwfm_lifecycle_exit(sc);
	pd80211_unlock();
	// a hook that never ran
	(void)pd_bwfm_take_hook(dev);
	pd_bwfm_dma_report(dev->dv_xname);
	return busy;
}

struct ieee80211com *
pd_bwfm_ic(struct device *dev)
{
	return &((struct bwfm_softc *)dev)->sc_ic;
}

int
if_setlladdr(struct ifnet *ifp, const uint8_t *lladdr)
{
	memcpy(((struct arpcom *)ifp)->ac_enaddr, lladdr, ETHER_ADDR_LEN);
	return 0;
}

// a chain becomes one cluster, the caller's pointer follows the new mbuf
int
pd_bwfm_m_defrag(struct mbuf **mp, int how)
{
	struct mbuf *m = *mp, *n;
	int len = M_PKTLEN(m);

	if (M_NEXT(m) == NULL)
		return 0;
	n = pd_bwfm_mclgetl(how, len);
	if (n == NULL) {
		PD_LOGN(4, "PDBwfm: defrag: no buffer for %d bytes\n", len);
		return ENOBUFS;
	}
	/* The driver only needs the bytes and the lengths; the packet header copy
	 * failed on hardware for every frame and is not needed for transmit. */
	m_copydata(m, 0, len, mtod(n, caddr_t));
	M_SETLEN(n, len);
	M_SETPKTLEN(n, len);
	m_freem(m);
	*mp = n;
	return 0;
}

static void
pd_bwfm_extfree(caddr_t buf, u_int size, caddr_t arg)
{
	(void)size;
	(void)arg;
	free(buf, M_DEVBUF, 0);
}

// a packet header mbuf with len bytes of contiguous storage. Control and event buffers are
// 8 KiB, where the mbuf allocator may only have a chain to offer, then the storage is ours
struct mbuf *
pd_bwfm_mclgetl(int how, u_int len)
{
	unsigned int chunks = 1;
	mbuf_t m = NULL;
	caddr_t buf;

	if (mbuf_allocpacket(how, len, &chunks, &m) == 0) {
		if (mbuf_next(m) == NULL && mbuf_maxlen(m) >= len)
			return m;
		mbuf_freem(m);
	}
	m = NULL;
	buf = malloc(len, M_DEVBUF, M_NOWAIT);
	if (buf == NULL)
		return NULL;
	if (mbuf_gethdr(how, MBUF_TYPE_DATA, &m) != 0) {
		free(buf, M_DEVBUF, len);
		return NULL;
	}
	if (mbuf_attachcluster(how, MBUF_TYPE_DATA, &m, buf, pd_bwfm_extfree, len, NULL) != 0) {
		mbuf_freem(m);
		free(buf, M_DEVBUF, len);
		return NULL;
	}
	return m;
}

// the board

/*
 * Board identity, used to pick the firmware and NVRAM files.
 *
 * On Linux the board type is in the device tree source ("brcm,board-type", e.g.
 * apple,honshu for the J293) and the antenna SKU comes from m1n1, which copies bytes
 * 8..15 of /arm-io/wlan wifi-antenna-sku-info into "apple,antenna-sku". Here both come
 * from the Apple device tree: the board type is "apple," plus the lower-cased
 * /arm-io/wlan module-instance, which is the same name on the J293. The boot-args
 * bwfm_board= and bwfm_antenna= override them.
 */
static void
pd_bwfm_board_identity(char *board, size_t boardlen, char *antenna, size_t antennalen)
{
	static int logged;
	uint8_t info[16];
	char adt[64];
	const char *boardsrc = "missing", *antsrc = "missing";
	int n;

	board[0] = antenna[0] = '\0';
	/* m1n1: brcm,board-type is "apple," + lower-cased /arm-io/wlan module-instance */
	n = pd_bwfm_adt_prop("/arm-io/wlan", "module-instance", adt, sizeof(adt) - 1);
	if (n > 0) {
		adt[MIN(n, (int)sizeof(adt) - 1)] = '\0';
		for (char *c = adt; *c != '\0'; c++) {
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
		}
		snprintf(board, boardlen, "apple,%s", adt);
		boardsrc = "device tree";
	}
	/* m1n1: apple,antenna-sku is bytes 8..15 of /arm-io/wlan wifi-antenna-sku-info */
	n = pd_bwfm_adt_prop("/arm-io/wlan", "wifi-antenna-sku-info", info, sizeof(info));
	if (n >= (int)sizeof(info)) {
		memcpy(adt, &info[8], 8);
		adt[8] = '\0';
		strlcpy(antenna, adt, antennalen);
		antsrc = "device tree";
	}
	if (pd_bwfm_bootarg("bwfm_board", board, (int)boardlen))
		boardsrc = "boot-arg";
	if (pd_bwfm_bootarg("bwfm_antenna", antenna, (int)antennalen))
		antsrc = "boot-arg";
	if (logged)
		return;
	logged = 1;
	printf(PD_BWFM_NAME ": board type %s (%s), antenna SKU %s (%s)\n",
	    board[0] ? board : "(none)", boardsrc, antenna[0] ? antenna : "(none)", antsrc);
}

// the fdt properties bwfm reads. The calibration blob and the mac address are where m1n1
// finds them for linux: /arm-io/wlan wifi-calibration-msf and /chosen mac-address-wifi0
static int
pd_bwfm_prop(int node, const char *name, void *buf, int len, int quiet)
{
	char board[64], antenna[16];
	const char *str = NULL, *path = NULL, *adtname = NULL;
	int n;

	pd_bwfm_board_identity(board, sizeof(board), antenna, sizeof(antenna));
	if (node == PD_BWFM_NODE_ROOT && strcmp(name, "compatible") == 0)
		str = board;
	else if (node == PD_BWFM_NODE_WLAN && strcmp(name, "brcm,board-type") == 0)
		str = board;
	else if (node == PD_BWFM_NODE_WLAN && strcmp(name, "apple,antenna-sku") == 0)
		str = antenna;
	else if (node == PD_BWFM_NODE_WLAN && strcmp(name, "brcm,cal-blob") == 0) {
		path = "/arm-io/wlan";
		adtname = "wifi-calibration-msf";
	} else if (node == PD_BWFM_NODE_WLAN && strcmp(name, "local-mac-address") == 0) {
		path = "/chosen";
		adtname = "mac-address-wifi0";
	}

	if (str != NULL) {
		n = (int)strlen(str) + 1;
		if (buf != NULL && len > 0)
			memcpy(buf, str, MIN(n, len));
		return n;
	}
	if (path == NULL)
		return -1;
	n = pd_bwfm_adt_prop(path, adtname, buf, len);
	if (!quiet) {
		if (n <= 0)
			printf(PD_BWFM_NAME ": device tree %s %s missing, continuing without\n", path, adtname);
		else
			printf(PD_BWFM_NAME ": device tree %s %s, %d bytes\n", path, adtname, n);
	}
	return n > 0 ? n : -1;
}

int
OF_getproplen(int node, const char *name)
{
	return pd_bwfm_prop(node, name, NULL, 0, 1);
}

int
OF_getprop(int node, const char *name, void *buf, int len)
{
	return pd_bwfm_prop(node, name, buf, len, 0);
}

#ifdef PD_BWFM_DMA_BOUNCE

// bus_dma on the DART bounce pool. Everything runs under the stack lock.

struct pd_bwfm_dmamem {
	struct pd_bwfm_dmamem *next;
	caddr_t		kva;
	uint64_t	iova;
	size_t		size;
};
static struct pd_bwfm_dmamem *pd_bwfm_dmamems;
static size_t pd_bwfm_dma_bytes, pd_bwfm_dma_allocs, pd_bwfm_dma_failures;

static void *
pd_bwfm_pool_alloc(size_t size, size_t align, uint64_t *iova)
{
	void *kva = pd_dart_pool_alloc(size, align, iova);

	if (kva == NULL) {
		// say so once in a while, an exhausted pool is otherwise silent
		if (pd_bwfm_dma_failures++ % 256 == 0)
			printf(PD_BWFM_NAME ": DMA pool has no %zu bytes (%zu bytes in %zu allocations held)\n",
			    size, pd_bwfm_dma_bytes, pd_bwfm_dma_allocs);
		return NULL;
	}
	pd_bwfm_dma_bytes += size;
	pd_bwfm_dma_allocs++;
	return kva;
}

static void
pd_bwfm_pool_free(void *kva, size_t size)
{
	pd_dart_pool_free(kva, size);
	pd_bwfm_dma_bytes -= size;
	pd_bwfm_dma_allocs--;
}

static void
pd_bwfm_dma_report(const char *name)
{
	printf("%s: DMA pool: %zu bytes in %zu allocations, %zu failed requests\n", name,
	    pd_bwfm_dma_bytes, pd_bwfm_dma_allocs, pd_bwfm_dma_failures);
}

int
bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegs, bus_size_t maxsegsz,
    bus_size_t boundary, int flags, bus_dmamap_t *mapp)
{
	bus_dmamap_t map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT | M_ZERO);

	*mapp = NULL;
	if (map == NULL)
		return ENOMEM;
	map->_dm_size = size;
	map->_dm_flags = flags;
	map->_dm_slot = pd_bwfm_pool_alloc(size, 64, &map->_dm_slot_iova);
	if (map->_dm_slot == NULL) {
		free(map, M_DEVBUF, sizeof(*map));
		return ENOMEM;
	}
	*mapp = map;
	return 0;
}

void
bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	if (map == NULL)
		return;
	map->_dm_mbuf = NULL;
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

void
bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	if (map == NULL)
		return;
	if (map->_dm_slot != NULL)
		pd_bwfm_pool_free(map->_dm_slot, map->_dm_size);
	free(map, M_DEVBUF, sizeof(*map));
}

// only memory from bus_dmamem_alloc is loaded this way: rings, index and scratch buffers.
// It already lives in the pool, so the map gives its own slot back and points at it
int
bus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf, bus_size_t len, void *p, int flags)
{
	struct pd_bwfm_dmamem *mem;

	for (mem = pd_bwfm_dmamems; mem != NULL; mem = mem->next) {
		if ((caddr_t)buf >= mem->kva && (caddr_t)buf + len <= mem->kva + mem->size)
			break;
	}
	if (mem == NULL) {
		printf(PD_BWFM_NAME ": bus_dmamap_load of memory outside the DMA pool refused\n");
		return EINVAL;
	}
	if (map->_dm_slot != NULL) {
		pd_bwfm_pool_free(map->_dm_slot, map->_dm_size);
		map->_dm_slot = NULL;
	}
	map->_dm_mbuf = NULL;
	map->dm_segs[0].ds_addr = mem->iova + (uint64_t)((caddr_t)buf - mem->kva);
	map->dm_segs[0].ds_len = len;
	map->dm_nsegs = 1;
	map->dm_mapsize = len;
	return 0;
}

int
bus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m, int flags)
{
	size_t len = (size_t)M_PKTLEN(m);

	if (map == NULL || map->_dm_slot == NULL)
		return EINVAL;
	if (len == 0 || len > map->_dm_size)
		return EFBIG;
	if (map->_dm_flags & BUS_DMA_READ) {
		// a receive buffer: the chip writes the slot, the mbuf is filled at POSTREAD
		map->_dm_mbuf = m;
	} else {
		m_copydata(m, 0, (int)len, map->_dm_slot);
		map->_dm_mbuf = NULL;
	}
	map->dm_segs[0].ds_addr = map->_dm_slot_iova;
	map->dm_segs[0].ds_len = len;
	map->dm_nsegs = 1;
	map->dm_mapsize = len;
	return 0;
}

void
bus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t off, bus_size_t len, int ops)
{
	if (map == NULL)
		return;
	__sync_synchronize();
	if ((ops & BUS_DMASYNC_POSTREAD) && map->_dm_mbuf != NULL && map->_dm_slot != NULL) {
		// once: the driver hands the mbuf up right after this
		mbuf_copyback(map->_dm_mbuf, 0, map->dm_mapsize, map->_dm_slot, MBUF_DONTWAIT);
		map->_dm_mbuf = NULL;
	}
}

int
bus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t align, bus_size_t boundary,
    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{
	struct pd_bwfm_dmamem *mem = malloc(sizeof(*mem), M_DEVBUF, M_NOWAIT | M_ZERO);

	if (mem == NULL)
		return ENOMEM;
	mem->kva = pd_bwfm_pool_alloc(size, align, &mem->iova);
	if (mem->kva == NULL) {
		free(mem, M_DEVBUF, sizeof(*mem));
		return ENOMEM;
	}
	mem->size = size;
	mem->next = pd_bwfm_dmamems;
	pd_bwfm_dmamems = mem;
	segs[0].ds_addr = mem->iova;
	segs[0].ds_len = size;
	segs[0]._ds_mem = mem->kva;
	*rsegs = 1;
	return 0;
}

void
bus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	struct pd_bwfm_dmamem **prev, *mem;

	for (prev = &pd_bwfm_dmamems; (mem = *prev) != NULL; prev = &mem->next) {
		if (mem->kva == segs[0]._ds_mem) {
			*prev = mem->next;
			pd_bwfm_pool_free(mem->kva, mem->size);
			free(mem, M_DEVBUF, sizeof(*mem));
			segs[0]._ds_mem = NULL;
			return;
		}
	}
}

int
bus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags)
{
	if (segs[0]._ds_mem == NULL)
		return EINVAL;
	*kvap = segs[0]._ds_mem;
	return 0;
}

void
bus_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{
}

#else /* !PD_BWFM_DMA_BOUNCE */

// bus_dma on the DART mapper (pd_dart_map.h). Everything runs under the stack lock.
//
// bus_dmamem memory (rings, index and scratch buffers) is wired kernel memory that stays
// mapped until bus_dmamem_free. An mbuf is mapped in place: the 16 KiB pages its data lies
// in get consecutive iovas, adjacent in memory or not, from bus_dmamap_load_mbuf until
// bus_dmamap_unload, and the device address is the iova plus the offset into the first page.
// That holds for transmit, command and receive buffers alike, nothing is copied.
//
// The DART wants an invalidate before a new mapping is used. It is not done per mapping but
// in bus_dmamap_sync(PRE*) of a bus_dmamem map: bwfm_pci_ring_write_commit syncs the ring
// right before it moves the write pointer, and a ring entry is the only way the chip learns
// a buffer address. That same invalidate retires everything unloaded since the last one.
//
// A bounce copy through the map's slot, a mapped buffer of the map's maximum size that is
// allocated on first use and kept, happens only when an mbuf cannot be mapped in place: it
// is a chain, the physical address of one of its pages cannot be resolved or is not one the
// DART takes, or the mapper is out of iovas. With PD_BWFM_DMA_RX_SLOTS defined every receive
// map (BUS_DMA_READ) is bounced on purpose: the slots then stay mapped for good and the
// receive path costs a copy per packet instead of a map, an invalidate and an unmap.

#define PD_BWFM_DMA_NONE	0
#define PD_BWFM_DMA_MEM		1	/* bus_dmamem memory, mapped by its owner */
#define PD_BWFM_DMA_MBUF	2	/* an mbuf mapped in place, _dm_dva and dm_mapsize */
#define PD_BWFM_DMA_SLOT	3	/* an mbuf bounced through the slot */

#define PD_BWFM_DMA_MAXPAGES	4	/* 64 KiB, the largest buffer here is 8 KiB */

struct pd_bwfm_dmamem {
	struct pd_bwfm_dmamem *next;
	caddr_t		kva;
	uint64_t	iova;
	size_t		size;
};
static struct pd_bwfm_dmamem *pd_bwfm_dmamems;
static size_t pd_bwfm_dma_bytes, pd_bwfm_dma_allocs, pd_bwfm_dma_failures, pd_bwfm_dma_slots;
static uint64_t pd_bwfm_dma_inplace, pd_bwfm_dma_bounced;

static void
pd_bwfm_dma_report(const char *name)
{
	printf("%s: DMA map: %zu bytes in %zu allocations, %llu mbufs mapped in place, "
	    "%llu bounced, %zu bounce slots, %zu failed requests\n", name,
	    pd_bwfm_dma_bytes, pd_bwfm_dma_allocs, (unsigned long long)pd_bwfm_dma_inplace,
	    (unsigned long long)pd_bwfm_dma_bounced, pd_bwfm_dma_slots, pd_bwfm_dma_failures);
	pd_dart_report(name);
}

// the physical page behind a page of mbuf storage. The pmap answers for every wired kernel
// address, cluster or not; the mbuf KPI is asked second because with a system mapper it
// would answer with that mapper's address
static uint64_t
pd_bwfm_dma_page_pa(uintptr_t va)
{
	uint64_t pa = pd_dart_kvtophys((const void *)va);

	if (pa == 0)
		pa = (uint64_t)mbuf_data_to_physical((void *)va);
	return pa;
}

static int
pd_bwfm_dma_map_mbuf(bus_dmamap_t map, struct mbuf *m, size_t len)
{
	uintptr_t va = (uintptr_t)mbuf_data(m);
	uintptr_t first = va & ~(uintptr_t)PD_DART_MAP_PAGE_MASK;
	size_t n = (va - first + len + PD_DART_MAP_PAGE_MASK) >> PD_DART_MAP_PAGE_SHIFT;
	uint64_t pa[PD_BWFM_DMA_MAXPAGES], iova = 0;

	if (va == 0 || n > PD_BWFM_DMA_MAXPAGES)
		return EFBIG;
	for (size_t i = 0; i < n; i++) {
		pa[i] = pd_bwfm_dma_page_pa(first + i * PD_DART_MAP_PAGE_SIZE);
		if (pa[i] == 0 || (pa[i] & PD_DART_MAP_PAGE_MASK) != 0 || (pa[i] >> 36) != 0)
			return EFAULT;
	}
	if (pd_dart_map_pages(pa, (unsigned)n, &iova) != 0)
		return ENOMEM;
	map->_dm_kind = PD_BWFM_DMA_MBUF;
	map->_dm_dva = iova + (va - first);
	map->_dm_mbuf = NULL;
	map->dm_segs[0].ds_addr = map->_dm_dva;
	map->dm_segs[0].ds_len = len;
	map->dm_nsegs = 1;
	map->dm_mapsize = len;
	pd_bwfm_dma_inplace++;
	return 0;
}

static int
pd_bwfm_dma_bounce_mbuf(bus_dmamap_t map, struct mbuf *m, size_t len)
{
	if (map->_dm_slot == NULL) {
		map->_dm_slot = pd_dart_alloc(map->_dm_size, &map->_dm_slot_iova);
		if (map->_dm_slot == NULL) {
			if (pd_bwfm_dma_failures++ % 256 == 0)
				printf(PD_BWFM_NAME ": no bounce slot of %llu bytes\n",
				    (unsigned long long)map->_dm_size);
			return ENOMEM;
		}
		pd_bwfm_dma_slots++;
	}
	if (map->_dm_flags & BUS_DMA_READ) {
		// a receive buffer: the chip writes the slot, the mbuf is filled at POSTREAD
		map->_dm_mbuf = m;
	} else {
		m_copydata(m, 0, (int)len, map->_dm_slot);
		map->_dm_mbuf = NULL;
	}
	map->_dm_kind = PD_BWFM_DMA_SLOT;
	map->dm_segs[0].ds_addr = map->_dm_slot_iova;
	map->dm_segs[0].ds_len = len;
	map->dm_nsegs = 1;
	map->dm_mapsize = len;
	pd_bwfm_dma_bounced++;
	return 0;
}

int
bus_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegs, bus_size_t maxsegsz,
    bus_size_t boundary, int flags, bus_dmamap_t *mapp)
{
	bus_dmamap_t map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT | M_ZERO);

	*mapp = NULL;
	if (map == NULL)
		return ENOMEM;
	map->_dm_size = size;
	map->_dm_flags = flags;
	*mapp = map;
	return 0;
}

void
bus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	if (map == NULL)
		return;
	// the iova is not handed out again before the next invalidate, see pd_dart_map.h
	if (map->_dm_kind == PD_BWFM_DMA_MBUF)
		pd_dart_unmap(map->_dm_dva, map->dm_mapsize);
	map->_dm_kind = PD_BWFM_DMA_NONE;
	map->_dm_dva = 0;
	map->_dm_mbuf = NULL;
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

void
bus_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{
	if (map == NULL)
		return;
	// bwfm_pci_cleanup destroys the maps of packets that are still posted
	bus_dmamap_unload(t, map);
	if (map->_dm_slot != NULL) {
		pd_dart_free(map->_dm_slot);
		pd_bwfm_dma_slots--;
	}
	free(map, M_DEVBUF, sizeof(*map));
}

// only memory from bus_dmamem_alloc is loaded this way: rings, index and scratch buffers.
// It is mapped already, the map points at it
int
bus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf, bus_size_t len, void *p, int flags)
{
	struct pd_bwfm_dmamem *mem;

	for (mem = pd_bwfm_dmamems; mem != NULL; mem = mem->next) {
		if ((caddr_t)buf >= mem->kva && (caddr_t)buf + len <= mem->kva + mem->size)
			break;
	}
	if (mem == NULL) {
		printf(PD_BWFM_NAME ": bus_dmamap_load of memory that is not bus_dmamem refused\n");
		return EINVAL;
	}
	bus_dmamap_unload(t, map);
	map->_dm_kind = PD_BWFM_DMA_MEM;
	map->dm_segs[0].ds_addr = mem->iova + (uint64_t)((caddr_t)buf - mem->kva);
	map->dm_segs[0].ds_len = len;
	map->dm_nsegs = 1;
	map->dm_mapsize = len;
	return 0;
}

int
bus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *m, int flags)
{
	size_t len = (size_t)M_PKTLEN(m);
	int inplace = 1;

	if (map == NULL)
		return EINVAL;
	if (len == 0 || len > map->_dm_size)
		return EFBIG;
	bus_dmamap_unload(t, map);
	// the driver takes one segment. Transmit chains are made one mbuf in bwfm_start and the
	// buffers the driver allocates are one mbuf, so a chain here is the exception: it is
	// bounced, which linearises it the way the pool always did
	if (M_NEXT(m) != NULL || mbuf_len(m) < len)
		inplace = 0;
#ifdef PD_BWFM_DMA_RX_SLOTS
	if (map->_dm_flags & BUS_DMA_READ)
		inplace = 0;
#endif
	if (inplace && pd_bwfm_dma_map_mbuf(map, m, len) == 0)
		return 0;
	return pd_bwfm_dma_bounce_mbuf(map, m, len);
}

void
bus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t off, bus_size_t len, int ops)
{
	if (map == NULL)
		return;
	__sync_synchronize();
	// a ring or index buffer is about to be shown to the chip: whatever was mapped for it
	// since the last invalidate has to be live now. Nothing to do when nothing was mapped
	if ((ops & (BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE)) && map->_dm_kind == PD_BWFM_DMA_MEM)
		pd_dart_sync();
	if ((ops & BUS_DMASYNC_POSTREAD) && map->_dm_kind == PD_BWFM_DMA_SLOT &&
	    map->_dm_mbuf != NULL && map->_dm_slot != NULL) {
		// once: the driver hands the mbuf up right after this
		mbuf_copyback(map->_dm_mbuf, 0, map->dm_mapsize, map->_dm_slot, MBUF_DONTWAIT);
		map->_dm_mbuf = NULL;
	}
}

int
bus_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t align, bus_size_t boundary,
    bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{
	struct pd_bwfm_dmamem *mem;

	// the memory is page aligned, in the kernel and for the chip
	if (size == 0 || align > PD_DART_MAP_PAGE_SIZE)
		return EINVAL;
	mem = malloc(sizeof(*mem), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mem == NULL)
		return ENOMEM;
	mem->kva = pd_dart_alloc(size, &mem->iova);
	if (mem->kva == NULL) {
		pd_bwfm_dma_failures++;
		free(mem, M_DEVBUF, sizeof(*mem));
		return ENOMEM;
	}
	mem->size = size;
	mem->next = pd_bwfm_dmamems;
	pd_bwfm_dmamems = mem;
	pd_bwfm_dma_bytes += size;
	pd_bwfm_dma_allocs++;
	segs[0].ds_addr = mem->iova;
	segs[0].ds_len = size;
	segs[0]._ds_mem = mem->kva;
	*rsegs = 1;
	return 0;
}

void
bus_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{
	struct pd_bwfm_dmamem **prev, *mem;

	for (prev = &pd_bwfm_dmamems; (mem = *prev) != NULL; prev = &mem->next) {
		if (mem->kva == segs[0]._ds_mem) {
			*prev = mem->next;
			pd_dart_free(mem->kva);
			pd_bwfm_dma_bytes -= mem->size;
			pd_bwfm_dma_allocs--;
			free(mem, M_DEVBUF, sizeof(*mem));
			segs[0]._ds_mem = NULL;
			return;
		}
	}
}

int
bus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap, int flags)
{
	if (segs[0]._ds_mem == NULL)
		return EINVAL;
	*kvap = segs[0]._ds_mem;
	return 0;
}

void
bus_dmamem_unmap(bus_dma_tag_t t, caddr_t kva, size_t size)
{
}

#endif /* PD_BWFM_DMA_BOUNCE */

#undef printf
extern void IOSleep(unsigned milliseconds);
int
pd_bwfm_printf(const char *fmt, ...)
{
	char line[320];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	printf("%s", line);
#ifdef PD_BWFM_SLOW_LOG
	IOSleep(25);
#endif
	return n;
}
