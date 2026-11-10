# m1-gpu-rtkit: Apple mailbox + RTKit on PureDarwin's LinuxKPI layer

Roadmap item F10 (`docs/roadmap/issues/SF-F-gpu-kernel.md`), with the RTKit part of SF-A item A3.
Linux's Apple coprocessor messaging layer (`drivers/soc/apple/mailbox.c`, `rtkit.c`,
`rtkit-crashlog.c`) ported to the FreeBSD-LinuxKPI-on-XNU layer (`src/Kernel/fbsdkpi`) so that
the Rust wrappers `rust/kernel/soc/apple/{rtkit,mailbox}.rs` and the Asahi GPU driver can call
it, plus a test kext, `PDRtkitProbe`, that looks at the GPU coprocessor's ASC (`/arm-io/gfx-asc`
on J293/T8103).

**Status: build evidence only.** Everything here compiles, links into a kext and that kext
links into a kernel collection. None of it has run. Nothing in this directory has touched
hardware, and no statement below about runtime behaviour is an observation.

Upstream: `omacom/linux-aurora` at `6a08a08e57558083f552733a5276d71b1a4d8ff6` (Linux 7.2.11),
as checked out in `sources/gpu-plan/linux-aurora`. The ported files are
`GPL-2.0-only OR MIT`, Copyright The Asahi Linux Contributors; SPDX and copyright lines are
kept. The new files in `src/` carry the same dual licence, the probe is GPL-2.0 like the kexts
it is modelled on.

## Layout

| Path | What |
| --- | --- |
| `src/mailbox.c`, `rtkit.c`, `rtkit-crashlog.c`, `rtkit-internal.h` | the port |
| `src/include/linux/soc/apple/{rtkit,mailbox}.h` | Linux's headers, byte-identical; this is what bindgen reads |
| `src/include/linux/mailbox_client.h` | empty; `rtkit.h` includes it and uses nothing from it |
| `src/pd_rtkit_compat.h`, `pd_rtkit_compat.c` | LinuxKPI gaps (see below), LinuxKPI side |
| `src/pd_rtkit_platform.h` | the platform interface: plain C, no Linux and no xnu types |
| `src/PDRtkitPlatform.cpp` | its IOKit implementation |
| `probe/` | `PDRtkitProbe.kext`: `PDRtkitProbe.cpp` (IOKit class, modes 1 and 2), `probe.c` (mode 2, LinuxKPI side), `glue.c`, `CMakeLists.txt`, `Info.plist` |
| `build-lib.nix` | (a) the C as a static library, compiled with the Rust kext's C flags |
| `build-probe-kext.nix`, `build-probe-kc.nix` | (b) the kext and a kernel collection containing it |
| `build-bindings-check.nix` | what bindgen and the Rust wrappers make of the headers |

## Ported verbatim vs adapted

Lines against upstream (`diff`, added / removed):

| File | Upstream | Here | + | - | Change |
| --- | ---: | ---: | ---: | ---: | --- |
| `include/linux/soc/apple/rtkit.h` | 266 | 266 | 0 | 0 | identical |
| `include/linux/soc/apple/mailbox.h` | 110 | 110 | 0 | 0 | identical |
| `rtkit-crashlog.c` | 237 | 237 | 0 | 0 | identical |
| `rtkit-internal.h` | 86 | 96 | 10 | 0 | includes `pd_rtkit_compat.h`; one private field for `devm_apple_rtkit_free` |
| `rtkit.c` | 1585 | 1623 | 38 | 0 | `devm_apple_rtkit_init`/`_free` without `devm_release_action` (3 `#ifdef` blocks). Every other line, including all protocol code, is untouched |
| `mailbox.c` | 912 | 1194 | 282 | 0 | see below |

No upstream line is deleted or edited; all changes are additions under `#ifdef __PUREDARWIN__`
or wrap upstream code in `#ifndef __PUREDARWIN__`.

`mailbox.c`:

- compiled as upstream: register layout, `apple_mbox_send`, both interrupt handlers,
  `apple_mbox_poll`, `apple_mbox_start`/`stop`/`hold_rx`/`release_rx`, `apple_mbox_quiesce`,
  the ASCWrap v6 lifecycle functions, all `apple_mbox_hw` tables and the `of_device_id` table
  (used to pick the hardware description by Linux compatible string);
- compiled out (213 lines): `apple_mbox_get`, `apple_mbox_get_byname`, `apple_mbox_probe`,
  `apple_mbox_remove`, the `platform_driver` registration, the body of
  `apple_mbox_clear_resume_error`;
- added (269 lines, one block at the end): `apple_mbox_probe_pd` (the probe for a mailbox
  described by the platform interface: same checks and order as `apple_mbox_probe` for a
  non-v6 mailbox), a table of providers by device-tree path, `apple_mbox_get`,
  `apple_mbox_get_byname`, and one new function, `pd_apple_mbox_bind(consumer, index, path)`.

