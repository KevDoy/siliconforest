# Licence status

The root [`LICENSE`](LICENSE) is a notice. This repository is not licensed as a
whole under the MIT License.

**Warning: this tree is a licensing mess.** Licences are mixed across
directories, and there are no reliable per-file headers or SPDX tags. Anyone
reusing code has to check each file's provenance. Crediting a project in
[`NOTICE`](NOTICE) or in the README does not satisfy GPL-2.0, APSL-2.0, or any
other licence, and it does not say which terms apply to which file. The
explanation is [`LICENSING.md`](LICENSING.md).

What the documents in this tree actually say:

- This repository chose the MIT License for original SiliconForest code (files written
  for this project and not ported, adopted, or copied), copyright "Copyright (c)
  2026 Chris Kearney and the SiliconForest contributors". The terms are
  [`LICENSES/MIT.txt`](LICENSES/MIT.txt). That choice does not cover the rest
  of the repository, and it has not been shown file by file.
- Ported and adopted code keeps the licence of the code it derives from.
  [`NOTICE`](NOTICE) is the provenance record. [`LICENSES/`](LICENSES/) holds
  the texts that were copied in. Neither one is a per-file assignment.
- [`AGENTS.md`](AGENTS.md) still says: "License selection for original code is
  pending; do not relicense upstream code."

The release to-do note is [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md).
It is the release checklist left in this repository on 17 November 2026. Two
items are the licensing work that was not finished:

- Item 4. GPL-2.0 kernel extensions prelinked with the APSL kernel. The
  checklist states the question and does not answer it: on what terms a built
  collection or system image may be distributed. It says a decision was needed
  before any binary is published. The same question is restated below. The
  PureDarwin base those kexts link against is itself mixed: BSD-2-Clause for
  `PUREDARWIN_LICENSE.txt` only, plus MPL-2.0, MPL-2.0 OR MIT, tri-licensed
  files (as a PureDarwin maintainer stated), MPL-2.0 OR BSD-2-Clause where
  `NOTICE` read that on a file, and APSL-2.0 for Apple's XNU and other Darwin
  code. Per-file terms are upstream PureDarwin's headers:
  <https://github.com/PureDarwin/PureDarwin>. The maintainer's comment:
  <https://github.com/chriskearney/siliconforest/issues/23#issuecomment-6984090313>.
- Item 6. Per-file headers. SPDX `MIT` tags with no copyright line, tags the
  project may not have the right to apply, missing copyright lines on GPL, ISC,
  BSD, and MPL files, Finch files that do not say which are Finch's, and ten
  GPL-derived directories with no `NOTICE`.

The checklist records that the owner then chose to make this repository public
with those items open. Publication did not close them. The README of 7 to 9
November 2026, which said that no project-wide distribution licence had been
selected and pointed at this file, is kept at
[`docs/history/README-2026-11-09.md`](docs/history/README-2026-11-09.md).

Other pages still speak as if "our code" were simply MIT and `LICENSE` were
the grant: the licence rules in [`CONTRIBUTING.md`](CONTRIBUTING.md) (those
rules are about code a contributor newly writes) and the attribution section of
[`docs/design/siliconforest-naming.md`](docs/design/siliconforest-naming.md).
Read them with `LICENSE`. They are not a file-by-file finding.

Nothing here relicenses another project's code, chooses among dual-licence
options, or states that a combined kernel collection or system image is
distributable.

## Unresolved: distribution of combined binaries

Open question. Not legal advice. No answer in this repository.

GPL-2.0-only Asahi-derived kexts are prelinked into the same kernel collection
as APSL-2.0 XNU. The FSF's licence list describes APSL 2.0 as a free software
licence incompatible with the GPL
(<https://www.gnu.org/licenses/license-list.html#apsl2>). Distributing that
collection may not be possible under both licences at once. Item 4 of the
release checklist leaves this open. A PureDarwin maintainer asked it directly
on issue 23.

The FSF's list also describes Apache-2.0 as incompatible with GPL version 2
(<https://www.gnu.org/licenses/license-list.html#apache2>). Finch is MIT OR
Apache-2.0. This tree does not show a Finch object linked into a GPL-2.0-only
kext, and it does not record which side of Finch's dual licence a given file
is used under. If a binary combines the Apache-2.0 side with GPL-2.0-only
code, that combination is unresolved too. The GPL-3.0-only desktop linking
the APSL libSystem, also noted in item 4, is unresolved too.

Texts named by [`LICENSES/README.md`](LICENSES/README.md) and still absent from
`LICENSES/`: BSD-3-Clause, and the Swift exception. Coverage of Apple's binary
driver licence over PureDarwin components: not established (`NOTICE`).
