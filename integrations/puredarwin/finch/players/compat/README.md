# Private VLC compatibility providers

Linux/Nix source builds with our own declarations. These are private prefix
providers; neither libFoundation nor libCoreFoundation is replaced. The
Foundation facade preserves the installed Versions/C Terminal/Activity Monitor
sources and Swift re-export. Audits must compare both Foundation A and C;
comparing A alone misses the framework wrapper's scripting classes.

`frameworks.nix` builds AddressBook, ScriptingBridge, WebKit and Sparkle as
unavailable-feature providers. Contact storage/automation factories return nil,
saves fail, and no web engine executes content. Web/JavaScript requests report
one asynchronous failure. The Sparkle v1 configuration API exposes no active
checks, downloads or installation; explicit requests notify the delegate with
an unavailable error. It replaces only the updater inside the test prefix,
leaving VLC's vendor GUI, codecs, demuxer and output modules as the target.
The source API reference is
[upstream SUUpdater.h](https://github.com/sparkle-project/Sparkle/blob/1.26.0/Sparkle/SUUpdater.h).

`foundation.m` provides NSAppleScript as an unavailable interpreter: source is
retained; compile and execute fail with an error dictionary. `calendar.m`
provides the Gregorian identifier, `gregorian`, in the CoreFoundation facade.
These are not permissions or scripting engines.

`coreservices.nix` retains the installed TEC/LaunchServices family and wrapper
additions. The new operations use actual FSRef/stat/statfs metadata and the
existing process-local registered-bundle cache:

* FSGetCatalogInfo supports node flags, volume, parent/node IDs and data sizes;
  other requested metadata and FSSpec output return unsupported. IDs that
  cannot fit the legacy 32-bit field fail rather than truncate.
* FSGetVolumeParms returns a stable process-local volume reference, verifies
  that its original mount is still present, and supplies its real device name
  when statfs reports one. Unavailable capability flags remain zero. This is
  the returned-volume-reference path, not a complete classic volume manager.
* LSCopyAllHandlersForURLScheme enumerates actual registered bundle URL types,
  matches case-insensitively and deduplicates identifiers. Unknown schemes
  produce an empty array; invalid arguments return NULL.

Declarations follow the public
[catalog structure](https://developer.apple.com/documentation/coreservices/fscataloginfo)
and [volume structure](https://developer.apple.com/documentation/coreservices/getvolparmsinfobuffer)
facts. Own packed declarations are checked at compile time: catalog 144 bytes,
volume 40 bytes, device pointer offset 28. A Linux-built public-API program on
the M5 observed volume version 5 and device pointer offset 28; the same program
on the M1 successfully queried its actual file and volume. No SDK header was
copied. Raw evidence: `runs/sffinch/m5-catalog-v1-results.json` and
`runs/sffinch/vlc-compat-stage-v2/candidate.json`.

`tests.nix` builds unavailable-feature, catalog and URL-registration probes.
The URL-registration probe is M1-only: it changes PD's in-process cache using
temporary test bundles, then removes them. Never run it on macOS, whose
LaunchServices registry persists. None of these probes is GUI playback proof.

`defaults.m` adds missing suite-search operations through real CFPreferences
application/suite APIs. It only installs methods absent from the current class
and checks the existing application-ID ivar layout. The M1-only fixture verifies
suite fallback, application preference priority and removal without writing
persistent preferences. `tests.nix` also includes that fixture.

V3 corrected the early facade's missing Versions/C additions and compatibility
versions. The current v4 providers passed the M1 failure-contract and catalog
fixtures; v5 combined tests also passed the defaults bridge and six decoder
option cases. Export audits retain all Foundation A/C, CoreFoundation and
CoreServices symbols. Older failed export/runtime receipts remain in runs.

`foundation-nib.nix` is a separate PRIVATE libFoundation build using the core
owner's networking recipe. `nib-allocator.py` honors AppKit's existing
+allocWithKeyedUnarchiver: hook inside the active archive-record container,
then caches the correctly sized object before decoding cyclic references.
It does not change an undersized proxy object's class. `nib-probe.m` verifies
a real512-byte padded class and cyclic UID graph; M1 passed in
`runs/sffinch/nib-allocator-stage-v1`. This is neither a canonical Foundation
install nor a claim that all VLC NIB/AppKit behavior works. The exact patch and
fixture are handed to the core owner for QEMU/guarded integration.