New code: `pd_rtkit_compat.{h,c}` 175 + 163 lines, `pd_rtkit_platform.h` 149,
`PDRtkitPlatform.cpp` 722, probe 480 lines of C/C++.

### Functions

All 36 functions the two headers declare are defined (`build-lib.nix` fails otherwise;
list in `$out/nix-support/api-check`).

| Needed by | Function / type | Here |
| --- | --- | --- |
| `rtkit.rs` | `apple_rtkit_init`, `_boot`, `_wake`, `_shutdown`, `_start_ep`, `_send_message`, `_is_running`, `_is_crashed`, `_has_endpoint`, `_free`, `_free_retaining_buffers` | upstream code |
| `rtkit.rs` | `struct apple_rtkit_ops` (`flags`, `crashed`, `recv_message`, `recv_message_early`, `shmem_setup`, `shmem_destroy`), `struct apple_rtkit_shmem`, `APPLE_RTKIT_COPROC_PLACES_BUFFERS`, opaque `struct apple_rtkit` | upstream header |
| `mailbox.rs` | `apple_mbox_get_byname`, `apple_mbox_start`, `apple_mbox_stop`, `apple_mbox_send`; fields `rx`, `cookie`, `dev` of `struct apple_mbox`; `struct apple_mbox_msg` | upstream code and header; `get_byname` adapted |
| `asahi/gpu.rs` (G13 path) | through `rtkit.rs` only: `RtKit::new(dev, None, 0, data)`, `boot`, `start_endpoint`, `send_message` | covered above |
| `asahi/regs.rs` (G17 path, same file) | `apple_mbox_get`, `apple_mbox_ascwrap_v6_get_lifecycle`, `_require_safe_cpu_lifecycle`, `_start_cpu`, `_stop_cpu`, `enum apple_mbox_ascwrap_v6_role` | defined; on a v4 mailbox they return `-EOPNOTSUPP` as upstream |
| not used by the GPU | `apple_rtkit_idle`, `_quiesce`, `_reinit`, `_poweroff`, `_poll`, `_hold_rx`, `_resume_rx`, `_flush_rx`, `_init_adopted`, `_adopt_running`, `_wake_early_ap`, `devm_apple_rtkit_init`, `devm_apple_rtkit_free`, `apple_mbox_poll`, `_hold_rx`, `_release_rx` | upstream code (`devm_*` adapted) |
| `bindings_helper.h` | `linux/soc/apple/pmp.h` | **not provided.** No `rust/kernel/soc/apple/pmp.rs` exists; only `asahi/g15/` and `m3/` use PMP, neither is built for T8103 |

What the Rust side sees (`build-bindings-check.nix`):

- with `-I src/include` added to lkpi's bindgen step, Linux's `bindings_helper.h` lines for
  `mailbox.h` and `rtkit.h` survive the header reduction (`ok`; `pmp.h` stays `absent`), and
  all 21 `bindings::` names the two wrappers use are generated (`-A check`);
- `soc::apple::mailbox`: no error of its own in lkpi's module matrix;
- `soc::apple::rtkit`: **one** error of its own, `rtkit.rs:192: expected usize, found u64`
  (`bfr_mut.iova = iova as u64`). The layer's `dma_addr_t` is `vm_paddr_t` = `phys_addr_t` =
  Haiku's `unsigned long` type, which bindgen gives Rust as `usize`; Linux's is `u64`. Same
  size and layout, different Rust type. With `pub type dma_addr_t = u64;` in the generated
  bindings the error is gone (`-A matrix`; `-A matrixAsIs` records the error). So the wrapper
  compiles unmodified only once the Rust kext's bindgen setup maps `dma_addr_t` to `u64`
  (a bindgen parameter or a layer typedef: a decision for `m1-rust-kext`, where every other
  user of `dma_addr_t` has the same question). The header here is left as Linux has it;
- both targets stay "blocked" in the matrix by the modules they pull in (`device`,
  `device::property`, `devres`: 40 errors, none in these two files). rustc does not finish
  type checking while those fail, so "no error of its own" is a statement about names and
  the types rustc got to, not a full compile.

## The platform interface (`src/pd_rtkit_platform.h`)

