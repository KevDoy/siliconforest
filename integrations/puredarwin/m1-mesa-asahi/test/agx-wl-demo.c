// SPDX-License-Identifier: MIT
// agx-wl-demo: a GPU-rendered Wayland client.  xdg-shell toplevel, EGL window surface on a
// wl_egl_window, GLES3, the three scenes of agx-panel-demo in an 800x600 window, one frame per
// wl_surface.frame callback, for --seconds (default 10) split over the scenes.
//
// It must be a hardware client: it fails if the renderer is a software one, if the compositor
// has no zwp_linux_dmabuf_v1, or if the Wayland traffic shows no dma-buf buffer or any wl_shm
// pool.  The traffic is libwayland's own WAYLAND_DEBUG log of this process, captured in a
// temporary file and counted at the end (create_immed requests, wl_shm.create_pool requests,
// attaches, buffer releases, frame callbacks).
//
// Per scene, before the animation: the fixed frame is drawn into the window's back buffer and
// into an ordinary GL framebuffer object and both are read back (AGXWLCHECK: equal or not, and
// the hash, which is comparable with the same program on Linux on the same GPU; a handful of
// pixels may differ because the window's framebuffer is y-inverted inside the driver).
//
// Output: AGXWL_INFO lines, AGXWLCHECK and AGXWL per scene, AGXWL_BUFFERS, AGXWL_DONE.
// Exit status 0 only for status=ok.
//
// The scenes, shaders and helpers are agx-panel-demo.c's, included as source (its main() is
// renamed and not called), so both programs draw the same pictures.
//
// usage: agx-wl-demo [--seconds N] [--size WxH] [--scenes plasma,quads,mesh]
//                    [--require-renderer substr] [--allow-software]

#define main agx_panel_demo_main
#include "agx-panel-demo.c"
#undef main
#undef T
#define T "agx-wl-demo: "

#include <poll.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell-client-protocol.h"

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

static struct {
	struct wl_compositor *compositor;
	struct xdg_wm_base *wm_base;
	uint32_t dmabuf_version, shm, presentation, compositor_version;
	int configured, closed, frame_done, new_w, new_h;
} wl;

static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial) { (void)d; xdg_wm_base_pong(b, serial); }
static const struct xdg_wm_base_listener wm_listener = { wm_ping };

static void reg_global(void *d, struct wl_registry *r, uint32_t name, const char *iface, uint32_t version)
{
	(void)d;
	if (!strcmp(iface, "wl_compositor")) {
		wl.compositor_version = version < 4 ? version : 4;
		wl.compositor = wl_registry_bind(r, name, &wl_compositor_interface, wl.compositor_version);
	} else if (!strcmp(iface, "xdg_wm_base")) {
		wl.wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wl.wm_base, &wm_listener, NULL);
	} else if (!strcmp(iface, "zwp_linux_dmabuf_v1")) {
		wl.dmabuf_version = version;	// Mesa binds it itself; this is only what is on offer
	} else if (!strcmp(iface, "wl_shm")) {
		wl.shm = version;
	} else if (!strcmp(iface, "wp_presentation")) {
		wl.presentation = version;
	}
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t name) { (void)d; (void)r; (void)name; }
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static void xs_configure(void *d, struct xdg_surface *s, uint32_t serial)
{
	(void)d;
	xdg_surface_ack_configure(s, serial);
	wl.configured = 1;
}
static const struct xdg_surface_listener xs_listener = { xs_configure };

static void top_configure(void *d, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states)
{
	(void)d; (void)t; (void)states;
	wl.new_w = w;
	wl.new_h = h;
}
static void top_close(void *d, struct xdg_toplevel *t) { (void)d; (void)t; wl.closed = 1; }
static const struct xdg_toplevel_listener top_listener = { .configure = top_configure, .close = top_close };

