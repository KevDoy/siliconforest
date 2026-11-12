## Goal
The SiliconForest kernel on the M1 without the m1n1 hypervisor: m1n1 hands the machine to XNU and is gone (GitHub issue #44). Still loaded into RAM over USB from the controller, still the same desktop, the M1 back in Linux afterwards. No disk, boot chain or boot policy in this stream.

## State
**The default desktop runs without the hypervisor, on all eight processors** (boots `nb07`, `nb08`): GL compositor on the panel with page flips, both terminals, Wi-Fi joined with an address and a ping, coreaudiod, Touch Bar ready, clean end of the session; the console challenge answered over the real UART; the M1 returned to verified Linux by the controller every time.

What the native kernel needed that the hypervisor had been providing:

| Need | Where | Kind |
|---|---|---|
| Entry at EL1 (open-source XNU has no EL2 entry), with EL2 set as the hypervisor sets it for a guest but trapping nothing | `stub/el2_drop.S`, one resident 16 KiB page; `native_boot.py` | launcher |
| Secondary cores: started and initialised by m1n1 (chicken bits), held at EL2, released into XNU's reset vector at EL1 when the kernel writes the core's reset-vector register | `stub/el2_drop.S` (`hold_entry`), `native_boot.hold_secondaries`; `platform-patches/0001` (boot-arg `pdcpuheld=1`: write the register, send an event, do not write the PMGR CPU-start block) | launcher + platform kext |
| Console: the real `uart0` routed to the USB-C debug pins (`macvdmtool serial`), typing paced to the UART's 16-byte FIFO | `native_boot.py` | harness only |
| End of session: the controller's reset without a proxy to see | `launcher-native.patch`, word `vdm-return` | harness only |

Nothing else: **no XNU patch and no device-tree change.** Timer, IPI and PMC FIQs arrive at EL1 without the hypervisor's re-injection; every driver of the default collection started as under the hypervisor; no SError, no panic in any native boot. With one processor the unchanged default collection boots natively (`nb04`, `nb05`).

Collection: `/nix/store/k2w23az428jr3nmqlca19da00kn4wshn-puredarwin-kc-arm64-0.1`, recipe `integrations/puredarwin/m1-native-boot/build-kc.nix` (the default collection `z66w83kk…`, `m1-boot-speed/build-kc.nix`, with only PDArmPlatformExpert replaced; the patch is inert without `pdcpuheld=1`, so it also boots under the hypervisor's boot-args, not booted that way). Boot-args as the default (`pd_quiet_wait=0 pd_rtc_wait=0`) plus `pdcpuheld=1` for more than one processor; launcher `launcher-native.patch` (a superset of `m1-boot-speed/launcher-fast-quiet.patch`) with `native_boot.py`, `native_stub.py`, `native_mode.txt`. One command: `NB_MODE=native NB_RETURN=vdm NB_WORDS="vdm-serial noiolog" NB_PROFILE=desktop NB_KC=<collection> integrations/puredarwin/m1-native-boot/tools/nb_boot.sh <nbNN> 175`.

### Timings (seconds after the kernel starts; `tools/nb_times.py`)
| | hypervisor `df02` | native `nb07` | native `nb08` (`noiolog`) |
|---|---|---|---|
| launchd started | 1.32 | 4.02 | 1.62 |
| shell prompt seen | 5.22 | 11.46 | 4.84 |
| guest command typed | 5.43 | 12.64 | 6.02 |
| guest script begins | 5.63 | 13.14 | 6.52 |
| compositor starts | 5.82 | 13.74 | 7.12 |
| **first frame** | **7.27** | **15.49** | **8.67** |
| both terminals ready | 7.41 | 15.62 | 8.87 |
| Wi-Fi joined | 10.81 | 18.29 | 11.91 |
| ping answered | 15.67 | 22.88 | 21.90 |
| console bytes | 75250 | 75310 | 28945 |

The native kernel is not slower; its console is. Natively the console is a real 115200 baud UART (11.5 KB/s) that the kernel waits for: the 75 KB of a full boot log cost about 8 s (`nb07`). With driver `IOLog` lines kept off the UART (`serial=0x13`, word `noiolog`) the shell prompt is there 0.4 s earlier than under the hypervisor, and the remaining 1.4 s to the first frame are the harness: 1.2 s instead of 0.2 s from prompt to typed command (typing at 320 bytes/s) and 29 KB still printed. An installed system has neither. The ping 10 s after the join in `nb08` (5 s under the hypervisor, 4.6 s in `nb07`) is one sample and was not looked into.

Not proven: see Open.

## Findings

### How the M1 is reset today, and what that depends on
- The controller ends every boot with one `macvdmtool reboot serial` on the M5 (`tools/m1_kernel_cycle.py`, `RealOps.reset`): a USB-PD vendor message to the M1's USB-C port controller, which resets the SoC. Neither m1n1 nor any software on the M1 takes part. It was qualified in sprint 008 against a **halted Linux** three times (`reports/sprint-008/recovery-three-cycles.json`, `docs/M1_RECOVERY.md`), so it does not need the proxy. Against a hung XNU it has not been run; `docs/M1_RECOVERY.md` itself lists "arbitrary hang" as not qualified.
- The controller's **rule** for sending it does depend on m1n1: `run_attempt` in `tools/m1_bringup_cycle.py` resets only when the m1n1 proxy's USB device is visible ("a healthy Linux is never reset"). A native XNU never shows that device (m1n1 shuts its USB down before it jumps), so with the unchanged controller a native boot ends in `RECOVERY_INCOMPLETE_NO_RETRY` with the latch armed and the M1 left in XNU.
- Two ways out, both implemented, neither used yet:
  1. `vdm-return`: `launcher-native.patch` gives the controller one more case. When Linux and the proxy are both absent **and** the launcher recorded that it vectored into a native XNU (`guest/native-start.json`), it waits 45 s for a Linux that is coming back by itself and then sends the same single reset. Only the per-boot copy of `tools/m1_bringup_cycle.py` changes; `tools/m1_kernel_cycle.py` (the hash-pinned recovery code) is untouched.
  2. `wdt=<s>`: the launcher arms the SoC watchdog (WD1 of `/arm-io/wdt`, 24 MHz, at most 178 s) just before the jump. Nothing in this kernel services it, so the M1 resets itself whatever state it is in, and the unchanged controller then finds Linux. Not proven on this bench: the M1's Linux reboots through the SMC (`macsmc-reboot: Handling reboot and poweroff requests via SMC`) and only holds this watchdog (systemd, 30 s) without ever letting it expire, so a watchdog reset would be a kind of reset this M1 has not been seen to take.

- Observation, for the unexplained recovery-mode incident (not this stream's to fix): after `nb02` the M1's Linux logged `macsmc-reboot: PMU logged 1 boot error(s) and 0 panic(s)`. Every cycle that ends in an external reset leaves one boot error in the PMU, and the Linux boot that follows clears it. A chain of resets with no Linux boot in between would let that count grow; whether Apple's firmware goes to recovery at some count is a hypothesis, not tested. A native boot has the same one reset per Linux boot as a hypervisor boot.

### Console without the hypervisor
- Under the hypervisor the guest's `uart0` is virtual. Natively XNU writes to the real one, which the M1's port controller can route to the USB-C debug pins; the M5 reads them as `/dev/cu.debug-console` (115200 8N1).
- `nb01`: listening there during an ordinary cycle gives **0 bytes**. The serial routing that `reboot serial` sets does not survive the Linux boot and reboot in between.
- `nb02`: after `macvdmtool serial` (the mode half of the same tool, no reset; 0.1 s, the USB proxy link unaffected) the M5 received the marker line the launcher had m1n1 write to the real UART, and m1n1's own output up to the point where the hypervisor takes the UART away: 1714 bytes, `marker_seen: true`. Boot result unchanged (`DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`).
- So a native boot's console is the same `ConsoleSession` on `/dev/cu.debug-console` instead of the USB console endpoint; the console challenge is typed and answered over the real UART. M5 to M1 on that line was proven in sprint 008 against Linux (`reports/sprint-008/serial-bidirectional.json`); with XNU's polled UART input it is not proven.
- Diagnosis below the kernel's first line: the EL2 page (next section) prints on the same UART when it starts, when EL2 is configured, and on any exception that reaches EL2 or reaches EL1 before XNU has its own vectors (`PDNB EXCEPTION at EL<n> vector .. esr .. elr .. far .. spsr .. mpidr ..`).
- Not used: RAM or PMGR scratch surviving a reset (a whole Linux boot lies between the reset and the next proxy session), the panel (nobody watches), Wi-Fi (works only once userland is up; the desktop profile has it anyway).

### EL2 or EL1
- m1n1 runs at EL2 and its ordinary chainload (`proxyclient/tools/chainload.py -x`, `P_VECTOR`) enters the next stage at EL2. Apple's XNU handles that itself. The open-source XNU does not: `pexpert/pexpert/arm64/H13.h` defines `HAS_EL2` and nothing uses it; `osfmk/arm64/start.s` has no EL2 code. Under the hypervisor the kernel has always run at EL1.
- This core cannot clear `HCR_EL2.E2H`, so "EL1" here means what the hypervisor's guest gets: `E2H=1, TGE=0`. `stub/el2_drop.S` is one resident 16 KiB page that sets EL2 up with the hypervisor's own values (m1n1 `src/hv/hv.c`, `proxyclient/m1n1/hv/__init__.py`) minus everything that makes it a hypervisor, enters XNU at EL1h, and afterwards only reports. It lies in m1n1's memory, below the first byte the boot arguments give to XNU.

### What the hypervisor does for the kernel today
From m1n1's sources and the worker logs of `df02` and `nb02` (every access the Python side handles is logged there).

| Provided by the hypervisor | Native replacement / expectation |
|---|---|
| Console: `uart0` trapped, forwarded over USB | real `uart0`, read on the M5's debug console (proven up to m1n1, above) |
| Stage 2 translation, MMIO pass-through, MCC carve-outs unmapped | none needed; the memory layout and boot arguments are the same |
| FIQs routed to EL2 (`FMO=1`) and re-injected: both EL1 timers, fast IPIs, PMC | `FMO=0`, `VM_TMR_FIQ_ENA_EL2=3`: FIQs taken at EL1 directly. Works: timers (`sleep 3` returns after 3.02 s, `nb05`), IPIs between eight cores (`nb06`). |
| IRQs: passed through (`IMO=0`), AIC untouched | same |
| Trapped system registers (`HACR_EL2`: HID, EHID, ACC, IPI, PM, CTRR, SError info). Seen from this kernel: `OSLAR_EL1` (passed), `ACC_CFG_EL1` read then written with the same value (write dropped), IPI and PMCR0 (passed, FIQ re-injected), `CYC_OVRD` (write dropped) | no traps: all reach hardware. `ACC_CFG` gets the value it already has; `CYC_OVRD` writes take effect (idle is a plain WFI unless `pdidledeep=1`; offline cores are parked, not powered down) |
| SErrors and external aborts trapped (`AMO`, `TEA`) and reported | taken at EL1: a panic on the UART |
| Debug traps (`MDCR_EL2`), breakpoints | none |
| PMGR write hooks: `UART0`, `ATC0_USB`, `ATC0_USB_AON` cannot be switched off | none; the logs show reads of two of them and no write |
| Two PMGR registers whose reads are faked (`0x23d29c044`, `0x23d29c05c`) | real values; whether this kernel reads them is not visible in the logs |
| Secondary cores: the write to the PMGR CPU-start block is trapped; m1n1, which already started and initialised every core (chicken bits, GXF), enters the guest on it at EL1 | m1n1 still starts and initialises every core; the EL2 page holds each until the kernel writes its reset-vector register (`pdcpuheld=1`, platform patch 0001), then enters XNU's reset vector at EL1 (`nb06`). A core released through the PMGR block instead would start at EL2 with no chicken bits; that path is not used natively. |
| `cpu-impl-reg + 0x100` run state masked for cores the guest has not started | real values |
| SPRR and GXF enabled before entry; pointer-authentication VM keys; AMX for EL1; `ACTLR_EL12.EnMDSB` | the stub does the same (`nosprr`, `noamx` switch two of them off) |
| Device tree: the hypervisor's USB port removed, product names changed, serial device set to `uart0`, CPUs pruned to the boot CPU | unchanged: `native_boot.native_init` runs the same `setup_adt()` |
| Boot arguments, device tree, RAM disk, trust cache, SEPFW copy, entropy, `kernel-ctrr-to-be-enabled=0` | unchanged: the launcher's own code, same addresses |
| m1n1 stays resident: USB port 0 alive, 1.3 GB below `phys_base` kept from XNU | m1n1 shuts USB down and jumps; the memory below `phys_base` stays unused (later: load over m1n1 as `chainload.py` does) |
| `hv_init`: PCIe shutdown, all secondaries started in m1n1 | not called; PCIe was never initialised, secondaries stay in reset |
| Fault capture, framebuffer capture, guest stop at the end of the capture | the stub's exception line; nothing can stop XNU but a reset |

### Expected failure points, in order
1. No `PDNB stub at EL2` on the UART: the vector, m1n1's teardown or the serial routing.
2. `PDNB EXCEPTION at EL2 vector 0x4` during set-up: a register write this core refuses at EL2 outside the hypervisor's context (`GXF_CONFIG`, `AMX_CTL`, the VM keys); `nosprr`/`noamx` remove two candidates.
3. Silence after `entering EL1 at ..`: XNU faulting after it has set its own (spinning) low vectors, or its UART set-up (baud divisor on the real UART; garbage instead of silence).
4. Banner, then a hang at the first timer wait: FIQ delivery at EL1.
5. Panic with an SError or data abort from a driver: something the hypervisor's stage 2 or traps had been absorbing.
6. Shell reached, challenge not answered: UART input (M5 to M1) under XNU's polled driver.
7. Later, with drivers: anything that depended on m1n1's live USB port, on hypervisor timing, or on the faked PMGR reads.

## Files (`integrations/puredarwin/m1-native-boot/`)
- `stub/el2_drop.S`, `stub/build.sh`, `native_stub.py` (the assembled page; the controller has no assembler).
- `native_boot.py`: mode words, the UART listener, `macvdmtool serial`, the hypervisor-free `init`, the native start and capture, the optional watchdog.
- `launcher-native.patch` (`PCIE_LAB_PATCH`): `m1-boot-speed/launcher-fast-quiet.patch` plus the four call sites in `host_progress.py` and the `vdm-return` case in `tools/m1_bringup_cycle.py`. Without `native_boot.py` and a `native_mode.txt` naming a mode it behaves as the boot-speed patch alone.
- `tools/nb_boot.sh <nbNN> [seconds]`: `NB_MODE=probe|native`, `NB_RETURN=vdm`, `NB_WDT=<s>`, `NB_PROFILE=shell|desktop`, `NB_CPUS`, `NB_KC`, `NB_WORDS`. `guest-shell.in`: the first boots' guest script. `tools/nb_times.py`: seconds from the kernel's start to each console marker.
- `platform-patches/0001-…`, `build-platform.nix`, `build-kc.nix`: the platform kext change for the secondary cores and the collection with it.
- `test_native_boot.py`: host-only (mode words, stub layout and rebuild, call order of the native start, the patch applies and leaves the pinned files alone).

## Boots
| Boot | What | Result |
|---|---|---|
| `nb01` | hypervisor, shell profile, listener on the debug console | shell and challenge as always; 0 bytes on the real UART |
| `nb02` | same, `macvdmtool serial` first | shell and challenge as always; marker and 1714 bytes of m1n1 output on the real UART |

| `nb03` | native | not booted: the controller was off the network when the packet was to be delivered |
| `nb03b` | as `nb02`, after the controller was moved | same as `nb02` (1714 bytes, marker seen) |
| `nb04` | **native**, shell profile, `cpus=1`, `vdm-return vdm-serial` | stub lines, banner, root mounted, launchd, shell prompt 7.27 s after the vector; 44 KB of console; thermal lines at uptime 107 and 167 s; challenge typed but received mangled (`NO_COMMAND_LINE`). Reset sent once against the running kernel, Linux answering 47 s later at the latest. Controller result `RECOVERY_INCOMPLETE_NO_RETRY`: a defect of this stream's controller patch (the 45 s wait and the wait after the reset both recorded a neighbour refresh; the second record failed), fixed. |
| `nb05` | native, shell profile, `cpus=1`, typing paced | `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`: the challenge answered over the real UART, guest script run, `sleep 3` returned after 3.02 s of console time, prompt 7.14 s after the vector. The controller itself waited 45 s, sent the one reset, verified Linux; no latch. PMU: 1 boot error, 0 panics. |
| `nb06` | native, shell profile, **`cpus=8`**, collection `k2w23az4…`, `pdcpuheld=1` | `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`; `hw.activecpu = 8`, idle counters for cpu0..cpu7; prompt 6.08 s after the vector. PMU: 1 boot error, 0 panics. |
| `nb07` | native, **desktop profile**, 8 processors, 30 s session | `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`; GL on KMS 2560x1600, 10 flips, 0 missed; Wi-Fi join, address, ping; Touch Bar ready; coreaudiod; both terminals; `GPUDESK_DESKTOP_RC=0`. First frame 15.49 s after the vector. PMU: 1 boot error, 0 panics. |
| `nb08` | as `nb07`, `noiolog` | same result; first frame 8.67 s. PMU: 1 boot error, 0 panics. |

`nb01`, `nb02`, `nb03b` ended `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`. Linux after `nb04`: `macsmc-reboot: PMU logged 1 boot error(s) and 0 panic(s)`, as after a hypervisor boot.

## Open
- **Reset against a dead kernel.** All five native resets were sent against a running, idle kernel. A panicked or hung native XNU has not been reset; the EL2 page's exception line has never had to print.
- **Session length.** Native sessions so far are at most 30 s of desktop (under 4 minutes of kernel uptime in `nb04`). Nothing can stop a native kernel but the reset; `wdt=` (the SoC watchdog as a dead-man) is implemented and has never been used.
- **Console.** The harness needs the UART; the real system should not print a boot log on it at all. Typing is 320 bytes/s; interactive sessions over the UART (the terminal-input experiments) were not tried.
- **Memory.** XNU still gets only the RAM above m1n1 and its heap (`hw.memsize` 15.05 GB, as under the hypervisor): the layout was kept identical on purpose. Loading over m1n1 (`chainload.py`'s way) would return about 1.3 GB and needs the EL2 page and the held cores moved out of m1n1's memory first.
- **Secondary cores depend on m1n1 having started them.** That is the same contract Linux has with m1n1 (spin-table), and fine for a boot chain with m1n1 in it. Finding m1n1's spin table by pattern is a launcher trick; a boot from disk wants m1n1 to hand the cores over itself.
- **Device tree.** It is still the hypervisor's edit of it: the debug USB port's nodes removed, product names saying "on m1n1 hypervisor". Native USB on that port and honest names are not done.
- **Not run natively:** sleep, core offlining (`cpuctl`), deep idle (`pdidledeep=1`: `CYC_OVRD` now reaches hardware), long soaks, sound output to a listener, the internal NVMe driver (off by rule), anything with `pdnvme=`.
- **The two faked PMGR reads** (`0x23d29c044`, `0x23d29c05c`): no visible effect natively; whether the kernel reads them is still unknown.

## Between this and booting from the internal disk
1. A boot chain that ends in this kernel: m1n1 (as installed, or a stage it chainloads) loading the collection, the device tree edits, the EL2 page and the RAM root or a real root from storage, with no controller. All of what `native_boot.py` does over the proxy has to exist in m1n1 or a payload. Out of this stream by rule (no disk, no boot chain, no boot policy).
2. A root on storage: the NVMe driver is another stream (SF-R; read-only proven under the hypervisor). The kernel's root-wait path sets NVRAM variables and panics after 60 s without a root; that path was kept unreachable here (`rd=md0`).
3. Reboot and shutdown: this kernel cannot restart or power off the machine (Linux does it through the SMC, `macsmc-reboot`; the SoC watchdog is the fallback). Without it every session ends in an external reset and one PMU boot error.
4. The PMU boot-error counter: every native boot left `1 boot error(s) and 0 panic(s)` for the next Linux boot to clear, exactly like a hypervisor boot. A system that boots only this kernel has to clear it itself (Linux: `macsmc-reboot`'s NVMEM cells), or the count grows with every boot; what the firmware does at some count is untested and is the suspected cause of the earlier recovery-mode incident.
5. A console that is not the UART, and a session started by launchd instead of a typed command (SF-D D4).
