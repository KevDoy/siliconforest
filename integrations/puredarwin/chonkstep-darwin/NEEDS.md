# Chonkstep compositor: what it offers GPU clients, and what it needs

Written by the compositor side (`sources/chonkstep-darwin`, local branch `wm-darwin`).
Read by the Mesa side (Wayland EGL platform on the Darwin Mesa build, GLES Wayland client).

Status on 10 November: **nothing in section 1 is implemented yet.** This is the surface the
compositor will implement next, so both sides can build to it. The line "Implemented in
commit ..." will be added here when it exists; until then a client finds no
`zwp_linux_dmabuf_v1` global and must fall back to `wl_shm`.

## 1. Server protocol surface for dma-buf clients (planned)

Global: `zwp_linux_dmabuf_v1`, **version 3** (no feedback objects; a client that binds 4 or 5
will find the global advertised at 3).

On bind the server sends, for each supported pair, `format` (for version < 3 clients) and
`modifier(format, modifier_hi, modifier_lo)`:

| DRM format | Modifier |
|---|---|
| `XRGB8888` (`XR24`) | `DRM_FORMAT_MOD_LINEAR` (0) |
| `ARGB8888` (`AR24`) | `DRM_FORMAT_MOD_LINEAR` (0) |

More modifiers (the asahi ones the GL import lists besides linear, `0x0c00000000000002` and
`0x0c00000000000001` on boot km01) will be added only after the compositor has imported such a
buffer into its own GL context on the M1. Do not rely on them; allocate **linear**.

Requests the server handles:

- `zwp_linux_dmabuf_v1.create_params`, `.destroy`
- `zwp_linux_buffer_params_v1.add(fd, plane_idx, offset, stride, modifier_hi, modifier_lo)`:
  one plane only (`plane_idx` 0). A second plane is a protocol error (`plane_idx`).
- `zwp_linux_buffer_params_v1.create_immed(buffer_id, width, height, format, flags)` and
  `.create(width, height, format, flags)` (answered with `created` or `failed`).
  `flags` must be 0: `y_invert`, `interlaced` and `bottom_first` are refused
  (`create_immed`: protocol error `invalid_wl_buffer`; `create`: `failed`).
- `zwp_linux_buffer_params_v1.destroy`

What a client buffer must be:

- One plane, `XRGB8888` or `ARGB8888`, linear, row 0 = top row, `stride >= width * 4`,
  `offset + stride * height <=` the dma-buf's size.
- `ARGB8888` pixels are **premultiplied** alpha (as on every Wayland compositor).
- Importable by `eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT)` in the compositor's context
  (Mesa asahi, surfaceless EGL, GLES 3.2) and bindable with `glEGLImageTargetTexture2DOES`
  to `GL_TEXTURE_2D`. A buffer allocated on `/dev/dri/renderD128` (driver `asahi`) with
  `DRM_IOCTL_ASAHI_GEM_CREATE` and exported with `drmPrimeHandleToFD` is the tested shape
  (it is what the compositor's own scanout buffers are).

Surface semantics, the same as for `wl_shm` buffers on this compositor today:

- `wl_surface.attach` + `damage`/`damage_buffer` + `commit`. Buffer scale 1 only, no
  transform, attach offset 0,0 (anything else is a protocol error, as for shm).
- The buffer is **not copied**. The compositor samples the client's memory when it renders,
  so the client must not draw into a buffer again until it receives `wl_buffer.release`.
  `release` is sent when the surface has a newer buffer committed and the frame showing the
  newer one has been handed to the display. A client therefore needs at least two buffers,
  three to never stall.
- Rendering must be complete when the client commits: the compositor does not wait on a
  fence of the client's. `glFinish()` before `commit` is correct. (Implicit sync through the
  dma-buf sync-file ioctls can be agreed later; write the request in section 3.)
- `wl_surface.frame` callbacks fire on the display's flip-complete event (60 Hz), one per
  presented frame. Use them to pace rendering.
- `xdg_wm_base` / `xdg_toplevel` as for shm clients; window size comes from the first
  buffer. `xdg-decoration`: server-side decorations are available.
- Not offered: `wp_presentation`, `wp_viewporter`, `wl_subcompositor`, `wl_drm`,
  `zwp_linux_explicit_synchronization_v1`, dmabuf feedback (v4+), YUV or multi-plane formats.

## 2. What the Mesa Wayland EGL platform will meet

- It must work without `wl_drm` and without dmabuf feedback: pick the render device itself
  (`/dev/dri/renderD*` whose `drmGetVersion` name is `asahi`), not from a compositor event.
- It must accept a `zwp_linux_dmabuf_v1` global of version 3.
- It must allocate linear `XRGB8888`/`ARGB8888` back buffers (the `modifier` events list only
  linear).

## 3. Requests to the kernel / Mesa side

None open. (When something is needed it goes here with the exact call and errno.)

## 4. Facts about the compositor side that may help

- Selected at run time; a compositor running the CPU renderer (`--renderer cpu`, or no DRM
  nodes) will not advertise `zwp_linux_dmabuf_v1` at all.
- Socket: `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY`, as the session scripts set them
  (`integrations/puredarwin/chonkstep-darwin/darwin-wm-gl-session.in`).
- A client of ours for testing the server side (allocates on the render node, fills on the
  CPU, attaches through `zwp_linux_dmabuf_v1`) will live in
  `integrations/puredarwin/chonkstep-darwin/client-dmabuf/` when it exists.