| Call | Linux counterpart | IOKit implementation |
| --- | --- | --- |
| `pd_rtkit_platform_init(host)` | | remembers the kext's IOService, makes one IOWorkLoop |
| `pd_rtkit_platform_mbox_resolve(path, &res)` | `platform_get_resource`, `platform_get_irq_byname` | `IORegistryEntry::fromPath(path, gIODTPlane)`; requires `iop,ascwrap-v4`; `IODTResolveAddressing` for reg 0 (once per node, array never released); mailbox = reg 0 + 0x8000, 0x4000 long (Linux `mbox@206408000`); reads the 4 `interrupts`; creates a private `IOService` carrying them as interrupt sources 0..3 (controller `gIODTDefaultInterruptController`, specifier = AIC number, the way `PDApcieT8103Host::setupMSI` describes MSI vectors) |
| `pd_rtkit_platform_mbox_for_consumer(node, index, path, len)` | `of_parse_phandle_with_args(np, "mboxes", ...)` | interim rule, below |
| `pd_rtkit_platform_ioremap` / `_iounmap` | `devm_platform_ioremap_resource` | `IODeviceMemory::withRange` + `map(kIOMapInhibitCache)`: non-posted |
| `pd_rtkit_platform_irq_request/free/enable/disable/disable_nosync/synchronize/stats` | `devm_request_irq(IRQF_NO_AUTOEN)`, `enable_irq`, `disable_irq`, `disable_irq_nosync`, `synchronize_irq` | below |
| `pd_rtkit_platform_power_state(path, &raw)` | genpd state | T8103 only: one read of PMGR `ps_gfx` (`0x23b7003f8`), after checking `/arm-io` is `arm-io,t8103` and `/arm-io/pmgr` reg 0 is `0x23b700000`. Read-only |
| `pd_rtkit_platform_log`, `_set_log_prefix`, `_bootarg` | `printk` | `IOLog`, `PE_parse_boot_argn` |

Interrupt roles. Apple's tree does not name the four lines and lists them out of order
(`gfx-asc`: 576 575 578 577). Linux's `t8103.dtsi` gives `agx_mbox` 575..578 as send-empty,
send-not-empty, recv-empty, recv-not-empty. The implementation requires four consecutive
numbers and assigns the roles in ascending order. Only send-empty and recv-not-empty are
used, as on Linux.

Interrupt delivery. A handle starts masked (Linux `IRQF_NO_AUTOEN`) with a disable depth of 1.
The primary handler masks the line at the controller (`disableInterrupt`) and signals a
provider-less `IOInterruptEventSource`; the Linux handler runs on the work loop thread, never
at interrupt level (LinuxKPI spinlocks are sleeping mutexes here), and the line is unmasked
afterwards only if the Linux side still has it enabled. That last condition is why a plain
`IOInterruptEventSource` on the line cannot be used: it unmasks a level interrupt by itself
after the action, and the mailbox's send-empty line is asserted whenever the FIFO is empty and
is disabled by its own handler.

`pdrtkit_irq=<n>` selects how a line is unmasked again:

- `2` (default): after a delivered interrupt, `unregisterInterrupt` + `registerInterrupt` +
  `enableInterrupt`. Reason, from reading `PDArmPlatformExpert/PDAppleAIC.cpp` and
  `IOInterruptController.cpp`: `handleInterrupt` reads the AIC event register, which acks and
  masks the line in hardware, and calls the handler without setting `interruptDisabledHard`;
  `IOInterruptController::enableInterrupt` writes the unmask only when that flag is set. Read
  that way a delivered level interrupt is never unmasked again by `enableInterrupt` alone. A
  fresh registration starts hard-disabled, and enabling it does write the unmask. This is an
  inference from source, not an observation; if the controller is fixed, mode 2 still works.
- `1`: `enableInterrupt` only (correct for a controller without that problem).
- `0`: no AIC interrupt at all. recv-not-empty becomes a 2 ms timer on the work loop, the
  mailbox is told there is no send-empty line and `apple_mbox_send` polls, as upstream does
  for a mailbox without one.

## Depends on other work; interim choices

| Needs | From | Interim here |
| --- | --- | --- |
| a `struct device` for the GPU with a usable `of_node` | Rust kext work (`m1-rust-kext`), device model | `apple_mbox_get(dev, 0)` works for any `dev`: an explicit `pd_apple_mbox_bind(dev, 0, "/arm-io/gfx-asc")` wins; else a consumer whose node is an `iop,ascwrap-v4` is its own mailbox; else `/arm-io/sgx` or **no node at all** means `/arm-io/gfx-asc` (logged). The fallback for a node-less device is a guess that is only right for the GPU |
| `mboxes = <&phandle>` on the synthesised GPU node | `m1-gpu-dt` leaves `mbox_phandle` to "whoever builds the tree" | not consumed. The mailbox is found by path, not by phandle. If a synthesised tree becomes visible to the layer's `of_*`, `apple_mbox_get` should try `of_parse_phandle_with_args` first |
| `pd_rtkit_platform_init(IOService *)` called before the first `apple_rtkit_init` | the GPU kext's IOKit class | the probe does it; nothing else does |
| `dma_addr_t` as `u64` in the Rust bindings | `m1-rust-kext/lkpi` (bindgen parameters) | none; documented above |
| GFX power domain on | SF-A item A2 (PMGR) | read-only state check; nothing here turns it on |
| level interrupts unmasked again by `PDAppleAIC` | platform (`PDArmPlatformExpert`) | `pdrtkit_irq=2` re-registration; `pdrtkit_irq=0` polling |
| UAT page tables, kernel VM, GEM | GPU driver | none; the RTKit core calls `ops->shmem_setup` as upstream |
| `linux/soc/apple/pmp.h` | only if G15/M3 paths are ever built | not provided |

