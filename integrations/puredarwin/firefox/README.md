# Firefox network support

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
