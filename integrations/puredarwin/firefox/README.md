# Firefox integration and verification

## Network notification backend and quit-crash qualification

`sc-network.nix` builds a private, complete SystemConfiguration framework from
upstream source. It preserves the existing address-based route lookup and adds
reachability callbacks on the caller's CF run loop. `sc-route-source.c` blocks
in kqueue on real PF_ROUTE messages and resolver-file/directory vnode events;
it does not poll or modify network configuration. Atomic resolver replacement
reopens the file watcher. Source destruction joins its worker and releases its
socket/context/descriptor resources. Scheduled reachability targets must be
unscheduled before their final caller release.

When configd is unavailable, callback-bearing dynamic-store sessions can use
this read-only network-notification backend. The session retains the canonical
SCDynamicStore CF type; supported network key patterns receive invalidations
from actual interface/route and resolver events. General store reads/writes
remain unsupported without configd. No EAPOL state is invented. This addresses
Firefox's network initializer returning early and subsequently removing and
releasing its uninitialized run loop during shutdown.

`sc-route-probe.nix` checks file rewrite/replacement notification and 65 complete
source lifetimes. `sc-network-probe.nix` checks 65 API lifetimes, actual callback
delivery, balanced context ownership, descriptor stability, idle CPU and
unsupported-operation failures. These native controls alone do not prove
Firefox can quit: require three actual browser natural quits, no family left
alive or cleanup signals, and UUID-matched private framework mappings before
`install-network-runtime.py` can select it. The installer plans by default,
requires cold original build bytes, preserves the launcher/manifest for checked
rollback, and leaves owner processes and profiles alone. Reporting remains
disabled until the crashreporter UI itself is qualified.

After a verifier failure has triggered rollback, `--retry` can reselect the same
qualified image. It checks both prior text hashes, the original cold payload,
and the executed image's load-command UUID. It preserves the original backup
and never treats lowered executed Mach-O bytes as a new pristine image. A
changed launcher or runtime manifest prevents both retry and rollback.

## Temporary QA data

Native QA directories can contain large renderer captures and fresh browser
caches. `retire-owned-run.py --run runs/firefox/<run>` verifies the collected
archive by reading every file and plans retirement of that exact remote run.
With `--apply`, use the shared GUI or core lock: it checks coordination again,
refuses live process and installed-runtime references, and records before/after
free space. It never removes a tool or provider prefix. The workstation archive
and extracted evidence remain available; executed image bytes in an archive
remain diagnostic evidence, not a pristine build source.

## Installed page and window gates

Run browser diagnostics through `launch.py --installed-bundle --bundle
/Applications/Firefox.app` with a fresh owned profile, the coordination gate and
shared GUI lock. All synthetic input uses the virtual seat and an explicitly
admitted owned window. Agent launches disable crash reporting.

`--functional-qa --presentation-seat --site-legibility` collects actual renderer
visible, text-hidden and restored captures. The checker includes supported
iframe documents, suppresses CSS transitions throughout the paint control,
and requires ink in every eligible visible text run plus faithful restoration.
`--paired-site-legibility` repeats with web fonts on/off in the same owned
profile, preserving cookies and restoring the original preference even on a
measurement failure. Compare its `legibility-on` and `legibility-off` outputs
with `compare-legibility.py --on-prefix legibility-on --off-prefix legibility-off`.
Inspect the actual renderer pictures, final URLs and requested-site coverage:
a CAPTCHA, interstitial, empty page or duplicate tab never qualifies ordinary
site legibility. Pixel differences alone cannot establish correct glyphs/fonts.

`lifecycle-run.py` pairs the owned audio fixture with per-process callbacks,
content-process identities and compositor window observations before cleanup.
`analyze-lifecycle.py` requires media teardown after close, no invisible live
window after close, and an actual compositor reopen/Quit route. Hidden and
minimised windows must retain their media and reopen the same tab. The
`--quit-menu-control` diagnostic can isolate an application termination-routing
failure, but cannot qualify desktop Quit. Cleanup signals never prove natural
shutdown, and callback silence does not establish acoustic output.

## Shared decoder video selection

`install-video-runtime.py` selects the complete qualified CoreVideo/VideoToolbox
family, preserving system graphics, the network fix and corrected audio clock.
Its dry plan checks the retained matched playback evidence, original cold-file
ledger, reviewed renderer pictures and loaded-image UUIDs. Apply only under the
shared GUI/core/bench locks after notice; follow immediately with the media
owner's `run-installed-video.py` acceptance packet and inspect both renderer
pictures. Failed or incomplete acceptance requires checked rollback. The
installed gate requires complete 600/300-frame playback, hardware jobs, pixels,
provider paths, natural status0 and no surviving browser family before cleanup.
Decoder pool retirement remains disabled. These gates do not establish panel
pacing, acoustic synchronisation or smooth playback while interacting.
