## Goal
Run at full CPU speed and on all eight cores.

## Why
The desktop is slow partly because the kernel runs on one efficiency core at whatever frequency iBoot left. Measured facts: the boot CPU is an Icestorm core (MIDR part 0x022), boot-args carry `cpus=1`, and nothing raises the performance state.

## Decision
- **Frequency:** first call m1n1's existing `cpufreq_init` from the launcher (configuration only). Later, a native kext ported from Linux `drivers/cpufreq/apple-soc-cpufreq.c` (461 lines, GPL-2.0).
- **SMP:** native code in the platform expert, ported from m1n1 `src/smp.c` (MIT). XNU already has the Apple SMP machinery (`USE_APPLEARMSMP`); it needs a real `enableCPUCore`.
- **The boot CPU stays an efficiency core.** XNU and m1n1 both take the core iBoot started; the fix is SMP plus scheduler placement, not changing the boot core.

## What exists (read from source; see `docs/roadmap/research/cpu-boot.md`)
- One CPU is enforced in four places: launcher (`hv.smp = False`, secondary start stops the guest), hypervisor device-tree pruning of non-running CPUs, `cpus=1` in the boot-args, and `PDAppleBootCPUPMGR` (130 lines) which panics in `enableCPUCore`.
- The m1n1 hypervisor already supports multi-vCPU guests (start hook on the PMGR CPU-start registers, 1:1 vCPU to core).
- `PDAppleAIC` routes every interrupt to CPU 0; IPIs are system-register fast IPIs and need no AIC code.
- `PDPerfControl` (85 lines) has an efficiency/performance cluster policy that the Apple platform-expert path does not call.

## Work items
| ID | Deliverable | Acceptance on the M1 | Size |
|---|---|---|---|
| C1 | Launcher calls `p.cpufreq_init()` before starting the guest | A fixed CPU loop in the guest runs at least 2x faster than today's baseline; cluster P-state register readback logged | ~5 lines |
| C2 | `enableCPUCore` for T8103 from m1n1 `smp.c:319-372`: PMGR CPU-start registers (`+0x54000`), per-cluster start bits, reset vector check | Compile evidence plus a register-sequence comparison against m1n1 | ~200 lines |
| C3 | Launcher SMP mode (needs SF-B B4), drop `cpus=1` | `sysctl hw.ncpu` reports 8; both clusters visible | ~100 lines |
| C4 | Scheduler placement: wire `PDPerfControl` on the Apple path; interrupt affinity review | An 8-thread load runs 10 minutes without panic; interactive threads land on performance cores | ~50 lines + soak |
| C5 | Native DVFS kext so frequency follows load | Performance cluster reaches its top states under load; 1,000 transitions without a hang | ~500 lines |
| C6 | Rerun the Wi-Fi lifecycle and throughput tests on 8 CPUs (the driver has only ever run on one) | Tests pass; any locking bug filed against SF-M | test only |

## Dependencies
C1 has none: do it first. C3 needs SF-B B4. C2 can be written in parallel with everything.

## Risks
- Whether our modified kernel start code still places the secondary reset vector where the hypervisor expects.
- XNU's idle path writes a register the hypervisor refuses in some modes.
- Hypervisor cost with 8 CPUs (IPIs trap; a slow path stops all CPUs).
- No thermal management exists once frequencies are raised.

## Out of scope
Running without the hypervisor.
