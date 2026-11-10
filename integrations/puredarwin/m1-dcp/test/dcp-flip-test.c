// SPDX-License-Identifier: MIT
/*
 * dcp-flip-test - KMS through /dev/dri/card0 on the display coprocessor driver (PDDcp).
 *
 * Prints the connector, mode, CRTC and planes; creates two dumb buffers of the mode's size,
 * fills each with a solid colour and a nonce pattern; one atomic commit with ALLOW_MODESET
 * to the first; then page-flips between the two for --seconds (default 2), counting
 * flip-complete events and printing the rate and the shortest and longest interval.
 * Events are taken with a blocking read() on the DRM fd (select() on it is not relied on).
 * Exit status 0 only if every step worked; a failure names the call and errno.
 *
 *   dcp-flip-test [--device /dev/dri/cardN] [--seconds N] [--nonce HEX] [--no-flip]
 *                 [--prime-from auto|/dev/dri/renderDN]
 *
 * Without --device the display is the card node whose DRM driver is "apple"; --prime-from
 * auto is the render node whose driver is "asahi".
 *
 * --prime-from: the two buffers are not dumb buffers of the display driver but GEM objects
 * of the GPU driver (asahi GEM_CREATE on the render node), filled through the render node's
 * mapping, exported as PRIME fds and imported into the card node: the display then scans
 * out memory that belongs to the GPU driver, with no copy. This tests the cross-driver
 * import; the pixels are written by the CPU here, not rendered.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <asahi_drm.h>	/* the pin's include/uapi/drm, see dcp-flip-test.nix */

#define P "dcp-flip-test: "

/*
 * Every line goes to stdout and, unbuffered, to /dev/console: if the kernel panics in one of
 * the calls below, the console still shows the last step that completed (boot ft02).
 */
static int console_fd = -1;

static void out(const char *fmt, ...)
{
	char line[512];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int)sizeof(line))
		n = sizeof(line) - 1;
	fwrite(line, 1, (size_t)n, stdout);
	if (console_fd >= 0)
		(void)!write(console_fd, line, (size_t)n);
}

static int fail(const char *what)
{
	int e = errno;

	out(P "FAIL %s: errno %d (%s)\n", what, e, strerror(e));
	return 1;
}

static uint64_t now_ns(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint32_t prop_id(int fd, uint32_t obj, uint32_t type, const char *name, uint64_t *value)
{
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, obj, type);
	uint32_t id = 0;

	if (!props)
		return 0;
	for (uint32_t i = 0; i < props->count_props && !id; i++) {
		drmModePropertyRes *p = drmModeGetProperty(fd, props->props[i]);

		if (p && !strcmp(p->name, name)) {
			id = p->prop_id;
			if (value)
				*value = props->prop_values[i];
		}
		drmModeFreeProperty(p);
	}
	drmModeFreeObjectProperties(props);
	if (!id)
		out(P "object %u has no property %s\n", obj, name);
	return id;
}

/* Blocks in read() until a flip-complete event arrives. 0, or 1 after printing the failure. */
static int wait_flip(int fd, const char *what)
{
	char evbuf[256];

	for (;;) {
		ssize_t n = read(fd, evbuf, sizeof(evbuf));

		if (n < 0 && errno == EINTR)
			continue;
		if (n < (ssize_t)sizeof(struct drm_event)) {
			out(P "%s: read returned %zd: ", what, n);
			return fail("read of the DRM event");
		}
		for (ssize_t off = 0; off + (ssize_t)sizeof(struct drm_event) <= n;) {
			struct drm_event *ev = (struct drm_event *)(evbuf + off);

			if (ev->type == DRM_EVENT_FLIP_COMPLETE)
				return 0;
			if (ev->length < sizeof(*ev))
				break;
			off += ev->length;
		}
	}
}

/*
 * Nodes are found by driver name, never by number: with both drivers loaded card0 is the GPU
 * (boot cb02). Scans /dev/dri for entries starting with `prefix` whose DRM driver is `driver`
 * and returns an open fd, with the path in `path`. -1 if there is none.
 */
