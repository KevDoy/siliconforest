# Experimental PureDarwin Chonkstep backend

This opt-in crate is the CPU Wayland/PDHID increment against Chonkstep
`e0fe3a000aec71b1db46b940e2a75670be4e0f31`. It uses the existing `wm-core`
`WindowManager`, `wm-theme` `RasterThemeEngine` with the NeXTSTEP Classic theme,
and the pinned vendored Smithay 0.7 frontend. Linux crates are unchanged.
There is no Sway subprocess, dummy renderer, fake input, or control IPC server.

The frontend processes real Wayland requests over a Unix socket. It negotiates
xdg toplevels and server decorations, imports ARGB8888/XRGB8888 SHM, copies
client pixels under Smithay's guarded SHM accessor, then releases the buffer.
Only owned premultiplied RGBA pixels survive that release. wm-core manages
mapping, placement, focus state, title/chrome changes, transients and workspace
visibility. tiny-skia composites those pixels and the actual Chonkstep raster
decorations. Destruction/disconnect retires the scene and callback records.
Unmapping requires a fresh configure/ack before remapping.

The `Pdgop` presenter owns the native connection for its entire drawing lifetime.
Its original C ABI is exactly 64 bytes, alignment 8; it matches
`src/Libraries/PDGOP/include/PDGOP.h` SHA-256
`c1b80e3bf0bd4ba640f503e8297749bc03ca3165c23e6e4c3478ff740449930a`.
Do not link this caller against a PDGOPOpen implementation that writes an
extended structure. It checks the native return code **and** service, connection,
address, dimensions, stride, span, pixel type and RGB masks before writing.
It requires PureDarwin `IOObjectConformsTo(service, "IOGOPFramebuffer", &conforms)`
to return `KERN_SUCCESS` and a nonzero boolean, and refuses other
backends. Both 8-bit RGB and 10-bit RGB in 32-bit storage are supported; masks
are exact, and row padding is not written.

Native presentation consists of volatile mapped framebuffer stores followed by
`dsb oshst`. It never calls PDGOPPresent: selector 11 is VirtIO-specific and is
not a presentation operation on IOGOP. Acceptance means stores were completed,
not measured vblank, scanout, or a visible panel result. Frame callbacks are
completed only after presentation succeeds. `Drop` calls PDGOPClose, whose
contract unmaps before closing the IOKit connection. The presenter is deliberately
neither Send nor Sync; no renderer or cleanup thread can retain the mapping.
Native ownership/mapping/scanout and failure-close behavior still need separate
guest qualification. This crate has not run on the M1.

## Offline checks and native build contract

```sh
cargo check --offline -p wm-darwin --features darwin-backend
cargo check --offline -p wm-darwin --features darwin-backend --target aarch64-apple-darwin
cargo test --offline -p wm-darwin --features darwin-backend
```

The target check type-checks the real native presenter but does not link it.
The project lead owns the SDK linker wrapper and native build:

```sh
cargo build --offline --release -p wm-darwin --features darwin-backend --target aarch64-apple-darwin
```

The link needs matching PureDarwin static `libPDGOP.a`, minimal static `libIOKit.a`,
plus `libxkbcommon` from Smithay's input types and the PureDarwin
libSystem stubs. It does not need DRM, GBM, EGL, GL, libinput, libseat, X11,
XWayland, libwayland-server C bindings, or a proprietary macOS SDK. Root supplies
all SDK/toolchain paths. Do not change the Rust target to Linux: Apple's std
selects the Darwin ABI and kqueue. A successful host test or target check is
not native link or runtime proof.

`IOObjectConformsTo` uses the **three-argument kern_return_t ABI** declared in
PureDarwin `PDIOKitLib.h:92`, implemented in minimal `IOKitLib.c:237`. The former's
SHA-256 is `22617ad9234b80b1153ca5b6d7eb5eaa6fb9fd2ab820bc334641ce51f47f75e2`;
the latter's is `3550629168210f9b3d0d5f85156742fbebe35de1b6b6d68380c74b369c94ce61`.
Apple's public two-argument boolean ABI is incompatible. The Rust declaration
and link name have been corrected before any native execution. PDGOP's legacy
matching-service ABI must use the minimal static IOKit implementation. Do not
substitute the CF-shaped matching API in dynamic IOKitCF. Both Rust link
attributes explicitly request static archives. Supply their paths through
Rust's `-L native=...`, not only inside the final linker wrapper.

The binary prints usage without opening PDGOP unless passed
`--run --socket /absolute/private/runtime/wayland-0`. The caller must create an
owned mode-0700 runtime directory. Clients use that socket through
WAYLAND_DISPLAY/XDG_RUNTIME_DIR; no clients are launched implicitly. SIGINT and
SIGTERM stop the single main loop and allow native teardown. There are no reboot,
boot-storage, server enrollment or hardware-recovery operations in this binary.
The lead's external recovery controller remains necessary for initial testing.