static void frame_done(void *d, struct wl_callback *cb, uint32_t time)
{
	(void)d; (void)time;
	wl_callback_destroy(cb);
	wl.frame_done = 1;
}
static const struct wl_callback_listener frame_listener = { frame_done };

// Dispatch until *flag is set; -1 on a connection error, 0 on timeout, 1 when set.
static int wait_for(struct wl_display *dpy, int *flag, int timeout_ms)
{
	uint64_t deadline = now_us() + (uint64_t)timeout_ms * 1000;
	while (!*flag) {
		while (wl_display_prepare_read(dpy) != 0)
			if (wl_display_dispatch_pending(dpy) < 0)
				return -1;
		if (*flag) {
			wl_display_cancel_read(dpy);
			break;
		}
		if (wl_display_flush(dpy) < 0 && errno != EAGAIN) {
			wl_display_cancel_read(dpy);
			return -1;
		}
		uint64_t now = now_us();
		struct pollfd pfd = { .fd = wl_display_get_fd(dpy), .events = POLLIN };
		int r = now >= deadline ? 0 : poll(&pfd, 1, (int)((deadline - now + 999) / 1000));
		if (r <= 0) {
			wl_display_cancel_read(dpy);
			if (r < 0 && errno == EINTR)
				continue;
			return r < 0 ? -1 : 0;
		}
		if (wl_display_read_events(dpy) < 0 || wl_display_dispatch_pending(dpy) < 0)
			return -1;
	}
	return 1;
}

static size_t count_in(const char *log, const char *needle)
{
	size_t n = 0;
	for (const char *p = log; (p = strstr(p, needle)) != NULL; p += strlen(needle))
		n++;
	return n;
}

