## Goal
The SiliconForest kernel on the M1 without the m1n1 hypervisor: m1n1 hands the machine to XNU and is gone (GitHub issue #44). Still loaded into RAM over USB from the controller, still the same desktop, the M1 back in Linux afterwards. No disk, boot chain or boot policy in this stream.

## State
Phase 1 (no native boot yet). The launcher change exists and passes its host tests; the console and the return path are settled as far as they can be without a native boot. **No native boot has been made.** Boots so far: `nb01`, `nb02`, both under the hypervisor.

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
| FIQs routed to EL2 (`FMO=1`) and re-injected: both EL1 timers, fast IPIs, PMC | `FMO=0`, `VM_TMR_FIQ_ENA_EL2=3`: FIQs taken at EL1 directly. **Unproven on this hardware; first suspect if the boot hangs after the banner.** |
| IRQs: passed through (`IMO=0`), AIC untouched | same |
| Trapped system registers (`HACR_EL2`: HID, EHID, ACC, IPI, PM, CTRR, SError info). Seen from this kernel: `OSLAR_EL1` (passed), `ACC_CFG_EL1` read then written with the same value (write dropped), IPI and PMCR0 (passed, FIQ re-injected), `CYC_OVRD` (write dropped) | no traps: all reach hardware. `ACC_CFG` gets the value it already has; `CYC_OVRD` writes take effect (idle is a plain WFI unless `pdidledeep=1`; offline cores are parked, not powered down) |
| SErrors and external aborts trapped (`AMO`, `TEA`) and reported | taken at EL1: a panic on the UART |
| Debug traps (`MDCR_EL2`), breakpoints | none |
| PMGR write hooks: `UART0`, `ATC0_USB`, `ATC0_USB_AON` cannot be switched off | none; the logs show reads of two of them and no write |
| Two PMGR registers whose reads are faked (`0x23d29c044`, `0x23d29c05c`) | real values; whether this kernel reads them is not visible in the logs |
| Secondary cores: the write to the PMGR CPU-start block is trapped; m1n1, which already started and initialised every core (chicken bits, GXF), enters the guest on it at EL1 | **open.** A core released from reset natively starts at its RVBAR at EL2 with no chicken bits. The stub has a reset entry that drops to EL1; `PDAppleBootCPUPMGR` would overwrite RVBAR with XNU's own vector, so SMP needs a kernel change. Native boots are `cpus=1` until then. |
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
- `tools/nb_boot.sh <nbNN> [seconds]`: `NB_MODE=probe|native`, `NB_RETURN=vdm`, `NB_WDT=<s>`, `NB_PROFILE=shell|desktop`. `guest-shell.in`: the first boots' guest script.
- `test_native_boot.py`: host-only (mode words, stub layout and rebuild, call order of the native start, the patch applies and leaves the pinned files alone).

## Boots
| Boot | What | Result |
|---|---|---|
| `nb01` | hypervisor, shell profile, listener on the debug console | shell and challenge as always; 0 bytes on the real UART |
| `nb02` | same, `macvdmtool serial` first | shell and challenge as always; marker and 1714 bytes of m1n1 output on the real UART |

Both ended `DARWIN_COMMAND_LINE_OBSERVED_LINUX_VERIFIED`.

## Open
- The first native boot waits for the owner's decision on the return path (`vdm-return`, `wdt=`, or both).
- Everything in the table marked unproven or open.
