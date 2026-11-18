# Release checklist: what stands between this repository and "public"

State: 17 November 2026, after the `release-prep` branch. Source: the open-source audit of
the same day (31 findings, numbered F1 to F31 below) and the work done since. The repository
is private; only the owner changes that.

"Owner" marks a decision only the owner can take. Effort is a rough estimate.
Nothing here is legal advice.

## Done on `release-prep`

| Item | Audit | What was done |
|---|---|---|
| Licence for our own code | F2 | `LICENSE` (MIT), `LICENSES/` with every licence text in use and its source, `NOTICE` corrected, `LICENSE_STATUS.md` now a pointer |
| Outsider documentation | F9 | `README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, screenshots in `docs/images/` |
| Disk identifier in a test | F6 | the literal in `tests/test_m1_boot_install.py` is synthetic |
| Build output in the tree | F5 | `chonkstep-darwin/client-dmabuf/target*/` removed, ignored |
| Session links inside files | F10 (files) | removed from two `m1-touchbar` patches and one `m1-sound` commit text |
| Other vendors' screenshots | F4 | 107 capture images removed from the vendored Chonkstep copy |
| Personal address in files | F14 | the three vendored packaging files use the noreply address |
| Issue wording | | titles and bodies reviewed; see the record kept outside git (`runs/public/issue-edits.md`) |

All of these are fixed **in the tree only**. Every one is still in the history of this
repository (see 2).

## Blockers

### 1. The old pull requests (F1). Owner; GitHub side.
Pull requests 1 to 22 are from an earlier, unrelated project that used this repository.
GitHub keeps `refs/pull/N/head` for each; three of them contain a file with the owner's
personal email address. A repository owner cannot delete these refs, and making the
repository public publishes them.
- Options: a fresh repository (removes them by construction; see 2), or a request to GitHub
  Support to remove the refs.
- Effort: none of ours for a fresh repository; days of waiting for Support.

### 2. History: a fresh repository or this one. Owner.
In history, and not removable by a commit: the old pull-request refs (1), the removed
captures, the 110 MiB of build output, the disk identifier, the three address literals, a
session link in 773 of 788 commit messages, 11 commits authored as `darwin-lab@localhost`,
and 414 Actions runs with their logs.
- Recommended by the audit: a new repository started from the cleaned tree (one commit, or a
  filtered history that keeps blame), with this one kept private as the archive.
- Cost: public blame and bisect are lost with a single commit; issues must be recreated
  (GitHub does not move issues from a private to a public repository;
  `tools/roadmap_create_issues.sh` and `docs/roadmap/issues/` exist for that).
- Also the owner's: whether `Co-Authored-By` lines and session links stay in commit messages,
  and whether the `upstream-*` branches (PureDarwin's public history) are published.
- Local only, never to be pushed: branches with 11 commits under another personal address
  (F15). Push one branch by name, never `--all` or `--mirror`.
- Effort: about an hour once the tree is clean; a filtered history, a day.

### 3. The vendored Chonkstep copy (F7, F18, F20, F31). Owner.
`integrations/siliconforest/chonkstep-darwin/desktop-shell/source/` is a full copy of
Chonkstep at one commit (about 4,000 files, a third of the tree), GPL-3.0-only, with 57 MiB
of benchmark evidence that names machines, and Bluetooth-style sample addresses that look
like fixtures but were not confirmed to be invented.
- Recommended by the audit: remove the copy, pin upstream Chonkstep by commit, keep the Darwin
  changes as a patch series with SPDX lines (only 2 of the 38 compositor patches carry a
  licence line today), and drop patches as upstream takes them.
- Until then the copy's tests that read the removed captures do not build.
- Effort: one to two days, plus the upstream merge on Chonkstep's side.

### 4. GPL-2.0 kernel extensions and the APSL kernel (F3). Owner; take advice.
About 350 files are GPL-2.0 ports of Linux drivers, built as kernel extensions against XNU's
interfaces and prelinked with XNU (APSL 2.0) into one kernel collection. The FSF lists
APSL 2.0 as a free licence that is not GPL-compatible. The question, stated and not answered
here: on what terms may a **built** collection or system image be distributed? Publishing
the sources side by side, each under its own licence with notices intact, is common practice.
- Related: about 77 files are dual-licensed (GPL-2.0 OR MIT / BSD) and could be taken under
  the permissive option, the Rust GPU driver among them; the desktop (GPL-3.0-only) links
  the APSL libSystem.
- Decision needed before any binary is published: source only, or images too; and if images,
  with which firmware and Apple-licensed parts left out.
- Effort: the decision; advice if wanted.

## High

### 5. Build reproducibility for outsiders (F8). Work.
32 recipes fetch from `file:///home/…` local clones; about 400 files name `/nix/store/<hash>`
paths; 99 tools or recipes read the untracked `artifacts/` or `private/`; there is no single
lock for the 25 source checkouts.
- To do: a `sources.lock` and fetch script; recipes that fetch by URL and hash; a build from a
  clean clone on a machine with none of the local directories; a QEMU boot of the result;
  a BUILDING and an INSTALL guide that are not written for one disk.
