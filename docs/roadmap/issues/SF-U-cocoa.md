# SF-U: Cocoa stack (Mac GUI applications on the SiliconForest desktop)

## Goal
Mac GUI applications run on the SiliconForest desktop (Chonkstep) using PureDarwin's open
reimplementation of Apple's application frameworks: first a trivial AppKit application, then
upstream's own applications, then the official macOS Firefox. Work is done in QEMU on the
workstation (arm64 `virt`, TCG; the QEMU stream's environment); the M1 proof comes later.

Tools and recipes: `integrations/puredarwin/cocoa/`. Run output: `runs/cocoa/` (not committed).

## Upstream state (checked 11 November)
- The development branch of github.com/PureDarwin/PureDarwin is `upstream-xnu-12377` (there is
  no `next`); our pin `e7aa00a6` (6 Nov) is on it, the tip was `c713d8fb` (11 Nov), 119 commits
  later. Fresh clone: `sources/puredarwin-upstream-next`.
- No Firefox image, recipe or download exists in the tree. What exists is the framework work
  "for the official Firefox build" (19 Oct, in our pin), tests (`tests/appkit-ff`,
  `tests/frameworks-ff`, `tests/cg-ff`; at the tip also `tests/ffprobe`), and the website's
  statement that "a handful of unmodified macOS apps run as well, Firefox among them" on the
  x86_64 Argon desktop (`neuswc` compositor).
- The WindowServer is a library inside every application and a plain Wayland client. It
  needs wl_compositor, xdg_wm_base and wl_shm; everything else is optional (layer-shell for
  desktop-level windows, `pd_window_position_v1` for absolute positions, `pd_surface_host_v1`
  for layers hosted across processes, subsurfaces, dmabuf, data device, output management).
  Every NSWindow is an xdg_toplevel.
- The pin already has arm64 builds of the whole stack in its flake (`appkit-msm-arm64`,
  `terminal-msm-arm64`, `calculator-msm-arm64`, `argon-desktop-msm-arm64`, ...), made for a
  Qualcomm board. The tree generates its own SDK; no Apple SDK is needed.
- Decision: no rebase of the lab for this stream. Tip commits that Firefox needs are taken as
  separate patches.

## State
| Item | State |
|---|---|
| arm64 Cocoa system root (AppKit, Foundation compat, CoreGraphics/Onyx2D, CoreText, QuartzCore, CoreVideo, IOSurface, OpenGL, Carbon, CoreServices, Cocoa, WindowServer, fonts, Mesa, LLVM, Swift runtime) | built from the pin: `build-cocoa-root.nix`; every library reference and imported symbol in it resolves against itself plus the base image (`cocoa_survey.py`) |
| Minimal AppKit application (`hello/`, `build-hello.nix`) on the Chonkstep desktop, QEMU arm64 | runs: window, label, button, text field; pointer click on the button and in a view, typed keys, a menu key equivalent (Super+G) all arrive (`runs/cocoa/h2`, 6 of 6 checks) |
| Window decorations, menus, scale | upstream's as-is: AppKit draws its own title bar, no menu is visible without upstream's menu bar server, scale 1 |
| Upstream Terminal, Calculator | built, not yet run |
| Firefox | official 160.0 fetched and unpacked at test time; static import survey done against the base image only |

## Runs
- `runs/cocoa/h1`: first AppKit window on the desktop (`shots/later.png`).
- `runs/cocoa/h2`: scripted input: `COCOAHELLO button presses=1`, `mouse view=189,79`,
  `key ... chars=h`, `chars=i`, `menu item=Greet`; screenshots `shots/01..03`.

## How to run
```sh
cd integrations/puredarwin/cocoa
nix-build --extra-experimental-features 'nix-command flakes' build-cocoa-root.nix --no-out-link   # the system root
nix-build --extra-experimental-features 'nix-command flakes' build-hello.nix --no-out-link        # CocoaHello.app
python3 cocoa_image.py --output runs/cocoa/<run>/disk.img --tree <root>=/ --tree <hello>=/ --tree <libSystem>/usr/lib=/usr/lib \
    --file <session>=/usr/bin/cocoa-session --file <compositor>=/usr/bin/chonkstep-darwin-desktop ... 
python3 cocoa_run.py --image runs/cocoa/<run>/disk.img --out runs/cocoa/<run> --script steps.json
```
The libSystem is the Homebrew stream's (`../homebrew/build-libsystem.nix`: the
`___chkstk_darwin` fix and the added entry points).
