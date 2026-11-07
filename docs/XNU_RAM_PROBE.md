# RAM-only XNU integration candidate

## What is real and what is not

The C probe engine and POSIX exerciser are implemented and executed on the host.
The C++ adapter in `integrations/puredarwin/DLMemoryProbe` is **source-only**:
no actual XNU SDK compile, kernel link, KC inclusion, load, or VM run has occurred.
The stage tool was executed against disposable fixture Git repositories, not a
PureDarwin checkout. The fixtures test patch application, not kernel semantics.

The intended first kernel tests do not use the Apple GPU, DART, RTKit, firmware,
physical target, or M5. Run in a disposable ARM64 PureDarwin VM only, after an
unmodified baseline is reproducible. Never target the filesystem under repair.

## Components

- `probe/memory_probe.c`: an opaque C service with 32 concurrent sessions, one
  buffer per session, a shared bounded backing store, and 256 single-use tickets.
  The shared store has 16 live-buffer slots and 64 simultaneous lease slots.
  Allocation limit: 1 MiB; total retained backing limit: 8 MiB. No buffer addresses
  or physical page numbers appear in the ioctl protocol. Mapped buffers can
  outlive the file session. All service operations require common serialization.
- `provider_iokit.cpp`: candidate IOBufferMemoryDescriptor allocator. It requests
  nonpageable kernel/user-shareable backing, prepares it, zeros the rounded size,
  and enumerates host physical segments with the no-mapper option. No physical
  contiguity assumption and no DMA mappings. Unexposed buffers pair successful
  prepare with complete and release. A completion error is permanently retained,
  not retried into uncertain descriptor state. All this still needs the real SDK
  and runtime to confirm the selected APIs, behavior, cache mode and build flags.
- `DLMemoryProbe.cpp`: candidate `pd_dri_ops` provider. Reuses PureDarwin's clone
  minors and page-pager bridge. Uses a sleeping mutex, root-only opens, architecture
  cache constants, exact ioctl sizes and initial read/write-only mapping requests.
  Requires `dlmemtest=1`; no unload path is allowed. The provider, service, and
  callback ticket storage remain alive until reboot. A published callback cannot
  be left pointing into unloaded code after a node-publication failure.
- `userspace/memory_probe_smoke.c`: real mmap/close/fork/exit exerciser. Device mode
  requires a named device and `--allow-experimental`; it never guesses hardware.
  `--posix-control` uses an unlinked temporary file, not a device or XNU.

## The unresolved teardown gate, deliberately NOT bypassed

At the pinned source revision the pager calls the registered release callback in
its named-reference teardown, before `memory_object_destroy`. The earlier source
review does not prove that this is a complete VM/TLB/backing-reuse barrier. This
is an unverified integration requirement, not a demonstrated upstream bug.

The real IOKit candidate **permanently quarantines every exported allocation**
until reboot. It does not advertise `DLMP_CAP_MAPPED_RECLAIM`, has no switch to
ignore that gate, and its collector reports retained backing/backend failures.
The mapped device test must therefore produce `BLOCKED_MAPPED_RECLAMATION`
(exit 77) after observing correct map/close/child-exit behavior, not `PASS`.
If any earlier step fails, it reports `FAILED`, not an expected block.

The no-mapping device test may eventually pass once the adapter builds and runs:
allocate, close, collect unexposed backing exactly once, verify zero resident
bytes. **Neither device test has been run.** Host tests simulate notifications and
provider teardown permission; they do not satisfy this gate.

A later, separate review must trace the final device-pager/VM-object/page lifetime
and then either prove the existing release point sufficient or supply a proper
finalization hold/hook. Do not replace this gate with a delay, a counter, a fake
quiescence acknowledgement, or a successful stub. Keep module/ticket lifetime
separate from buffer-backing lifetime.

## Staging, only in a separate healthy checkout

```sh
# Read-only source plan; verifies exact HEAD, clean tree and DRM-header blob.
python3 tools/stage_puredarwin.py --checkout /healthy/path/PureDarwin-lab

# Applies only the candidate source patch after repeating verification.
python3 tools/stage_puredarwin.py --checkout /healthy/path/PureDarwin-lab \
  --execute --acknowledge-experimental
```

The only accepted commit is `e7aa00a60f0b56ef35aa24252e81467787bf71ea`.
The tool adds `src/Kernel/Extensions/DLMemoryProbe` and a single parent CMake
`add_subdirectory`. It runs `git apply --check` before applying. It does not fetch,
commit, publish, configure, build, install, or boot anything. It rejects a dirty
checkout including untracked files. The patch copies only original lab code; no
upstream implementation/firmware/credentials are embedded.

Review the staged diff and explicitly track the new files before using the Git
flake: untracked files can be missing from its source snapshot. Source review of
`nix/sources.nix` shows the extensions subtree is included in both kext source
filters, but that is **not** proof that this bundle is selected into the desired
kernel collection or that its start routine executes. Verify CMake target, XNU
private headers, symbols, library dependency and collection/startup selection.
No guessed `nix build .#DLMemoryProbe` attribute is promised by this repository.

## Userspace build and modes

On the host:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iprobe \
  userspace/memory_probe_smoke.c -o /healthy/path/memory-probe-smoke
/healthy/path/memory-probe-smoke --posix-control
```

For Darwin, use the **actual pinned target SDK/toolchain**, not this host binary.
Only after reviewing the image, verifying bundle loading and explicitly setting
the boot argument in a disposable VM should a privileged VM shell invoke:

```sh
memory-probe-smoke --device /dev/dri/dlmemtest --allow-experimental --unmapped-only
memory-probe-smoke --device /dev/dri/dlmemtest --allow-experimental
```

Use a clean VM probe baseline for each mapped experiment. The second command is
expected to block final reclamation with this candidate. It checks initial zero
contents, aliases, initial executable/private/out-of-range rejection, owner-file
close, inherited mappings, parent unmap and child process-exit cleanup. Later
mprotect upgrades, sandboxing, hostile concurrent kernel clients and production
security are NOT validated here. PDDRMDri publishes mode 0666; the adapter's
credential check, not those node permissions, is the intended open gate.

## Result vocabulary

| Result | Meaning |
|---|---|
| PASSED_POSIX_CONTROL | Real native host syscalls passed; no XNU claim |
| PASSED_KERNEL_UNMAPPED | Future device run only: unexposed backing reclaimed |
| BLOCKED_MAPPED_RECLAMATION | Future device run: mapping tests observed, final reuse withheld |
| PASSED_KERNEL_MAPPED | Reserved for a later verified finalization implementation |
| FAILED | A real check failed; never turn this into a skipped success |

Only the first of these results has actually been produced in this increment.

## Pinned source observations

See `reports/sprint-004/source-review.json` and `docs/MAPPING_HANDOFF.md`.
The source ledger distinguishes selected line reads from full byte verification.