- Effort: one to two weeks. Until then the README says plainly that the system cannot be
  built from a clone.

### 6. Per-file headers (F24, F25, F26, F27). Work, with owner sign-off on the MIT tags.
- About 1,975 files say `SPDX-License-Identifier: MIT` with no copyright line (807 under
  `finch/`, 259 under `firefox/`, 138 under `m1-thunderbolt/`). Each needs to be confirmed
  as ours (then gets the copyright line) or re-marked as its source's.
- 273 of 437 files tagged GPL, ISC, BSD or MPL have no copyright or author line in their
  first 40 lines; ports need their upstream notices restored.
- Under `finch/` only 28 files carry Finch's copyright line and the files do not say which
  are Finch's and which are ours.
- Ten directories with GPL-derived code have no `NOTICE` (`m1-rust-kext`, `m1-gpu-rtkit`,
  `m1-dcp`, `m1-dcp-dart`, `m1-lkpi-extras`, `m1-cpufreq`, `m1-thermal`, `m1-drm72`,
  `m1-native-boot`, `m1-avd-capacity`); `m1-apcie-host/NOTICE` is the model.
- Licence texts still missing from `LICENSES/`: BSD-3-Clause, Swift-exception.
- Effort: three to five days, mostly mechanical, with a review per directory.

### 7. Home paths and bench addresses (F16, F17, F19). Owner decides the scope; then work.
`/home/<user>` appears about 8,800 times in 650 files and a macOS home path about 120 times;
five private-network bench addresses with the SSH user name are in about 90 tracked files
(the audit counted about 290 before the rename); Linux `boot_id` values are in 43 report
files; one install note gives the exact free block range of the owner's disk.
- In live tools and recipes: parametrise (environment variables, a local config file).
- In frozen reports and attempts: these are records and are not rewritten. Publish as they
  are, move to an archive branch, or keep private (see 8).
- Effort: two to three days for the live files.

### 8. Frozen evidence and the agent work log (F11, F30). Owner.
`reports/` (979 files), 126 `attempts/` files, 336 logs, `archive/`, `planning/`, the parked
`m1-apcie-bridge` models and `AGENTS.md` (98 KiB of working instructions and log, with bench
addresses) document how the system was brought up. No credential or device identifier was
found in them beyond 7.
- Options: publish, move to an archive branch, or keep in the private repository.
- `AGENTS.md` is loaded by tools and was not touched. Suggested: keep the full file in the
  private archive and publish a short one (rules that still hold: truthfulness, boundaries,
  required checks, naming, licence rules) that points to `CONTRIBUTING.md`.

### 9. Continuous integration (F13). Work.
Every recent run of the one workflow (`Offline laboratory contracts`) fails; the runs and
logs become public with the repository (a fresh repository has none).
- To do: make the workflow pass from a clean clone or remove it, so the first public run is
  green. One test needs an untracked artefact.
- Effort: half a day.

## Medium

| Item | Audit | Kind | Effort |
|---|---|---|---|
| Apple applications as a feature: the README says exactly what runs (two applications, the owner's copies, nothing in the repository). Confirm this presentation, and that the screenshots, which show those two applications' windows, are wanted | F23 | Owner | decision |
| Tools that fetch a kernelcache from Apple's CDN and extract tables (`m1-ave/tools/`): publish or not | F22 | Owner | decision |
| Five copies of Apple's `IOHIDEventService.cpp` (APSL header intact) under `m1-native-input/`: reduce to patches if possible | F21 | Work | half a day |
| Clean-room status of reimplemented private frameworks (`sysmon`, CoreUI pieces, `UTTypeTable.inc`) was not assessed | audit, "not checked" | Owner | review |
| The 219 images in the tree were not opened by the audit; the six in `docs/images/` were | audit | Work | an hour |
| Issue comments that only the owner should rewrite or remove | | Owner | an hour |
| Remaining old names behind escape sequences (issue #60) | | Work | small |

## Before the switch

1. Re-run the scans on the release tree: gitleaks; the value search for Wi-Fi and disk
   identifiers; the hash comparison against `private/`; greps for home paths, private
   addresses, session links and personal email domains; a binary inventory.
2. Create the public repository private first; push one branch by name; inspect it on GitHub
   (size, licence detection, no pull-request refs, no Actions runs).
3. Enable private vulnerability reporting (`SECURITY.md` relies on it).
4. Recreate the roadmap issues.
5. The owner reads it as an outsider and changes visibility himself.
6. Keep the old repository private until the new one has been checked against it.
