# SF-AD — AVE encoder source-port study

**Offline study, 14 November 2026; DPE clarification updated15 November. No AVE kernel build, AVE device access, firmware
extraction, installation or encoder execution was performed.** The first target
is the T8103/J293 M1. Keep the working PDAppleAVD decoder and audio stack.

The reference is aquarat's GPL-2.0-only
[apple-m-series-ave-driver, pinned at 9ab14e45281e8f61fb499b760408028171a5f2b5](https://github.com/aquarat/apple-m-series-ave-driver/tree/9ab14e45281e8f61fb499b760408028171a5f2b5).
It is cloned, git-ignored, under `sources/ref-ave` with a sparse source/document
checkout. No firmware, kernelcache, RAM dump or derived-data directory was
checked out; no firmware-fetch/generation tool was run. Upstream documentation
is CC BY-SA 4.0; this report cites it and the code without copying its prose or
tables. Its LGPL FFmpeg patches are separate from the GPL driver.

## Decision and first scope

The session/command code is a useful source-port base. It is **not ready to run
under the owner's “iBoot-preloaded firmware only” rule**. The current Linux path
still opens a firmware Mach-O file, and its enabled DPE initialization loads
AppleAVE2-derived register tables. Those inputs must be removed or independently
replaced before an authorized hardware attempt. A Linux build alone would not
settle either problem.

Start with one T8103 instance, H.264, NV12, one reference, no B frames, fixed QP,
explicit dimensions/strides and a bounded synchronous request. Upstream records
H.264 B/two-reference hangs on this SoC; its source clamps that configuration.
Do not enable HEVC, P010, reordering, multipass, multiple clients, suspend or
warm recovery merely because their fields exist. The older M1 results in
[docs/89](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/docs/89-t8103-m1.md)
are upstream evidence, not SiliconForest evidence. Later ADT/cold-DATA changes
explicitly say their current hardware runs remain outstanding in
[docs/99](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/docs/99-adt-placement.md)
and [docs/100](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/docs/100-no-apple-data.md).

## Source structure and PureDarwin mapping

The driver directory is about 24k lines, including roughly 3.4k lines of wire
ABI headers. `ave_session.c` alone is about 5.8k lines. Porting is substantially
more than translating the V4L2 entry points.

| Upstream piece | Role | SiliconForest mapping / required work |
|---|---|---|
| `ave_soc.c/.h`, `ave_adt.c/.h`, `ave_version.c` | SoC banks, power topology, live firmware placement and exact firmware ABI selection | Read the **live** `/arm-io/ave` and `dart-ave` through IORegistry/ADT. Validate J293's actual segments; never adopt the J313/J274 physical addresses in the reference row. Keep version rejection. The selector recognizes 13.5 and 26.6/26.6.2, but T8103's mapped image/DATA facts and demonstrated path are 13.5 H13G. Controller macOS version does not identify the M1's stub firmware. |
| `ave_drv.c`, `ave_hw.h` | Probe/start/stop, power, interrupts, DPE initialization | Follow `m1-avd/kernel/PDAppleAVD.cpp`: IOKit service shell, MMIO descriptors, interrupt event sources, locks, bounded waits, `pd_lfile` device. Use `pd_soc.cpp`'s `pd_pmgr_enable_node`/reset/state helpers with AVE's own domains and ordering. Both T8103 ME domains matter; AVD gate IDs are not substitutes. T8103's row has no PMP vote. |
| `ave_fw.c` | Image adoption/copy, TEXT validation, DATA handling, DMA mapping | Replace file-based `request_firmware()` paths with a validated, reserved iBoot-preload handoff. This is a new adapter, not PDAppleAVD's firmware-file reader. Preserve TEXT and cold DATA before a general allocator can overwrite them. |
| `ave_dapf.c`, `ave_smmu.c` | CPUDART/DAPF permissions, stream diagnostics, observed SMMU faults | Reuse DART page-table/IOBufferMemoryDescriptor primitives from `pd_avd_dart.cpp` and sound's fixed DART adapter, **not their address-space policy**. AVD allocates from 64MiB in a 2GiB window; AVE needs fixed firmware/DATA IOVAs, device-specific streams and DAPF translations. SMMU code is an optional observer, not another generic DART reset target. |
| `ave_ipc.c`, `ave_abi_boot.h`, `ave_abi.h` | Boot exchange, FwIPC channels, phase-bit rings and wire layouts | AVE uses eight scratch registers/doorbell and shared rings. It explicitly **does not use RTKit endpoint messaging**. Existing RTKit infrastructure supplies patterns for DMA ownership, interrupts, cache barriers and waits; its HELLO/endpoint transport cannot be reused as AVE's protocol. Implement the actual AVE handshake/ring state machine. |
| `ave_cmd.c/.h`, `ave_session.c/.h`, `ave_gop.h` | Commands, work-surface sizing, session lifecycle, frame submissions and replies | Retain GPL provenance and the protocol packing; replace Linux allocations, work queues/completions and timeout mechanics through a narrow OS interface. Validate every firmware reply before using offsets or lengths. Keep GOP policy minimal initially. |
| `ave_v4l2.c/.h` | vb2/m2m queues, controls, mmap/DMABUF, Linux format enumeration | Replace with `/dev/pdave`; do not emulate V4L2 in XNU. Keep policy, sizes and state transitions that are independent of vb2. A future userland `libpdave` feeds VTCompressionSession; do not put GPL session bodies in an LGPL FFmpeg userland module. |

These mappings are code observations from the
[upstream driver](https://github.com/aquarat/apple-m-series-ave-driver/tree/9ab14e45281e8f61fb499b760408028171a5f2b5/driver),
particularly [the non-RTKit transport](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/driver/ave_ipc.c),
and our existing [AVD kernel shell](../../../integrations/puredarwin/m1-avd/kernel/PDAppleAVD.cpp),
[DART allocator](../../../integrations/puredarwin/m1-avd/kernel/pd_avd_dart.h)
and [PMGR interface](../../../integrations/puredarwin/m1-sound/kernel/pd_soc.h).
They do not establish that AVE DMA, interrupts or firmware run on our M1.

## Apple-derived inputs: exact requirements and possible replacements

### What “Acis DPE input unresolved” means

It is a small **host-side hardware register initialization program**, not the
encoder firmware and not simply a set of optional quality presets. For T8103,
the `Acis_8103` input supplies one CAC-default and 39 CAC-8-bit
`{register offset, bits to clear, bits to set}` entries. The reference applies
them after power-on, checks the readback, and enables the DPE. Its normal source
is a table in **AppleAVE2.kext**, not the ASC firmware iBoot preloads.

**We cannot currently promise any encoding without it, including lower-quality
encoding.** The driver's own comment says the option became enabled by default
because every working encode needed it. `dpe_tunables=0` merely skips the entire
programming/enable function and leaves reset defaults; returning zero there is
not an encoder result. The reviewed M1 success used the correct Acis table.
Another SoC's table failed readback on M1. Treat the missing input as a functional
bring-up prerequisite, not a quality tradeoff. This does not prove that an
independently developed initialization could never work; it means that neither
successful output nor a safe reduced-function fallback has been demonstrated.
See the pinned [driver's rationale and loader](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/driver/ave_drv.c#L235)
and [M1 differences](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/docs/89-t8103-m1.md#2-the-port).

**No demonstrated derivation from iBoot's remaining memory was found.** The
preloaded ASC TEXT/DATA and its tunable records are different inputs; the driver
notes that this firmware does not program the host DPE block. Nothing reviewed
identifies the Acis host table inside those preloaded segments. Merely preserving
them therefore does not resolve this dependency. If iBoot actually leaves the
DPE programmed, an early, on-device capture of its powered register state could
help develop an independent initializer. But that premise is unmeasured; a
power-domain reset may already erase it, and final register values do not recover
the original clear/set masks, ordering, or volatile/status-bit semantics. Reading
unpowered or wrong-generation banks can reset the machine. Such a future capture
needs the kernel/boot owner and must keep all runtime Apple-derived bytes private
on the device. No MMIO access, memory dump, extraction, firmware copy or kernel
change was performed for this update.

The distinction between **ASC firmware tunables in DATA** and **host DPE
register tables** is essential. An empty ASC table was used successfully in
upstream's older M1 experiment; that experiment still used the Acis DPE table.
It does not prove the DPE data can be omitted.

| Input in the current reference | Origin / current use | Owner-compliant route and limit |
|---|---|---|
| `apple/ave_h13g.bin` | Unwrapped AppleAVE2FW_H13G Mach-O, requested by `ave_fw_load()` and other identity/restore helpers | Do **not** fetch an IPSW, copy a Mac file or install this file. Adopt only the copy iBoot already loaded, identified by `pre-loaded` and `segment-ranges` (32-byte entries). Rewrite image-walk/validation paths to consume reserved segments and known protocol/layout facts. Whether all bytes needed for validation/restart remain in J293's preload must be measured; the upstream file loader cannot simply be called unchanged. |
| `apple/ave-13.5-dpe-acis_8103.bin` | Host DPE settings from AppleAVE2.kext's `gs_sAVE_DPE_CfgSet_Acis_8103`, normally generated from a macOS13.5 `kernelcache.release.mac13g` (the extractor also finds the same set in the other13.5 kernelcaches). Acis has 0 CAT-default, 1 CAC-default and 39 CAC-8-bit entries. `AVEDPE01` stores three counts plus `{offset,clear,set}` triples. | **Unresolved functional prerequisite under current authorization.** No kernelcache extraction, old deleted header, other-SoC table or source-history copy is allowed as a shortcut. `dpe_tunables=0` skips initialization/enable; it does not establish even low-quality encoding. The source says every working encode needed it; the cited T8103 success used Acis. No successful omission or preload-memory derivation was found. An early private on-device register capture might inform independent initialization only if iBoot actually programmed and preserved the block, which is unmeasured. Final values alone do not reveal masks/order/status semantics. |
| Pristine DATA / `apple/ave-13.5-h13g-data-pristine.bin` | Firmware DATA plus BSS and iBoot fills, used for owned DATA/restart. The T8103 reference DATA span is0x128000; actual placement must come from the live ADT. On stock-m1n1 J274 Linux overwrote the preload; the Linux solution builds DATA from the firmware **file**. | Reserve/copy this boot's cold DATA before allocator use or first ASC start, retain it privately on-device, restore only after proven quiescence. Do not dump it to Linux/M5/repository. If already overwritten, fail and require a fresh boot with a proper handoff; a disk firmware file or another machine's dump is outside scope. Check that our boot memory reservation actually covers both segments before any driver experiment. |
| ASC tunables (`TUNS`/`TUNZ`) and tag fills (`CpAd`, `WrAd`, `IOBA`, `SOC_`) | iBoot writes fields in DATA. The image has an empty ASC-tunable table; upstream reconstructed T8103 DATA with that table when its live copy was gone. | Prefer the exact cold preload. A deliberate empty-table reconstruction would need the corresponding preloaded image bytes and separately reviewed validation, not borrowed H13S values. Upstream's successful empty-ASC result does not authorize skipping DPE tables. |
| STKG stack-guard field | Per-boot iBoot value; an older research builder retained a sample cookie | Keep the current boot's value private with its DATA snapshot. Do not copy the research sample; no shared sample value is needed for the interface. |
| Command/field/register facts, DevInfo identifiers and image hashes | Wire/API facts and identity digests in GPL source; not firmware byte blobs | Retain field-by-field provenance. Docs/100 separately flags43 Start_AVC and9 per-frame default-field values as an author judgment case. Prefer named independent settings over copying opaque blocks. Hash identity alone does not supply firmware or certify a different version. |

The input inventory and extraction format are checked against
[docs/100](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/docs/100-no-apple-data.md),
[the DPE loader](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/driver/ave_drv.c#L612),
[the firmware loader](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/driver/ave_fw.c#L1648)
and [T8103's SoC data](https://github.com/aquarat/apple-m-series-ave-driver/blob/9ab14e45281e8f61fb499b760408028171a5f2b5/driver/ave_soc.c).
No extraction/generation command in those documents is an instruction to run it here.

## Local host-only checks

On this Linux builder,upstream's ASan/UBSan command-builder self-test passed
2259 checks,including its negative control;the synthetic ADT parser passed1270
checks. The optional real Apple ADT case was skipped because no dump was fetched.
Logs are in `runs/sffinch/ave-{abi,adt}-host-selftest.log`. These compile ordinary
host programs only,not a kernel/module,and establish no hardware or firmware
execution. No Apple-derived input was needed for these host checks.

## Proposed `/dev/pdave` contract

Use a versioned, fixed-width ABI modeled on the ownership discipline of
`pdavd_ioctl.h`, rather than its stateless codec-control payloads. AVE is a
**stateful encoder**: its context owns reference frames and work surfaces.
This is a design proposal; no header, ioctl number or driver is implemented.

| Operation | Initial behavior |
|---|---|
| INFO | ABI/size, actual firmware identity/availability, supported codec/pixel format, dimension/stride limits, busy/fault state. Advertise no codec until its prerequisites are valid. |
| CREATE | H.264/NV12, dimensions, Y/UV strides, fixed QP, frame-rate numerator/denominator, bounded buffer counts; returns an opaque per-open context ID. One active hardware session initially; second CREATE returns busy. |
| ALLOC / MAP | Kernel-owned, wired input/output buffers mapped through AVE's own DART. Input mapping writable, output mapping read-only to userland. Return generation-tagged IDs/lengths, never physical addresses or caller-chosen IOVAs. Defer external IOSurface/DMABUF import. |
| ENCODE | Input buffer ID, output buffer ID, PTS/duration and force-IDR flag; bounded synchronous completion returning produced length, DTS/PTS, keyframe flag and actual error/status. Initial no-reordering policy keeps one frame in flight. Validate all ranges/overflow before DMA. |
| DRAIN / DESTROY | Flush valid pending output, then prove firmware stopped referencing buffers before unmapping/freeing. On timeout mark the context/device faulted; never report successful drain or recycle an active DMA buffer. A reboot may be the only valid recovery. |

An optional later poll/dequeue interface can add asynchronous jobs after lifetime
rules are measured. Keep explicit sizes/reserved-zero fields and Linux/Darwin
cross-layout checks. A future CLI generates NV12 locally, submits30 1280x720 IPPP
frames, then software-decodes the output with ffmpeg: frame count/dimensions,
valid timing, ≥40dB PSNR on that test pattern, repeated context teardown and
bounded fault handling. That would be new M1 evidence; current upstream PSNR or
host ABI checks are not substitutes.

## Effort, risks and next decision

My estimate, **after** preload and DPE prerequisites are resolved:1–3 engineering
days for the OS interface/source split;3–7 for power/DART/DAPF/boot IPC;5–10 for
session/ioctl plumbing and the first repeated30-frame H.264 test. These are
planning ranges, not measured performance or a delivery promise. Firmware/DPE
input investigation has no bounded estimate yet. HEVC, CVPixelBuffer import,
VTCompressionSession and recovery/suspend need separate follow-on work.

The largest risks are cold DATA loss during boot, firmware-build/ABI mismatch,
DAPF writes at the wrong power/start phase causing a system error, incorrect
DART ownership/cache ordering, firmware reply lengths and timeout teardown of
active DMA. The reference's H.264 B-frame restriction and latest unrerun changes
must remain visible. Preserve separate GPL kernel files and LGPL/MIT userland
interfaces; keep all Apple-derived runtime bytes on-device.

The next useful owner decision is how to resolve the **Acis DPE input without
using Apple files**, and who owns the early preload reservation/handoff. This
study does not request permission to build or boot a kernel. Until those inputs
are resolved, continue software encoding/decoding and retain the working AVD
path; do not report AVE hardware support.