## LinuxKPI gaps hit

Evidence is in the layer tree the Rust kext is built from (`lkpi.tree`,
`src/Kernel/fbsdkpi/...`); `lk/` below is `freebsd/linuxkpi/common/include/linux/`.

| # | Linux API | In the layer | Handling |
| --- | --- | --- | --- |
| 1 | `<linux/mailbox_client.h>` (mbox framework) | no such header | empty header in `src/include`. Not needed: this RTKit calls `apple_mbox_*` directly, not the generic framework |
| 2 | `enable_irq`, `disable_irq` | `linux_interrupt.c:179` `lkpi_enable_irq`: `-EINVAL` unless the number belongs to a PCI device; `lkpi_disable_irq` likewise does nothing | platform interface; `#define` in `pd_rtkit_compat.h` |
| 3 | `disable_irq_nosync` | `lk/interrupt.h:107`: same as `disable_irq` | platform interface |
| 4 | `synchronize_irq` | `lk/hardirq.h:40` -> `_intr_drain`, empty (`pd/lkpi/pd_lkpi_libkern.c:349`) | platform interface (waits on the work loop gate) |
| 5 | `IRQF_NO_AUTOEN`, `IRQF_NO_SUSPEND` | not defined (`lk/interrupt.h` has `IRQF_SHARED`, `IRQF_NOBALANCING`); a device-tree interrupt is enabled as soon as it is installed (`xnu/pd_fbsd_os.cpp:1532`) | handles start masked |
| 6 | `platform_get_irq_byname("recv-not-empty")`, `platform_get_irq_byname_optional` | the first reads `interrupt-names` (`pd/lkpi/pd_linux_platform.c:152`), which Apple's tree lacks; the second does not exist | roles from the AIC numbers |
| 7 | `platform_driver` probe via `of_match_table` | exists (`pd_linux_platform.c`), matched against Apple's compatible strings by an IOKit class per kext; Apple's node is `iop,ascwrap-v4`, not `apple,asc-mailbox-v4`, has no `reg-names`, and covers the whole ASC, not the mailbox | provider created on first `apple_mbox_get`; Linux probe compiled out |
| 8 | `of_parse_phandle_with_args(np, "mboxes", "#mbox-cells")`, `of_find_device_by_node`, `device_link_add`, `DL_FLAG_AUTOREMOVE_CONSUMER` | first two exist, but Apple's tree has no `mboxes`; device links do not exist | lookup by path; no device link (a provider is never torn down) |
| 9 | `pm_runtime_resume_and_get` | `lk/pm_runtime.h:158`: returns **1** (Linux: 0 or negative errno); `mailbox.c` treats non-zero as failure | wrapper returning 0 for >= 0 |
| 10 | `pm_runtime_set_suspended`, `pm_runtime_barrier`, `devm_pm_runtime_enable`, `dev->power.runtime_error` | absent (`lk/pm.h:50`: `struct dev_pm_info { usage_count; can_wakeup; }`) | no-ops; the one use of `runtime_error` compiled out. No runtime PM: the mailbox never powers anything |
| 11 | `readl_poll_timeout_atomic` | `lk/iopoll.h` has `read_poll_timeout_atomic` only | defined on top of it |
| 12 | `dma_rmb`, `dma_wmb` | absent | `rmb()`, `wmb()` |
| 13 | `alloc_ordered_workqueue(fmt, flags, args...)` | `lk/workqueue.h:165`: two-argument macro, flags dropped | redefined; `WQ_HIGHPRI \| WQ_MEM_RECLAIM` have no effect, the queue is one thread in order |
| 14 | `kfifo_put`/`kfifo_get` lock-free for one producer and one consumer; `kfifo_reset` | `lk/kfifo.h:91,109`: both sides modify a shared `count`; no `kfifo_reset` | replaced by index-only versions (63 of 64 slots usable). Only used with `apple,sync-syslog-ack`, which the GPU does not have |
| 15 | `devm_release_action` | absent (`devm_add_action[_or_reset]` exist) | `devm_apple_rtkit_free` empties a cell the action reads |
| 16 | `of_machine_is_compatible` | absent | `"apple,tXXXX"` asked of the tree as `arm-io,tXXXX` |
| 17 | `dev_err`/`dev_warn`/`dev_info` | `lk/device.h:215`: `device_printf(dev->bsddev, ...)`, a NULL dereference for a device without a newbus device | own printer using the kobject name |
| 18 | `dev_dbg` | `lk/device.h:219`: empty | prints when `pd_rtkit_debug` is set (boot-arg `pdrtkit_debug=1`; the probe sets it) |
| 19 | `MODULE_DEVICE_TABLE(of, ...)`, `module_platform_driver` | the first does not compile | compiled out with the Linux probe |
| 20 | `dma_alloc_coherent(rtk->dev, ...)` (RTKit buffers of a coprocessor without `shmem_setup`) | `linux_pci.c:2045`: NULL unless `dev->dma_priv` is set up; default mask is 32 bit (`:2062`) and RAM on this SoC starts at `0x8_0000_0000`; no DART, so the handle is the physical address | code left as upstream. Not used by the GPU (driver-owned buffers through `shmem_setup`). For another coprocessor it only works if that coprocessor takes physical addresses and the consumer set a 64-bit mask; a DART-backed one needs SF-A's DART first |
| 21 | `dma_addr_t` = `u64` | `unsigned long` (via `vm_paddr_t`) | none here; Rust-side decision, see above |
| 22 | hard-IRQ context, real spinlocks | handlers run on a thread, `spinlock_t` is a sleeping mutex | none needed. Consequences: `apple_mbox_send(..., atomic = true)` and `recv_message_early` are not atomic/IRQ context here, and a caller in a genuinely atomic context cannot use them |
| 23 | `current` on any thread | a thread entering from IOKit must call `linux_set_current(curthread)` | done in the interrupt thunk and the probe (`pd_rtkit_enter()`); the GPU kext's entry points need the same |

