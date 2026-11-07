# Darwin Lab

An experimental Apple Silicon Darwin/XNU project, with Omacom's m1n1,
Linux Aurora and Mesa integration branches as the primary hardware references,
and Chonkstep as the intended desktop. `darwin-lab` is a working repository
name, not a final OS brand.

**Implemented now:** verified-source tooling, a lexical driver inventory,
enum/layout probes, an executable lifetime policy, and a provider-backed memory
coordinator with real host-allocation tests, plus a source-traced pager-release
handoff with threaded notification tests. Compile-only checks emit ARM64
Mach-O and AArch64 ELF objects and measure our selected C layouts. These are
portable components, not a working native XNU driver. Increment 004 adds a
bounded RAM-probe engine, a real POSIX lifetime exerciser, and a **source-only
IOKit/PDDRMDri integration candidate**. That C++ adapter has NOT been compiled
against XNU, linked, loaded or booted. It intentionally quarantines mapped backing
until the final VM teardown/reuse boundary is verified. **Not implemented:** a working XNU platform port,
a GPU driver, a Darwin Chonkstep backend, or hardware boot automation.

The recovering Linux machine has not been used. The M5 remains the future
USB/debug controller. No target model is assumed.

## Run the actual tests

Python 3.10+ and a native C11 compiler are sufficient. There are no Python package
dependencies. On a healthy Linux or macOS workspace:

```sh
python3 tools/check.py --sanitize --cross
python3 tools/lab.py validate
python3 tools/lab.py plan
```

The first command compiles and executes **our C policy, backing-store coordinator
and host test provider**, not upstream driver code. Its `--cross` step emits
foreign object files but never executes them; that step requires Clang. The last command remains `BLOCKED` and
has no hardware backend. Omit `--sanitize` only when your compiler lacks the
sanitizer runtime; that is a weaker test run and is recorded accordingly.

See `reports/sprint-004/checks-clang.json`, `checks-gcc.json`, `probe-protocol-abi.json`
and the three mutation reports for the latest completed local checks. The prior sprint reports remain historical.
GitHub Actions is configured but **has not run on GitHub**.

## Useful commands

```sh
# No network: review pinned file acquisition.
python3 tools/acquire_sources.py plan --repo omacom/linux-aurora

# Explicit network opt-in, on a healthy machine. Downloads source only.
# Fails rather than accept a hash mismatch or overwrite a corrupt cached file.
python3 tools/acquire_sources.py fetch --allow-network --repo omacom/linux-aurora

# Lexical inventory, not compiler/type analysis.
python3 tools/source_inventory.py path/to/driver.rs path/to/gem.rs

# Exit 0: enum values match; exit 1: difference; exit 2: unsupported/input error.
python3 tools/uapi_contract.py enum kernel/asahi_drm.h mesa/asahi_drm.h

# Generate a real SDK-specific sizeof/alignof/offsetof/constant probe.
python3 tools/uapi_contract.py generate-probe specs/pd-dri-layout-probe.json > probe.c
```

Compile a generated probe separately with each reviewed target toolchain and
header set. Never label a Linux-host layout result as a Darwin ARM64 result.
`specs/pd-dri-layout-probe.json` intentionally labels its existing capture
`linux-x86_64-host-only`.

## GitHub publication

This repository was created and committed locally. The connected GitHub toolset
in the development session exposed reads only. **No remote repository, issue,
PR, push, or CI run was created.** Intended destination:
`chriskearney/darwin-lab`, private.

The ZIP includes `.git` history and has no remotes. After extraction, inspect the
code, authenticate `gh` through its normal login flow, and run:

```sh
python3 tools/publish_github.py           # Plan only; no subprocess or network.
python3 tools/publish_github.py --execute # Creates a private repo and pushes.
```

The publisher checks the account, clean main branch, and absence of existing
remotes. It never force-pushes or deletes a repository. It verifies visibility
and the remote commit after pushing. A partial failure must be inspected rather
than blindly retried. Its write-path unit tests use mocks; no live publication
was possible in the development session.

Using the separate Git bundle instead of the ZIP:

```sh
git clone --origin imported-bundle --branch main /path/to/darwin-lab-sprint-004-2026-11-07.bundle darwin-lab
cd darwin-lab
git remote remove imported-bundle
```

## Start here

- `docs/SPRINT_004.md`: latest implementation and execution evidence.
- `docs/XNU_RAM_PROBE.md`: candidate kernel integration, staging, acceptance gates.
- `docs/SPRINT_003.md`: previous mapping-handoff increment.
- `docs/MAPPING_HANDOFF.md`: inspected pager boundary, ticket lifetime and remaining kernel work.
- `docs/SPRINT_002.md`: preceding coordinator/cross-compilation increment.
- `docs/BACKING_STORE.md`: ownership, provider contract and missing XNU integration.
- `docs/CROSS_ABI.md`: compile-only methodology and target limitations.
- `docs/SPRINT_001.md`: previous development increment.
- `docs/PORTING_MAP.md`: selected Linux dependencies and XNU adapter obligations.
- `specs/BUFFER_LIFETIME.md`: executable policy and integration limits.
- `docs/SOURCE_VERIFICATION.md`: provenance levels and how to obtain full sources.
- `planning/sprint-001.json`: local completion evidence and next tasks.
- `planning/issues/`: issue drafts, not created GitHub issues.
- `AGENTS.md`: boundaries for later coding agents.

## Repository layout

```text
integrations/ Source-only XNU adapter, not a kernel build result
probe/       Tested RAM-probe protocol/engine, not a physical driver
userspace/   Actual POSIX control and future explicit device exerciser
core/        Portable provider-backed coordinator, not an XNU driver
contracts/   Freestanding C reference policy, not kernel-integrated code
specs/       Boot/graphics/policy contracts and a real-header layout probe spec
tools/       Tested Python CLI tools; live actions require explicit opt-in
tests/      Python tests and compiled C regression/adversarial tests
reports/     Results from this development session, with scope and limitations
archive/     Original audit, candidate pins, and old scaffold evidence
planning/    Work items and unpublished issue drafts
```

Downloaded sources, firmware, credentials, binaries, private logs, and build
outputs are excluded. Upstream code is not redistributed in this package.
No project-wide distribution license has been selected; see `LICENSE_STATUS.md`.
