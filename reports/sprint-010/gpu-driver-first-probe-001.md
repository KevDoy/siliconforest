# GPU driver: first probe of the Rust Asahi driver under Darwin

Boot `ag02` (10 November 2026), GPU-only collection (`build-agx-kc.nix --arg wifi false`), boot
arguments `pdgpudt=2 pdgpudt_stringprops=1 pdagx=1`. `PDAgx.kext` is Omacom's Asahi driver
(Linux 7.2.11, common code + G13) compiled against the Rust-for-Linux kernel crate on the
compatibility layer; 2 small patch files against the driver.

Reached on hardware, with the driver's own log lines equal to Linux's on this machine:
bound through the platform bus -> GFX powered -> "Probing..." -> MMU/UAT initialised (kernel page
tables created, TTBs initialised, i.e. the handoff with firmware memory works) -> "GPU ID
registers: 0x4021100 0x40a04 0x30808 0x40404 0x1320300 0x101311" -> Type G13G, 8 cores ->
"Dynamic configuration fetched" (all device-tree properties accepted).

Stopped at: `HeapAllocator[Kernel Shared]::add_block: Failed to map at 0xffffffa800000000
(Error(12))`, the first GEM allocation mapped into the GPU's address space; probe returned -12.
No panic; `/dev/dri/renderD128` and `card0` appeared (from the DRM registration) but the test
found no asahi render node because the probe failed. Mesa's software path still passes.

Also found: the collection with Wi-Fi + PDAgx is rejected by the launcher ("Overlapping
file-backed segments map different bytes"): the kext area below the kernel is full.