Present and used as is: `linux/completion.h` (`wait_for_completion_interruptible_timeout`
returns remaining jiffies / 0 / negative as Linux), `queue_work`, `flush_workqueue`,
`cancel_work_sync`, `system_highpri_wq`, `kzalloc_obj`, `__free(kfree)`, `readq_relaxed`,
`writeq_relaxed`, `memcpy_fromio`, `of_property_read_bool`, `of_property_match_string`,
`devm_add_action_or_reset`, `check_mul_overflow`, `dev_*_ratelimited`.

The layer's `ioremap()` maps `kIOMapInhibitCache` today (`pd/lkpi/pd_linux_page.c:242`), which
is the non-posted mapping these registers need. The port does not use it for the mailbox, so
that a later change to the layer's default cannot silently turn it into a posted mapping.

## Build

```sh
cd integrations/puredarwin/m1-gpu-rtkit
NB="$HOME/.nix-profile/bin/nix-build --extra-experimental-features nix-command --extra-experimental-features flakes"
$NB build-lib.nix                          # (a) libpdrtkit.a + nix-support/{commands,defined,undefined,api-check}
$NB build-probe-kext.nix                   # (b) PDRtkitProbe.kext + nix-support/{build-commands,objects,undefined}
$NB build-probe-kc.nix                     # (b) kernel collection: m1-rust-kext/build-kc.nix + PDRtkitProbe
$NB build-bindings-check.nix -A check      # bindgen names
$NB build-bindings-check.nix -A matrix     # Rust wrappers, dma_addr_t = u64
$NB build-bindings-check.nix -A matrixAsIs # Rust wrappers, bindings as generated (records the one error)
```

