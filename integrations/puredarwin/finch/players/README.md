# Third-party media target

Verified publisher downloads are described in downloads.json: [VLC's checksum](https://get.videolan.org/vlc/3.0.24/macosx/vlc-3.0.24-arm64.dmg.sha256)
and [IINA's release page](https://iina.io/download/). DMGs, extracted apps,
bundled libraries and signatures stay in ignored private/mac-apps/third-party/.
No third-party app binaries are committed. Existing licenses remain bundled.

`fetch.py --execute` verifies published SHA256. Extract locally with Nix 7zz.
`prepare.py` requires that verified DMG and a new stage; records hashes of its
copied bundle, and copies our generated H264+AAC movie. Run sf_gapscan.py per
SF-Y against the staged app and current root plus all selected media/interface
plugins. GAPS.md preserves direct missing imports/classes/selectors by provider.
Plugin scan: runs/sffinch/player-scan-v2/vlc-plugins-live-root.md.

VLC has62 direct imports,0 missing; its loaded closure initially lacks two errno
globals and five Breakpad Mach service APIs. IINA has2202 direct imports,783
missing strong,30 missing classes and407 selectors absent throughout its closure.
VLC is the smaller target. The installed CoreMedia/CoreVideo, VideoToolbox,
Accelerate, ColorSync and CoreImage cover the listed direct imports with no
reported gaps in those providers. IINA's bundled FFmpeg gaps remain bundled
library-version issues; root media providers do not supply private libavcodec
symbols. A load/export match is not a functional playback test.

VLC plugin gaps filed to TERM6: strong AddressBook/ScriptingBridge/WebKit loads,
CAOpenGLLayer, five AppKit constants, three display fade functions plus OpenGL
acceleration query, Foundation NSAppleScript and CoreServices catalog/handler
functions. The auhal plugin has no actual missing CoreAudio import in this scan.
The VideoToolbox plugin has four field/deinterlace keys not yet supplied.
Standalone plugin @executable_path diagnostics must be evaluated in the real app
entry context; missing Sparkle/libvlccore paths there are not proven app gaps.

Staged attempt history:

- Original open-a returns0 but no surviving process. Direct diagnostic aborts
  before main on _sys_errlist. Own prefix-only system-compat.c builds actual
  strerror-backed tables and reexports existing libSystem; no core replacement.
- With that facade, vendor main reaches its intentional root refusal. Existing
  nobody cannot access the desktop Wayland socket. No global permissions or
  identity-return APIs were changed.
- vlc-main.nix rebuilds the public GPL upstream darwinvlc.m on Linux with the
  upstream ALLOW_RUN_AS_ROOT option. Breakpad is excluded at compile time; no
  pretend Mach crash-service success functions. Original vendor main is retained
  in each bundle. Our CFBundleExecutable selects the named rebuilt entry.
- First rebuilt entry lacked @rpath/libvlc search path; direct dyld failure is
  retained. Recipe supplies @executable_path/lib.
- Corrected Mac flags are --ignore-config and no --one-instance (not a Mac
  option in this build). Process-local SF_VLC_LOG captures original-main
  checkpoints. v8/v9 reaches libvlc_new, returns NULL, exits1; public error
  API supplies no message. These failures remain recorded.
- Upstream Darwin config_GetLibDir recognizes an executable image ending in
  VLC. The staged entry is now SiliconForest-VLC, so unchanged plugin discovery
  reaches real plugins. v10 reports missing CAOpenGLLayer (QuartzCore),
  NSImageNameNetwork (AppKit/Growl) and VT DeinterlaceMode; direct bounded run
  then exits SIGSEGV. No successful window or playback is claimed. Exact
  runtime evidence and dependency requests are filed to TERM6.

`launch.py --capture` uses open-a, local movie, own file log and same-uid
compositor capture only when its own VLC process survives. It sends no synthetic
screen input and never signals another application. Missing clicks go to TERM6
in requests-for-TERM5.md. Playback, decoded counts, HAL accepts and capture are
still unproven; open success alone is not launch/playback evidence.

REA confirmed original thin arm64 entry identity; its Mach-O utility requires
macOS and deep providers are unavailable on this Linux session. No REA execution
or decompilation claim is made. sf_gapscan and actual M1 stderr are the relevant
import/startup evidence here. Preserve failed receipts under runs/sffinch/.
