# First Linux session, after filesystem recovery

This is a future checklist, not a request to interrupt the Btrfs repair. Do not place source, build stores, logs or large artifacts on the affected filesystem until recovery and data safety have been resolved.

## 1. Put the preparation bundle in a healthy workspace

Run the offline helper tests. Record Python/OS/tool versions used. The bundle already contains committed Git history. Verify the checkpoint and keep the baseline checkout separate from the experimental integration checkout. No remote repository has been created.

## 2. Validate the candidate source registry

Confirm repository access and each exact commit object. If a revision is unavailable or inconsistent, stop and record the discrepancy instead of silently replacing it with a branch tip. Record recursive submodules, Nix inputs, dirty patches and file provenance. Preserve Omacom as the requested primary hardware reference.

A validated Git object is not a reproducible build. Add toolchain input hashes and output hashes in a separate tested build record.

## 3. Discover the build at the selected PureDarwin revision

Upstream's current guide documents Linux plus Nix for image/kernel-collection/runner builds; macOS support is partial. Inspect the actual checked-out flake and lock. Do not copy a changing web command as a version-independent API.

Current documentation names `nix run .#arm64-virt` for its ARM64 virt runner; its default `.#vm` uses x86_64. Verify that output and the appropriate image attribute exist at our pinned revision before building. Run `nix flake show` deliberately in that clean checkout to inspect available outputs. It may evaluate/fetch inputs, so it belongs on the healthy development machine, not the repairing filesystem.

Use isolated VM images/overlays, not physical disks. Keep the unmodified baseline separate from our patches. Capture exact commands, full build logs, hashes and the serial transcript. Successful compilation is not boot success, and a QEMU virt boot is not an Apple hardware port.

## 4. Inventory the existing M5 controller setup

Record the actual m1n1 revision, known-good target, kernel/firmware profile, proxy and UART devices, local tooling, and recovery method. Do not change that baseline while validating the builder. Do not infer a device name or a target model from the template.

## 5. Introduce a reviewed hardware runner later

Test transport discovery, exclusive access, logging, failure handling and recovery against the existing known-good procedure before replacing the guest kernel. Do not enable blind reboot loops. Then attempt one independently built XNU payload with explicit approval and preserved output.

Documentation references and dates are in `docs/SOURCES.md`. None of these setup or hardware steps was executed when creating this bundle.

## 6. Build-gate the RAM-only integration candidate

After the unmodified ARM64 VM baseline, follow `docs/XNU_RAM_PROBE.md` in a
separate checkout. Use the stage tool's dry run first. It does not create a Nix
output or prove kernel-collection inclusion/startup. Compile the candidate against
the pinned SDK and fix actual diagnostics without successful substitutes for
missing kernel APIs. Cross-build the userspace exerciser with that same SDK.

The first device criterion is the **unmapped** allocation/close/reclaim test. The
mapped test deliberately cannot yet pass final backing reclamation: the kernel
provider quarantines exported pages pending a real finalization proof. Record a
BLOCKED result accurately. Do not connect the M5 or enable GPU DMA to work around
this VM-only memory-lifetime gate.