`build-lib.nix` uses `lkpi.cflags.cc` unchanged (the list the Rust kext's helper objects and
bindgen use) plus `-I src/include -I src -O2`. `build-probe-kext.nix` fails if `rtkit.c` is
compiled by cmake with a different `-D` set than the bindings were generated with (30 flags
compared). The kernel collection log lists unresolved symbols per kext; for `PDRtkitProbe`
they are `dyld_stub_binder` and `_pd_usb_module`, `_pd_usb_register`, `_pd_usb_unregister`,
the same four `PDRustLKPI` has (the layer's USB references in a kext built without USB).

These recipes import `../m1-rust-kext` (pins, `lkpi`, `build-kc.nix`); when that directory
changes, the store paths change with it.

## The probe

`PDRtkitProbe` matches `IOResources` and runs once at start. Every line it logs starts with
`PDRtkitProbe: `.

| Boot-arg | Effect |
| --- | --- |
| `pdrtkitprobe=0` | nothing |
| none, or `pdrtkitprobe=1` | read-only probe |
| `pdrtkitprobe=2` | read-only probe, then the RTKit boot attempt |
| `pdrtkitprobe=3` | DCP mode (SF-H item H1), none of the GPU steps; see "DCP mode" below |
| `pdrtkit_irq=0\|1\|2` | interrupt mode, see above (mode 2 only) |

To boot it: use the `kernel` from `build-probe-kc.nix` in place of the phase A collection in
the existing boot path. That is not done here.

### Mode 1 (read-only)

Device-tree description; one PMGR read; and, only if the GFX domain reads as powered, four
ASC reads: `CPU_CONTROL` (+0x44), `CPU_STATUS` (+0x48), mailbox `A2I_CONTROL` (+0x8110),
`I2A_CONTROL` (+0x8114). No write, no interrupt registration, no message. The FIFO data
registers are never read (reading `I2A_RECV` would take a message out).

Predicted log, with this machine's device tree (`original-adt.bin`, SHA-256 `51de4135...`).
`x` digits and the words in `{}` are hardware state and are not predicted:

```
PDRtkitProbe: start, mode 1 (read-only: no register write, no interrupt, no message)
PDRtkitProbe: /arm-io/gfx-asc: iop,ascwrap-v4, reg 0 0x206400000 len 0x6c000, mailbox (Linux apple,asc-mailbox-v4) 0x206408000 len 0x4000
PDRtkitProbe: /arm-io/gfx-asc: AIC interrupts listed 576 575 578 577: send-empty 575, send-not-empty 576, recv-empty 577, recv-not-empty 578
PDRtkitProbe: PMGR GFX power state 0xxxxxxxxx: target 0xx actual 0xx ({powered|NOT powered})
```

then, if the domain is powered (ACTUAL = 0xf):

```
PDRtkitProbe: state: ASC CPU_CONTROL 0xxxxxxxxx (RUN {0|1}) CPU_STATUS 0xxxxxxxxx ({running|not running}{, stopped}{, idle})
PDRtkitProbe: state: mailbox A2I_CONTROL 0xxxxxxxxx ({empty|full|not empty}{, overflow}, count x) I2A_CONTROL 0xxxxxxxxx ({empty|full|not empty}{, overflow}, count x)
PDRtkitProbe: state: coprocessor {started (RUN set)|not started (RUN clear)}; AP->IOP FIFO {empty|holds messages}, IOP->AP FIFO {empty|holds messages}
PDRtkitProbe: mode 1 done: 5 register reads (1 PMGR, 4 ASC), 0 writes, 0 interrupts registered, 0 messages
```

or, if it is not:

```
PDRtkitProbe: GFX domain is not powered: ASC registers NOT read. Stopping (1 register reads, 0 writes)
```

What the state is likely to be (not a prediction of the log): neither iBoot nor m1n1 starts
the GPU coprocessor, so `RUN 0` and both FIFOs empty would be unsurprising; whether the GFX
domain is still powered when this kernel runs is not known from source.

### Mode 2 (RTKit boot attempt)

Order, with the Linux call each step stands for:

1. device tree, PMGR state, register state: as mode 1. Stops if GFX is not powered.
2. firmware memory check: the handoff, page-table and shared regions (`/arm-io/sgx`
   `gfx-handoff-base`, `gpu-region-base`, `gfx-shared-region-base`) and both firmware segments
   (`gfx-asc` `segment-ranges`) must lie outside `[physBase, physBase + memSize)` of this
   kernel's boot-args. Stops, coprocessor not started, if any does not or is not described.
3. `CPU_CONTROL |= RUN` (`regs.rs` `Resources::start_cpu`).
4. `apple_rtkit_init(dev, cookie, NULL, 0, ops)` (`rtkit.rs` `RtKit::new`): mailbox
   interrupts registered, receive armed.
5. `apple_rtkit_boot(rtk)` (`gpu.rs` `init()`): HELLO, endpoint map, system endpoints
   started, wait for the IOP power ack, AP power ON.
6. stop. Endpoints 0x20/0x21 are not started, no initdata is sent. The instance stays
   attached so that syslog traffic is acknowledged and a crash is reported.

`ops` are what `rtkit.rs` builds for the G13 `GpuManager` (flags 0, all five callbacks), except
that `shmem_setup` refuses: a buffer request is logged and left unanswered.

**What a bare RTKit boot of gfx-asc lacks, compared with Linux**

| Linux, before or during `rtk.boot()` | Here |
| --- | --- |
| `power-domains = <&ps_gfx>`: genpd powers the domain before probe | not done. Read-only check; if off, mode 2 stops before any ASC access |
| `Uat::new` between `start_cpu` and `RtKit::new` (`mmu.rs`): writes `magic_ap = 0x4b1d000000000002`, `cur_slot`, waits up to 1 s for the firmware's `magic_fw` in the handoff region, zeroes the flush slots, builds the kernel VM on the `pagetables` region and stores `ttb0`/`ttb1 \| VALID` in TTBAT slot 0 | none of it. The handoff and TTBAT are left exactly as iBoot left them |
| `shmem_setup` -> `shmem_alloc`: a GEM object mapped into the firmware's kernel address space (`IOVA_KERN_RTKIT_RANGE`) through UAT, its VA returned to the firmware for the syslog/crashlog/ioreport/oslog buffer requests | refused. There is no address that could honestly be given: the firmware would use it as a virtual address in page tables this kernel has not built |
| m1n1 hands Linux the UAT regions as no-map reserved memory | checked against the boot-args memory range only (step 2) |
| initdata, endpoints 0x20/0x21, `MSG_INIT` | deliberately not done |

**Can mode 2 reach "ready"?** Not expected, and it cannot be decided from source.

- Up to "all system endpoints started" (the endpoint map acknowledged, `boot_result == 0`):
  plausible. HELLO, the endpoint map and endpoint starts need no shared memory. It still
  assumes the firmware gets as far as RTKit's management endpoint without the AP half of the
  UAT handoff, which Linux and m1n1 (`hw/uat.py`, `fw/agx/handoff.py`: "Waiting for FW PPL
  init") never test, because both write `magic_ap` first.
- "Ready" in `apple_rtkit_boot`'s sense needs the firmware's IOP power ack (0x20). Whether
  this firmware sends it while its buffer requests are unanswered is unknown. If it does not,
  the log ends with `RTKit: IOP power acknowledgment failed` and `apple_rtkit_boot returned`
  a timeout after 1 s. If it does, AP power ON is sent and the probe reports ready, with a
  firmware that has no log buffers.
- The last step mode 2 takes is therefore the end of `apple_rtkit_boot`. It never hands the
  firmware an address and never starts a GPU endpoint. What it cannot undo is step 3: once
  RUN is set the firmware runs until reset, and Linux has no stop for this ASC either
  (shutdown is an RTKit power-state message, which needs a completed boot). A boot with
  `pdrtkitprobe=2` must not also load a GPU driver.

If mode 2 times out at the endpoint map, the last lines tell the cases apart: `I2A_CONTROL`
not empty with 0 interrupts taken means a message is waiting and the interrupt did not arrive
(try `pdrtkit_irq=0`); empty with RUN set means the firmware did not speak.

## DCP mode (`pdrtkitprobe=3`, SF-H item H1)

Attaches the port to the display coprocessor (`/arm-io/dcp`) in whatever state the boot chain
left it, wakes its RTKit the way Linux does, logs the endpoint map and stops. Build evidence
only, like the rest of this directory. Lines start with `PDRtkitProbe: dcp`.

```sh
$NB build-dcp-probe-kc.nix     # ../m1-apcie-host/build-kc.nix + PDRtkitProbe.kext; boot-args pdrtkitprobe=3
```

The collection needs no other kext for the probe: it links its own hidden copy of the Linux
layer and the port, and the recipe fails on any unresolved symbol beyond the layer's four
known ones or if fewer than 4 pages stay free before the kernel's `__TEXT`. The GPU kexts are
left out on purpose.

A read-only look from the m1n1 proxy before the guest starts is in
`../m1-apcie-bridge/proxy_linkup.py` (`dcp_probe`, mode file `dcp_probe_mode.txt` = `dcp`):
PMGR state of the display domains and, if `DISP0_CPU0` is powered, ASC `CPU_CONTROL`,
`CPU_STATUS` and the two mailbox control registers; `dart` adds DART register reads.

### What Linux does (`/home/chrisk/source/aurora-linux`, `e4735882`)

| Step | Where | Panel / DART |
| --- | --- | --- |
| genpd powers `ps_disp0_cpu0` (parent `ps_disp0_fe`) for `dcp`, `dcp_mbox`, `dcp_dart`, `disp0_dart` | `t8103.dtsi:631,643,657,678`, `t8103-pmgr.dtsi:1006` | already on when the panel is lit; a power cycle would blank it |
| DART drivers attach `dcp` (stream 0 of `dcp_dart`) and the `piodma` child (stream 4 of `disp0_dart`); m1n1's reserved-memory entries are mapped again | `t8103.dtsi:665,687`, `dcp.c:2165-2196`, `apple-dart.c:942` | rewrites live translation state; **needs DART**; not done here |
| `dma_set_mask_and_coherent`, map `coproc`, panel properties, display registers, clock | `dcp.c:2455-2522` | no hardware effect |
| `CPU_CONTROL \|= RUN` | `dcp.c:2533-2536` | no change when RUN is already set |
| `devm_apple_rtkit_init(dev, dcp, "mbox", 0, &rtkit_ops)` | `dcp.c:2538` | mailbox interrupts on; nothing sent |
| `apple_rtkit_wake`: IOP power state 0x220, then HELLO, endpoint map, start of the system endpoints, IOP power ack, AP power 0x20 | `dcp.c:2543`, `rtkit.c:1110-1130`, `:118`, `:155`, `:198`, `:1018` | management endpoint only |
| during that, buffer requests on crashlog/syslog/ioreport/oslog go to `dcp_rtk_shmem_setup` | `rtkit.c:261-340`, `dcp.c:1416-1454` | with an iova (firmware-placed): `iommu_iova_to_phys` + `memremap`, **no reply** (`rtkit.c:324`); without: `dma_alloc_coherent` through `dcp_dart` and a reply. **Needs DART** either to read or to map |
| later, from the DRM driver: `dcp_start`: system endpoint 0x20, on some machines iBoot 0x23 and DPTX 0x2a, then IOMFB 0x37 | `dcp.c:1883-1953` | **this is where the panel can change**; not done here |

Minimum sequence to "endpoint map received": the two rows `devm_apple_rtkit_init` and
`apple_rtkit_wake`, nothing before them when the domain is on and RUN is set, nothing after.
`apple_rtkit_wake` goes past the map (it starts the system endpoints and waits for the power
acks); the port's `rtkit.c` is used unmodified, so the probe runs it whole.

### What state the DCP is in on this machine

Not the one the research assumed. In the saved console of
`runs/m1-desktop-010/pcie-enum-attempt-pd02` m1n1 does not take the "already initialized"
exit of `display_init()` (`src/display.c:777`); it takes the power-cycle path (`:771-775`):

```
display: Internal display on t8103 or t8112 with Sequoia SFW, power cycling
dart: dart /arm-io/dart-dcp at 0x23130c000 is a t8020 (locked)
dart: dart /arm-io/dart-disp0 at 0x231304000 is a t8020
rtkit(dcp): booting with version 12
rtkit(dcp): unknown system endpoint 0x0a
rtkit(dcp): unknown oslog message 103ff8000f0025f
display: Modeset took 911 ms
display: Quiescing DCP (internal)
```

So m1n1 itself boots the DCP's RTKit, sets the mode through the iBoot endpoint, and then
quiesces it (`display_shutdown` -> `dcp_shutdown(dcp, false)` -> `rtkit_quiesce`,
`src/display.c:805-809`, `src/dcp.c:309`, `src/rtkit.c:786-851`: AP power 0x10, then IOP
power 0x10), frees its RTKit buffers and unmaps them from `dart-dcp`
(`src/dcp.c:319-322`; `dart_shutdown` leaves a locked DART's registers alone,
`src/dart.c:1089`). The hypervisor path (`proxyclient/m1n1/hv/__init__.py`) does not touch
the DCP. The kernel therefore meets a DCP that is running, quiesced, at RTKit protocol 12,
with `dart-dcp` reported locked and `dart-disp0` not. Quiesced-then-`apple_rtkit_wake` is
the same hand-over Linux gets from m1n1 on this machine.

That log also says what to expect: a system endpoint 0x0a, and an oslog buffer request
`0x0103ff8000f0025f`, which in Linux's field layout is size 0x3ff8 at iova 0xf0025f000:
placed by the firmware, no reply owed. Whether crashlog/syslog/ioreport are firmware-placed
or need an allocation is not visible there (m1n1 prints nothing for either).

### What the probe does

| Stage (`PDRtkitProbe: dcp <stage>:`) | Action |
| --- | --- |
| `dt` | `/arm-io/dcp` from the device tree: ASC window, mailbox = reg 0 + 0x8000, the four AIC numbers. No access |
| `power` | two PMGR reads, `DISP0_FE` and `DISP0_CPU0` (T8103 addresses, checked against `/arm-io` and the PMGR node). Stops unless `DISP0_CPU0` is powered |
| `regs before` | four ASC reads. Stops unless `CPU_CONTROL.RUN` is set: the probe never starts the coprocessor |
| `cpu` | Linux's RUN write is skipped (it would write the same value) |
| `rtkit-init` | `apple_rtkit_init`; then a trace function is put in front of RTKit's receive function |
| `wake` | `apple_rtkit_wake` |
| `rx`, `hello`, `endpoint`, `endpoint-map`, `buffer-request`, `power` | from the trace: every received message (first 96 in full), every announced endpoint with its name, every buffer request with endpoint, size and iova |
| `endpoints`, `summary`, `regs after`, `irq`, `done` | results |

Buffer requests are never answered and nothing is mapped: `shmem_setup` returns an error. For
a firmware-placed buffer that is what Linux puts on the wire too (nothing). For a request
without an iova the firmware does not get the reply Linux would send; the `buffer-request`
line then ends in `NOT answered (needs H2)` and is the specification for H2. Neither DART is
read or written. Afterwards the coprocessor is left as it is: not quiesced, not put to sleep,
the RTKit instance attached.

Messages the probe causes to be sent, all by the unmodified `rtkit.c`: IOP power state 0x220,
HELLO reply, endpoint-map replies, one start message per system endpoint in the map
(below 0x20), AP power state 0x20 if the IOP ack arrives, and acknowledgments of syslog and
ioreport notifications.

## Unverified

Everything at runtime. In particular:

1. interrupt delivery through `PDAppleAIC` for a level line, in any of the three modes; the
   AIC analysis above is from source;
2. that `gIODTDefaultInterruptController` is the controller of these four lines (the node's
   `interrupt-parent` is not compared), and that a bare `IOService` attached to the host can
   register them;
3. the role assignment by ascending AIC number, beyond `gfx-asc` on T8103 where it matches
   Linux's device tree;
4. the PMGR `ps_gfx` address and field layout (taken from Linux's device tree and
   `m1-gpu-preflight`), and that ACTUAL = 0xf is sufficient for the ASC window to be readable;
5. that the firmware's memory is outside what this kernel uses whenever it is outside the
   boot-args range (step 2 is a necessary check, not a proof);
6. the layer primitives the port relies on, under real concurrency: completions, the ordered
   workqueue, sleeping "spinlocks" taken from the interrupt thread;
7. the replaced kfifo, the `devm` variant, `apple_mbox_get_byname` by node name: compiled,
   never executed;
8. the Rust wrappers beyond what rustc reaches while `device`/`devres` do not compile.
