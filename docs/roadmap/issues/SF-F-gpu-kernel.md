## Goal
The M1's GPU (AGX G13) driven by Omacom's kernel driver, running in our kernel, presenting the same interface to userspace as on Linux.

## Decision
**Reuse Omacom's Rust driver; do not rewrite it in C.** Build the Rust-for-Linux `kernel` crate on PureDarwin's Linux compatibility layer (`src/Kernel/fbsdkpi`: FreeBSD LinuxKPI plus drm-kmod) and compile the driver against it. Reasons:
- The driver's value is its firmware-version-specific structure definitions; a rewrite forks from Omacom permanently.
- PureDarwin's DRM core already includes the scheduler, GPU VM, exec and sync objects, the exact pieces this driver selects.
- Rust now runs in our kernel (proven on the M1, `reports/sprint-010/rust-kext-spike-001.md`).

Fallback if the crate approach stalls: a C port in the style of `PDPowerVR` (13,552 lines), using the Rust driver as the specification. Decide at F3.

## Measured inputs (`docs/GPU_PLAN_NUMBERS.md`)
- Driver at the lab pin (Linux 7.2.11): 111,468 lines of Rust; the M1 needs the common code plus G13, **29,653 lines**. The rest is later GPU generations.
- It references about 27,600 lines of `kernel`-crate abstractions directly, calls 122 C symbols itself, and sits on about 9,000 lines of C (Apple RTKit and mailbox, DRM scheduler, GPU VM).
- It reads about 180 device-tree property names; on Linux, m1n1 synthesises many GPU ones (`src/kboot_gpu.c`, 3,400 lines). Darwin has only Apple's device tree.
- Matched pair, proven daily on this M1: `linux-aurora 7.2.11.aurora6` with `mesa-aurora 26.3.0_devel.20261025.147a62f6190` (omacom/mesa branch `integration/aurora`).
- Gaps: no Rust in PureDarwin's kernel build before our spike; the DRM core there is at about the Linux 6.14 level while the Rust wrappers target 7.2.

## What exists
- `integrations/puredarwin/m1-rust-kext/`: toolchain (rustc 1.95 from the pinned package set, custom target, own-built `core`/`alloc`), `PDRustHello` proven on hardware. Work on bindings and the crate is in progress in `lkpi/`.
- RTKit, power domains, DART from the Wi-Fi work (to be shared through SF-A).
- Earlier, never-run GPU groundwork: `integrations/puredarwin/m1-gpu-*` (3,139 lines of C: UAT page tables, RTKit control, channel codec).

## Work items
| ID | Deliverable | Acceptance | Size |
|---|---|---|---|
| F1 | Bindings generated from the compatibility layer's headers; status table of every header the Linux helper includes (present / broken / absent) | Table committed; a `bindings` crate builds | in progress |
| F2 | `kernel` crate core tier on LinuxKPI (errors, allocation, locks, time, logging), as a patch series against the pristine crate | Test kext `PDRustLKPI` prints ALL OK on the M1 | in progress |
| F3 | Subsystem tier status matrix (device, platform, of, io, dma, DRM gem/sched/gpuvm/syncobj, RTKit) with each gap sized; **go/no-go on the crate approach** | Matrix committed; decision recorded here | in progress |
| F4 | Close LinuxKPI holes and bring the DRM scheduler/GPU VM to the level the wrappers expect (or adapt the wrappers) | Subsystem tier compiles and links | per F3 |
| F5 | Apple pieces under the crate: RTKit and mailbox bindings on SF-A's RTKit; `of_*` over the Apple device tree | Unit kext reads GPU node properties through `kernel::of` | ~1,500 lines |
| F6 | Device-tree synthesis for the GPU: port the T8103 part of m1n1 `kboot_gpu.c` so the driver finds the properties it expects | Property dump matches what Linux sees on this M1 (`/proc/device-tree`) | ~600 lines |
| F7 | Build the G13 subset of the driver; boot the GPU firmware | Kernel log: firmware version detected, coprocessor "ready", no crash | 29.6k lines compiled, few changed |
| F8 | `/dev/dri` render node speaking `asahi_drm.h` through `PDDRMDri` | `drmGetVersion` returns `asahi`; `GET_PARAMS` matches Linux on this M1 | small |
| F9 | First render: one submitted job completes | Mesa's off-screen test (SF-G G5) passes by pixel hash | integration |

## Dependencies
SF-A (RTKit A3, PMGR A2). SF-G depends on F8. Independent of SF-H until zero-copy scanout.

## Interfaces this ticket owns
`integrations/puredarwin/m1-rust-kext/` and the future GPU kext directory; the Rust toolchain definition (`rust/toolchain.nix`). Any other Rust-in-kernel user builds on that toolchain rather than adding one.

## Risks
- The kernel collection has one global symbol namespace; Rust code shared between kexts must live in one kext with explicit C exports.
- Allocation semantics (blocking vs atomic) differ between LinuxKPI-on-XNU and Linux.
- API drift 6.14 to 7.2 in scheduler and GPU VM.
- GPU firmware state when the hypervisor hands over is unverified.