Append `--input pdhid` to acquire the fixed `/dev/usb_hid_kbd` and
`/dev/usb_hid_mouse` character devices. Both must open successfully before the
frontend advertises keyboard and pointer capabilities. `--input pdhid-keyboard`
opens only `/dev/usb_hid_kbd` and advertises a keyboard capability with no
pointer; a missing mouse node is not an error in that mode, and a missing
keyboard node is. Without an input option the frontend stays rendering-only
with no wl_seat. There is no alternate input file, pipe or network injection
interface in the binary. Opening a queue or advertising a capability is not
native input.

NeXTSTEP Classic requests DejaVu Sans. Install an appropriately licensed font
into the guest's `/Library/Fonts` or another fontdb Darwin search path when
assembling the rootfs; no proprietary fonts are needed. No font is fetched here.

## PDHID input contract

The pinned kernel ABI is `PDHIDEventQueue.h`: keyboard records are 8 bytes and
mouse records 12 bytes, both little-endian. Reads return exactly one record or
EAGAIN. These devices do not support kqueue filters; the main loop drains up to
64 records of each kind every 8 ms. File descriptors stay owned for the complete
session. Opening the keyboard also grabs its console HID path; closing restores
it. `--input pdhid-keyboard` owns only that keyboard descriptor and does not
open `/dev/usb_hid_mouse` or construct a pointer seat. This does not claim exclusive access: the kernel currently allows competing
readers, so run only one queue consumer. Startup discards the bounded pre-open
backlog; the operator must release all keys/buttons before launching.

The decoder validates record size, reserved fields, key state and flags, handles
sequence wrap, and refuses gaps/reordering (queue overflow or another reader).
On an input error the main loop exits, dropping its owned descriptors and
presenter. It does not invent releases to conceal lost state. Keyboard usages
are translated to evdev/XKB numbering, including modifiers; duplicate keydown
records are suppressed and clients receive real repeat_info. Pointer fractions,
signed relative movement, per-device button aggregation and wheel events are
translated to the actual Wayland seat. wm-core handles focus and frame hit
testing, titlebar moves/resizes, and close. Client-requested moves/resizes require
the matching seat, active left-button grab serial and exact initiating surface.
Client cursor SHM is copied/released like window pixels; cursor frame callbacks
also wait for presentation. Touch and tablet protocols are not advertised.

The guest needs the XKB `evdev` rules, `pc105` model and `us` layout data. Package
the licensed xkeyboard-config tree and set `XKB_CONFIG_ROOT` to its guest path
when the library's compiled-in prefix is a builder-only store path. Missing
keymap data is an initialization error, not a fallback keyboard success.

Source evidence: `PDHIDEventQueue.cpp:129-178` defines the read ABI;
`:108-125` defines the keyboard console grab; `:538-610` produces records and
overwrites the oldest entry on overflow. Its keyboard sequence increment is
outside the queue lock: concurrent producer reordering may therefore trigger
the deliberate discontinuity refusal. `PDSerialHID.cpp:77-106` can feed this
same kernel HID path from SOH serial reports. This is a serial virtual HID
transport, not proof of the M1's built-in keyboard or trackpad. No input device
has been opened by this agent; real guest queue-to-client delivery is still
unqualified, even though the host wire test passes.

## Current compatibility limits

No subcompositor, layer shell, dmabuf, GPU, clipboard/DND, lock, capture,
privileged protocol, output-management or X11 global is advertised. Popups are
dismissed explicitly. Maximize/fullscreen capabilities are omitted; requests
receive an unchanged configure, not a successful mode claim. Minimize has no
restore UI and is explicitly refused. Scale other than 1, transformed buffers,
nonzero attach offsets and cropped xdg geometry are rejected with a protocol
error. These are real client compatibility limits, not completed implementations.
The source supports multiple managed toplevels, but only a single output and
the built-in classic decoration recipe are qualified by the host test.

The host wire test uses an actual wayland-client connection, SHM file and
xdg configure/ack. Its test-only memory presenter exercises real wm-core
workspace hide/show, raster chrome, retained pixels after release, unmap/remap,
destruction and failure gating of callbacks. The memory presenter is explicitly
not a PDGOP substitute or hardware result. Layout tests cover the 64-byte C ABI
and RGB30 conversion/stride/range refusal. The input wire test feeds labelled
synthetic PDHID records into the real decoder, receives XKB keymaps, key/modifier
and pointer events, and tests client cursor pixels, Chonkstep titlebar dragging,
close and authenticated client-requested dragging. It does not open native
devices or establish hardware input. Native linkage, syscall availability,
framebuffer cache attributes, physical scanout and input remain unqualified by
this source receipt; the project lead records independent native evidence.
