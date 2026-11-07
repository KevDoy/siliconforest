# First Linux session, after filesystem recovery

This is a future checklist, not a request to interrupt the Btrfs repair. Do not place source, build stores, logs or large artifacts on the affected filesystem until recovery and data safety have been resolved.

## 1. Put the preparation bundle in a healthy workspace

Run the offline helper tests. Record Python/OS/tool versions used. Initialize a Git repository only when its destination and publication policy are chosen. This bundle has not created a remote repository.

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