int main(int argc, char **argv)
{
	const char *require = NULL, *scenes = "plasma,quads,mesh";
	double seconds = 10.0;
	int allow_software = 0, failed = 0;
	char logname[] = "/tmp/agx-wl-demo.XXXXXX";

	W = 800;
	H = 600;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
		else if (!strcmp(argv[i], "--size") && i + 1 < argc && sscanf(argv[i + 1], "%dx%d", &W, &H) == 2) i++;
		else if (!strcmp(argv[i], "--scenes") && i + 1 < argc) scenes = argv[++i];
		else if (!strcmp(argv[i], "--require-renderer") && i + 1 < argc) require = argv[++i];
		else if (!strcmp(argv[i], "--allow-software")) allow_software = 1;
		else {
			printf("usage: %s [--seconds N] [--size WxH] [--scenes plasma,quads,mesh] [--require-renderer substr] [--allow-software]\n", argv[0]);
			return 2;
		}
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	// libwayland's protocol log of this process goes to stderr: send stderr to a file
	int logfd = mkstemp(logname), saved_stderr = dup(2);
	if (logfd >= 0) {
		setenv("WAYLAND_DEBUG", "1", 1);
		fflush(stderr);
		dup2(logfd, 2);
	} else {
		printf(T "no temporary file for the protocol log (%s); buffer types cannot be counted\n", strerror(errno));
	}

	struct wl_display *dpy = wl_display_connect(NULL);
	if (!dpy) {
		printf("AGXWL_FAIL call=\"wl_display_connect\" errno=%d (%s) WAYLAND_DISPLAY=%s XDG_RUNTIME_DIR=%s\n", errno, strerror(errno),
		    getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(unset)", getenv("XDG_RUNTIME_DIR") ? getenv("XDG_RUNTIME_DIR") : "(unset)");
		return 2;
	}
	struct wl_registry *registry = wl_display_get_registry(dpy);
	wl_registry_add_listener(registry, &reg_listener, NULL);
	wl_display_roundtrip(dpy);
	printf("AGXWL_INFO globals wl_compositor=%u xdg_wm_base=%d zwp_linux_dmabuf_v1=%u wl_shm=%u wp_presentation=%u\n", wl.compositor_version,
	    wl.wm_base != NULL, wl.dmabuf_version, wl.shm, wl.presentation);
	if (!wl.compositor || !wl.wm_base) {
		printf("AGXWL_FAIL call=\"bind wl_compositor and xdg_wm_base\" errno=0 (not advertised)\n");
		return 2;
	}
	if (!wl.dmabuf_version && !allow_software) {
		printf("AGXWL_FAIL call=\"zwp_linux_dmabuf_v1\" errno=0 (the compositor does not advertise it: a GPU client cannot hand over buffers)\n");
		return 1;
	}

	PFNEGLGETPLATFORMDISPLAYEXTPROC get_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
	EGLDisplay edpy = get_display ? get_display(EGL_PLATFORM_WAYLAND_KHR, dpy, NULL) : EGL_NO_DISPLAY;
	EGLint major, minor, ncfg = 0;
	if (edpy == EGL_NO_DISPLAY || !eglInitialize(edpy, &major, &minor)) {
		printf("AGXWL_FAIL call=\"eglInitialize (Wayland platform)\" errno=0 (EGL error 0x%x)\n", eglGetError());
		return 2;
	}
	eglBindAPI(EGL_OPENGL_ES_API);
	EGLConfig cfgs[64], cfg = NULL;
	static const EGLint want[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };
	eglChooseConfig(edpy, want, cfgs, 64, &ncfg);
	EGLint alpha = -1, visual = 0;
	for (int i = 0; i < ncfg && !cfg; i++) {	// an opaque 8-bit config if there is one
		EGLint r = 0, a = 0;
		eglGetConfigAttrib(edpy, cfgs[i], EGL_RED_SIZE, &r);
		eglGetConfigAttrib(edpy, cfgs[i], EGL_ALPHA_SIZE, &a);
		if (r == 8 && a == 0)
			cfg = cfgs[i];
	}
	for (int i = 0; i < ncfg && !cfg; i++) {
		EGLint r = 0;
		eglGetConfigAttrib(edpy, cfgs[i], EGL_RED_SIZE, &r);
		if (r == 8)
			cfg = cfgs[i];
	}
	if (!cfg) {
		printf("AGXWL_FAIL call=\"eglChooseConfig\" errno=0 (no 8-bit window config with depth; %d candidates)\n", ncfg);
		return 2;
	}
	eglGetConfigAttrib(edpy, cfg, EGL_ALPHA_SIZE, &alpha);
	eglGetConfigAttrib(edpy, cfg, EGL_NATIVE_VISUAL_ID, &visual);
	static const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	EGLContext ctx = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, ctx_attribs);

	struct wl_surface *surface = wl_compositor_create_surface(wl.compositor);
	struct xdg_surface *xs = xdg_wm_base_get_xdg_surface(wl.wm_base, surface);
	struct xdg_toplevel *top = xdg_surface_get_toplevel(xs);
	xdg_surface_add_listener(xs, &xs_listener, NULL);
	xdg_toplevel_add_listener(top, &top_listener, NULL);
	xdg_toplevel_set_title(top, "agx-wl-demo");
	xdg_toplevel_set_app_id(top, "agx-wl-demo");
	wl_surface_commit(surface);
	if (wait_for(dpy, &wl.configured, 5000) != 1) {
		printf("AGXWL_FAIL call=\"first xdg_surface.configure\" errno=0 (none within 5 s)\n");
		return 1;
	}
	if (wl.new_w > 0 && wl.new_h > 0) {	// a tiling compositor decides the size
		W = wl.new_w;
		H = wl.new_h;
	}
	struct wl_egl_window *win = wl_egl_window_create(surface, W, H);
	EGLSurface esurf = win ? eglCreateWindowSurface(edpy, cfg, (EGLNativeWindowType)win, NULL) : EGL_NO_SURFACE;
	if (ctx == EGL_NO_CONTEXT || esurf == EGL_NO_SURFACE || !eglMakeCurrent(edpy, esurf, esurf, ctx)) {
		printf("AGXWL_FAIL call=\"eglCreateWindowSurface / eglMakeCurrent\" errno=0 (EGL error 0x%x)\n", eglGetError());
		return 2;
	}
	eglSwapInterval(edpy, 0);	// pacing is this program's own frame callback, not Mesa's
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	int software = !renderer || strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe") || strstr(renderer, "swrast");
	const char *ext = eglQueryString(edpy, EGL_EXTENSIONS);
	printf("AGXWL_INFO GL_RENDERER=\"%s\" GL_VERSION=\"%s\" size=%dx%d config_alpha=%d native_visual=%.4s software=%d egl_dma_buf_import=%d\n",
	    renderer, (const char *)glGetString(GL_VERSION), W, H, alpha, (const char *)&visual, software,
	    ext && strstr(ext, "EGL_EXT_image_dma_buf_import") != NULL);
	if ((software && !allow_software) || (require && (!renderer || !strstr(renderer, require)))) {
		printf("AGXWL_FAIL call=\"hardware renderer\" errno=0 (renderer is \"%s\")\n", renderer);
		failed = 1;
	}

	GLuint depth;
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
	fbo8 = make_fbo(GL_RGBA8, depth);
	glDisable(GL_DITHER);
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	setup_plasma();
	setup_quads();
	setup_mesh();
	uint8_t *a = malloc((size_t)W * H * 4), *b = malloc((size_t)W * H * 4);
	enum { MAXF = 8192 };
	static uint64_t render_us[MAXF], iv_us[MAXF];
	int nscenes = 0;
	for (int s = 0; s < NSCENES; s++)
		nscenes += strstr(scenes, scene_names[s]) != NULL;

	for (int scene = 0; scene < NSCENES && !stop_requested && !wl.closed && !failed && nscenes; scene++) {
		if (!strstr(scenes, scene_names[scene]))
			continue;
		// fixed frame: window back buffer against an ordinary framebuffer object
		int bbox[4], first[2];
		char hex[65];
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		draw_scene_ex(scene, HASH_FRAME, 0, 0);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, a);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo8);
		draw_scene_ex(scene, HASH_FRAME, 0, 0);
		glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, b);
		force_alpha(a);
		force_alpha(b);
		// The window's framebuffer is y-inverted inside the driver, so a rasteriser may round a
		// few interpolated values differently: equal apart from at most 0.1 % of the pixels
		// off by more than 8/255 counts as the same picture.
		size_t d = diff_blocks(a, b, bbox, first), flat = 1, differ = 0, over = 0;
		unsigned max_diff = 0;
		for (size_t i = 0; i < (size_t)W * H * 4; i++) {
			unsigned x = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
			if (x > max_diff) max_diff = x;
		}
		for (size_t i = 0; i < (size_t)W * H; i++) {
			unsigned worst = 0;
			for (int ch = 0; ch < 3; ch++) {
				unsigned x = a[4 * i + ch] > b[4 * i + ch] ? a[4 * i + ch] - b[4 * i + ch] : b[4 * i + ch] - a[4 * i + ch];
				if (x > worst) worst = x;
			}
			differ += worst > 0;
			over += worst > 8;
		}
		for (size_t i = 1; i < (size_t)W * H && flat; i++)
			flat = memcmp(a, a + 4 * i, 4) == 0;
		int bad = flat || over * 1000 > (size_t)W * H;
		sha256_hex(a, (size_t)W * H * 4, hex);
		printf("AGXWLCHECK scene=%s frame=%d size=%dx%d window_vs_fbo=%s differ_px=%zu over8_px=%zu max_diff=%u blocks_differ=%zu flat=%d sha256=%s result=%s\n",
		    scene_names[scene], HASH_FRAME, W, H, d ? "different" : "equal", differ, over, max_diff, d, (int)flat, hex, bad ? "FAIL" : "ok");
		failed |= bad;

		uint64_t begin = now_us(), end = begin + (uint64_t)(seconds / nscenes * 1e6), last = 0;
		int n = 0, niv = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		for (int frame = 0; now_us() < end && !stop_requested && !wl.closed; frame++) {
			uint64_t t0 = now_us();
			draw_scene_ex(scene, frame, 0, 0);
			glFinish();
			if (n < MAXF)
				render_us[n++] = now_us() - t0;
			wl.frame_done = 0;
			wl_callback_add_listener(wl_surface_frame(surface), &frame_listener, NULL);
			if (!eglSwapBuffers(edpy, esurf)) {
				printf("AGXWL_FAIL call=\"eglSwapBuffers\" errno=0 (EGL error 0x%x)\n", eglGetError());
				failed = 1;
				break;
			}
			int r = wait_for(dpy, &wl.frame_done, 2000);
			if (r != 1) {
				printf("AGXWL_FAIL call=\"wl_surface.frame callback\" errno=%d (%s)\n", r < 0 ? errno : 0,
				    r < 0 ? "connection error" : "none within 2 s of the commit");
				failed = 1;
				break;
			}
			uint64_t t = now_us();
			if (last && niv < MAXF)
				iv_us[niv++] = t - last;
			last = t;
		}
		double elapsed = (double)(now_us() - begin) / 1e6;
		uint64_t iv_max = 0;
		for (int i = 0; i < niv; i++)
			if (iv_us[i] > iv_max) iv_max = iv_us[i];
		double r_med = ms_at(render_us, n, 0.5), r_p99 = ms_at(render_us, n, 0.99);
		printf("AGXWL scene=%s frames=%d seconds=%.2f fps=%.1f render_ms_median=%.2f render_ms_p99=%.2f frame_interval_ms_median=%.2f frame_interval_ms_max=%.2f\n",
		    scene_names[scene], n, elapsed, elapsed > 0 ? n / elapsed : 0.0, r_med, r_p99, ms_at(iv_us, niv, 0.5), iv_max / 1000.0);
	}
	GLenum err = glGetError();
	eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(edpy, esurf);
	wl_egl_window_destroy(win);
	xdg_toplevel_destroy(top);
	xdg_surface_destroy(xs);
	wl_surface_destroy(surface);
	eglTerminate(edpy);
	wl_display_roundtrip(dpy);
	wl_display_disconnect(dpy);

	// what actually went over the wire
	size_t dmabuf = 0, shm_pools = 0, attaches = 0, releases = 0, callbacks = 0;
	int counted = 0;
	if (logfd >= 0) {
		fflush(stderr);
		dup2(saved_stderr, 2);
		off_t len = lseek(logfd, 0, SEEK_END);
		char *log = len > 0 ? calloc(1, (size_t)len + 1) : NULL;
		if (log && pread(logfd, log, (size_t)len, 0) == len) {
			dmabuf = count_in(log, ".create_immed(");
			shm_pools = count_in(log, ".create_pool(");
			attaches = count_in(log, ".attach(");
			releases = count_in(log, ".release()");
			callbacks = count_in(log, ".done(");
			counted = 1;
		}
		free(log);
		close(logfd);
		unlink(logname);
	}
	printf("AGXWL_BUFFERS counted=%d dmabuf_create_immed=%zu shm_create_pool=%zu surface_attach=%zu buffer_release=%zu callback_done=%zu\n", counted,
	    dmabuf, shm_pools, attaches, releases, callbacks);
	if (!allow_software && counted && (dmabuf == 0 || shm_pools > 0)) {
		printf("AGXWL_FAIL call=\"dma-buf buffers\" errno=0 (%s)\n", shm_pools ? "wl_shm pools were created: shm fallback" : "no dma-buf wl_buffer was created");
		failed = 1;
	}
	printf("AGXWL_DONE status=%s gl_error=0x%x\n", stop_requested ? "interrupted" : wl.closed ? "closed" : failed ? "failed" : err ? "gl-error" : "ok", err);
	return failed || err != GL_NO_ERROR || stop_requested || wl.closed;
}