static int open_by_driver(const char *prefix, const char *driver, char *path, size_t path_len)
{
	DIR *d = opendir("/dev/dri");
	struct dirent *e;
	int found = -1;

	if (!d)
		return -1;
	while (found < 0 && (e = readdir(d)) != NULL) {
		drmVersion *v;
		int fd;

		if (strncmp(e->d_name, prefix, strlen(prefix)))
			continue;
		snprintf(path, path_len, "/dev/dri/%s", e->d_name);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;
		v = drmGetVersion(fd);
		out(P "%s: driver %s\n", path, v ? v->name : "(no version)");
		if (v && !strcmp(v->name, driver))
			found = fd;
		else
			close(fd);
		drmFreeVersion(v);
	}
	closedir(d);
	return found;
}

struct buf {
	uint32_t handle, pitch, fb;
	uint64_t size;
	uint32_t *map;
};

/* A GPU-driver buffer: created and mapped on the render node, imported into the card node. */
static int make_prime_buf(int fd, int render_fd, struct buf *b, uint32_t w, uint32_t h)
{
	struct drm_asahi_gem_create create = { 0 };
	struct drm_asahi_gem_mmap_offset mo = { 0 };
	int prime_fd = -1;

	b->pitch = w * 4;
	b->size = ((uint64_t)b->pitch * h + 0x3fff) & ~0x3fffull;
	create.size = b->size;
	out(P "step: asahi GEM_CREATE %" PRIu64 " bytes on the render node\n", b->size);
	if (drmIoctl(render_fd, DRM_IOCTL_ASAHI_GEM_CREATE, &create))
		return fail("DRM_IOCTL_ASAHI_GEM_CREATE");
	mo.handle = create.handle;
	if (drmIoctl(render_fd, DRM_IOCTL_ASAHI_GEM_MMAP_OFFSET, &mo))
		return fail("DRM_IOCTL_ASAHI_GEM_MMAP_OFFSET");
	out(P "step: mmap of the GPU buffer through the render node\n");
	b->map = mmap(NULL, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, render_fd, (off_t)mo.offset);
	if (b->map == MAP_FAILED)
		return fail("mmap of the GPU buffer");
	out(P "step: PRIME export from the render node (handle %u)\n", create.handle);
	if (drmPrimeHandleToFD(render_fd, create.handle, DRM_CLOEXEC | DRM_RDWR, &prime_fd))
		return fail("drmPrimeHandleToFD (render node)");
	out(P "step: PRIME import into the card node (fd %d)\n", prime_fd);
	if (drmPrimeFDToHandle(fd, prime_fd, &b->handle))
		return fail("drmPrimeFDToHandle (card node)");
	close(prime_fd);
	out(P "GPU buffer: render handle %u -> card handle %u\n", create.handle, b->handle);
	return 0;
}

static int make_buf(int fd, int render_fd, struct buf *b, uint32_t w, uint32_t h, uint32_t colour, uint32_t nonce)
{
	struct drm_mode_create_dumb create = { .width = w, .height = h, .bpp = 32 };
	struct drm_mode_map_dumb map = { 0 };
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };

	if (render_fd >= 0) {
		if (make_prime_buf(fd, render_fd, b, w, h))
			return 1;
		goto fill;
	}
	out(P "step: CREATE_DUMB %ux%u\n", w, h);
	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create))
		return fail("DRM_IOCTL_MODE_CREATE_DUMB");
	b->handle = create.handle;
	b->pitch = create.pitch;
	b->size = create.size;
	map.handle = b->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map))
		return fail("DRM_IOCTL_MODE_MAP_DUMB");
	out(P "step: mmap %" PRIu64 " bytes at offset 0x%" PRIx64 "\n", b->size, (uint64_t)map.offset);
	b->map = mmap(NULL, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.offset);
	if (b->map == MAP_FAILED)
		return fail("mmap of the dumb buffer");
