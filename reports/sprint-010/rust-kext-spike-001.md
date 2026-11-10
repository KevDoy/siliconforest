# Rust in an XNU kext on the M1 (2026-11-09)

Boot `r01` on the J293 under the m1n1 hypervisor; M1 returned to Linux. The kernel collection
held the working bridge and Wi-Fi kexts plus `PDRustHello` (`integrations/puredarwin/m1-rust-kext/`),
a C++ IOKit shell around a `#![no_std]` Rust static library built with rustc 1.95.0 from the
PureDarwin flake's own nixpkgs, custom target `aarch64-apple-xnukext`, own-built `core`,
`compiler_builtins` and `alloc`.

All 20 expected console lines appeared and matched the host-run prediction exactly, ending:

    PDRustHello: rust returned 0xc2c91db8c4ef571f, expected 0xc2c91db8c4ef571f: match
    PDRustHello: rust failures=0 live_bytes=0 calls=1 counter after = 1
    PDRustHello: ALL OK

Shown on hardware: `core::fmt`, 64/128-bit arithmetic through compiler_builtins, atomics,
`Vec`/`Box<dyn>`/`BTreeMap` over a `GlobalAlloc` on `IOMalloc`, aligned and large allocations,
a variadic call into `IOLog`, and no leak (56 allocations, 56 frees). Wi-Fi in the same boot was
unaffected (join, DHCP, ping, lookup by name all passed).

Not shown: the Rust-for-Linux `kernel` crate, bindgen against the Linux compatibility layer,
or anything GPU. Those are the next phase.