fill:
	for (uint32_t y = 0; y < h; y++) {
		uint32_t *row = (uint32_t *)((uint8_t *)b->map + (size_t)y * b->pitch);

		for (uint32_t x = 0; x < w; x++)
			row[x] = colour;
	}
	/* the nonce as 32 squares of 32x32 pixels along the top: white for 1, black for 0 */
	for (uint32_t bit = 0; bit < 32 && (bit + 1) * 32 <= w && h >= 64; bit++)
		for (uint32_t y = 16; y < 48; y++) {
			uint32_t *row = (uint32_t *)((uint8_t *)b->map + (size_t)y * b->pitch);

			for (uint32_t x = bit * 32; x < bit * 32 + 32; x++)
				row[x] = (nonce >> (31 - bit)) & 1 ? 0x00ffffff : 0x00000000;
		}
	handles[0] = b->handle;
	pitches[0] = b->pitch;
	out(P "step: ADDFB2\n");
	if (drmModeAddFB2(fd, w, h, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &b->fb, 0))
		return fail("drmModeAddFB2 (XRGB8888)");
	out(P "buffer handle %u pitch %u size %" PRIu64 " fb %u colour 0x%06x\n", b->handle, b->pitch,
	       b->size, b->fb, colour);
	return 0;
}

int main(int argc, char **argv)
{
	const char *device = NULL;	/* default: the card node whose driver is "apple" */
	char found_path[64] = "";
	double seconds = 2.0;
	uint32_t nonce = 0x5d0c0f11;
	int do_flip = 1;
	const char *prime_from = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--device") && i + 1 < argc)
			device = argv[++i];
		else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
			seconds = atof(argv[++i]);
		else if (!strcmp(argv[i], "--nonce") && i + 1 < argc)
			nonce = (uint32_t)strtoul(argv[++i], NULL, 16);
		else if (!strcmp(argv[i], "--prime-from") && i + 1 < argc)
			prime_from = argv[++i];
		else if (!strcmp(argv[i], "--no-flip"))
			do_flip = 0;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	console_fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	out(P "start: device %s, %.1f s, nonce %08x\n", device ? device : "(by driver name)", seconds, nonce);

	int fd;
	if (device) {
		fd = open(device, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			return fail(device);
	} else {
		fd = open_by_driver("card", "apple", found_path, sizeof(found_path));
		if (fd < 0) {
			out(P "FAIL no /dev/dri/card* with DRM driver \"apple\" (the display driver)\n");
			return 1;
		}
		device = found_path;
	}
	out(P "display node %s\n", device);
	drmVersion *v = drmGetVersion(fd);
	if (v) {
		out(P "driver %s %d.%d.%d (%s)\n", v->name, v->version_major, v->version_minor,
		       v->version_patchlevel, v->desc);
		drmFreeVersion(v);
	}
	if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1))
		return fail("DRM_CLIENT_CAP_UNIVERSAL_PLANES");
	if (drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1))
		return fail("DRM_CLIENT_CAP_ATOMIC");

	out(P "step: drmModeGetResources\n");
	drmModeRes *res = drmModeGetResources(fd);
	if (!res)
		return fail("drmModeGetResources");
	out(P "resources: %d crtcs, %d connectors, %d encoders\n", res->count_crtcs, res->count_connectors,
	       res->count_encoders);

	drmModeConnector *conn = NULL;
	for (int i = 0; i < res->count_connectors && !conn; i++) {
		drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);

		if (c)
			out(P "connector %u type %u: %s, %d modes, %ux%u mm\n", c->connector_id,
			       c->connector_type, c->connection == DRM_MODE_CONNECTED ? "connected" : "not connected",
			       c->count_modes, c->mmWidth, c->mmHeight);
		if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0)
			conn = c;
		else
			drmModeFreeConnector(c);
	}
	if (!conn) {
		out(P "FAIL no connected connector with a mode\n");
		return 1;
	}
	drmModeModeInfo mode = conn->modes[0];
	for (int i = 0; i < conn->count_modes; i++)
		if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED)
			mode = conn->modes[i];
	out(P "mode %s %ux%u @ %u Hz (clock %u kHz)\n", mode.name, mode.hdisplay, mode.vdisplay,
	       mode.vrefresh, mode.clock);

	/* the CRTC: the first one an encoder of this connector can drive */
	uint32_t crtc_id = 0;
	int crtc_index = -1;
	for (int e = 0; e < conn->count_encoders && !crtc_id; e++) {
		drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[e]);

		for (int c = 0; enc && c < res->count_crtcs && !crtc_id; c++)
			if (enc->possible_crtcs & (1u << c)) {
				crtc_id = res->crtcs[c];
				crtc_index = c;
			}
		drmModeFreeEncoder(enc);
	}
	if (!crtc_id) {
		out(P "FAIL no CRTC for connector %u\n", conn->connector_id);
		return 1;
	}
	out(P "crtc %u (index %d)\n", crtc_id, crtc_index);

	out(P "step: drmModeGetPlaneResources\n");
	drmModePlaneRes *planes = drmModeGetPlaneResources(fd);
	if (!planes)
		return fail("drmModeGetPlaneResources");
	uint32_t plane_id = 0;
	for (uint32_t i = 0; i < planes->count_planes; i++) {
		drmModePlane *pl = drmModeGetPlane(fd, planes->planes[i]);
		uint64_t type = 99;

		if (!pl)
			continue;
		prop_id(fd, pl->plane_id, DRM_MODE_OBJECT_PLANE, "type", &type);
		out(P "plane %u type %" PRIu64 " possible_crtcs 0x%x, %u formats, first %.4s\n", pl->plane_id, type,
		       pl->possible_crtcs, pl->count_formats, pl->count_formats ? (const char *)&pl->formats[0] : "none");
		if (!plane_id && type == DRM_PLANE_TYPE_PRIMARY && (pl->possible_crtcs & (1u << crtc_index)))
			plane_id = pl->plane_id;
		drmModeFreePlane(pl);
	}
	if (!plane_id) {
		out(P "FAIL no primary plane for crtc %u\n", crtc_id);
		return 1;
	}
	out(P "using plane %u\n", plane_id);

	uint32_t p_conn_crtc = prop_id(fd, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", NULL);
	uint32_t p_mode = prop_id(fd, crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", NULL);
	uint32_t p_active = prop_id(fd, crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", NULL);
	static const char *const pn[] = { "FB_ID", "CRTC_ID", "SRC_X", "SRC_Y", "SRC_W", "SRC_H",
					  "CRTC_X", "CRTC_Y", "CRTC_W", "CRTC_H" };
	uint32_t pp[10];
	int missing = !p_conn_crtc || !p_mode || !p_active;
	for (int i = 0; i < 10; i++) {
		pp[i] = prop_id(fd, plane_id, DRM_MODE_OBJECT_PLANE, pn[i], NULL);
		missing |= !pp[i];
	}
	if (missing) {
		out(P "FAIL a required property is missing\n");
		return 1;
	}

	struct buf bufs[2] = { { 0 }, { 0 } };
	int render_fd = -1;
	if (prime_from) {
		char rpath[64] = "";

		if (!strcmp(prime_from, "auto")) {
			render_fd = open_by_driver("renderD", "asahi", rpath, sizeof(rpath));
			if (render_fd < 0) {
				out(P "FAIL no /dev/dri/renderD* with DRM driver \"asahi\" (the GPU driver)\n");
				return 1;
			}
			prime_from = rpath;
		} else {
			render_fd = open(prime_from, O_RDWR | O_CLOEXEC);
			if (render_fd < 0)
				return fail(prime_from);
		}
		out(P "buffers come from the GPU driver on %s\n", prime_from);
	}
	if (make_buf(fd, render_fd, &bufs[0], mode.hdisplay, mode.vdisplay, 0x00203080, nonce) ||
	    make_buf(fd, render_fd, &bufs[1], mode.hdisplay, mode.vdisplay, 0x00804020, ~nonce))
		return 1;

	uint32_t mode_blob = 0;
	if (drmModeCreatePropertyBlob(fd, &mode, sizeof(mode), &mode_blob))
		return fail("drmModeCreatePropertyBlob (mode)");

	drmModeAtomicReq *req = drmModeAtomicAlloc();
	uint64_t pv[10] = { bufs[0].fb, crtc_id, 0, 0, (uint64_t)mode.hdisplay << 16, (uint64_t)mode.vdisplay << 16,
			    0, 0, mode.hdisplay, mode.vdisplay };
	drmModeAtomicAddProperty(req, conn->connector_id, p_conn_crtc, crtc_id);
	drmModeAtomicAddProperty(req, crtc_id, p_mode, mode_blob);
	drmModeAtomicAddProperty(req, crtc_id, p_active, 1);
	for (int i = 0; i < 10; i++)
		drmModeAtomicAddProperty(req, plane_id, pp[i], pv[i]);
	out(P "step: atomic commit with ALLOW_MODESET\n");
	uint64_t t_commit = now_ns();
	if (drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_PAGE_FLIP_EVENT, NULL))
		return fail("drmModeAtomicCommit (ALLOW_MODESET | PAGE_FLIP_EVENT, first frame)");
	drmModeAtomicFree(req);
	uint64_t t_returned = now_ns();
	out(P "first frame committed: fb %u, nonce %08x; the blocking commit took %.3f ms\n", bufs[0].fb, nonce,
	    (t_returned - t_commit) / 1e6);
	/* a client must not flip again before the previous commit's event: wait for it */
	if (wait_flip(fd, "first frame"))
		return 1;
	out(P "first frame's flip-complete event arrived %.3f ms after the commit returned\n",
	    (now_ns() - t_returned) / 1e6);

	if (!do_flip) {
		sleep((unsigned)seconds);
		out(P "PASS (no flips requested)\n");
		return 0;
	}

	out(P "step: flipping\n");
	uint64_t start = now_ns(), last = start, min_iv = UINT64_MAX, max_iv = 0, end = start + (uint64_t)(seconds * 1e9);
	unsigned flips = 0, next = 1;

	unsigned busy_retries = 0;

	while (now_ns() < end) {
		int tries = 0, ret;

		/*
		 * EBUSY means the previous commit on the CRTC is not finished although its event
		 * has been read; counted, retried at most 100 times 1 ms apart, and reported.
		 */
		for (;;) {
			req = drmModeAtomicAlloc();
			drmModeAtomicAddProperty(req, plane_id, pp[0], bufs[next].fb);
			ret = drmModeAtomicCommit(fd, req, DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK,
						  (void *)(uintptr_t)(flips + 1));
			drmModeAtomicFree(req);
			if (!ret || errno != EBUSY || ++tries > 100)
				break;
			busy_retries++;
			usleep(1000);
		}
		if (ret) {
			out(P "flip %u (after %d EBUSY retries): ", flips + 1, tries);
			return fail("drmModeAtomicCommit (PAGE_FLIP_EVENT | NONBLOCK)");
		}
		if (wait_flip(fd, "flip"))
			return 1;
		uint64_t t = now_ns(), iv = t - last;

		if (flips > 0) {
			if (iv < min_iv)
				min_iv = iv;
			if (iv > max_iv)
				max_iv = iv;
		}
		last = t;
		flips++;
		next ^= 1;
	}
	double elapsed = (double)(last - start) / 1e9;

	out(P "%u flips in %.3f s: %.2f per second, interval min %.3f ms max %.3f ms, %u EBUSY retries\n", flips,
	       elapsed, elapsed > 0 ? flips / elapsed : 0.0, flips > 1 ? min_iv / 1e6 : 0.0,
	       flips > 1 ? max_iv / 1e6 : 0.0, busy_retries);
	if (flips < 2) {
		out(P "FAIL fewer than two flips completed\n");
		return 1;
	}
	out(P "PASS\n");
	return 0;
}
