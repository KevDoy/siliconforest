# Licensing of this repository

Read [`LICENSE`](LICENSE) first.

**Warning: this tree is a licensing mess.** It is not licensed as a whole under
the MIT License. Licences are mixed across directories. There are no reliable
per-file headers or SPDX tags. Anyone reusing code has to check each file's
provenance. A credits list does not satisfy a licence and cannot tell you which
terms apply to which file. GPL-2.0 and APSL-2.0 code carry their own terms (the
licence text, notices, source availability, and the APSL's modification
notices), as do the other licences in the tree.

[`LICENSE`](LICENSE) is a notice, not a grant. The MIT text that used to be
that file is the ordinary MIT licence in [`LICENSES/MIT.txt`](LICENSES/MIT.txt).
This repository stated that MIT covers original SiliconForest code only: files written
for this project that were not ported, adopted, or copied, under "Copyright (c)
2026 Chris Kearney and the SiliconForest contributors". That statement is true
only where a given file really is that code. It is not established file by
file. [`AGENTS.md`](AGENTS.md) still says licence selection for original code
is pending. [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md) item 6
records SPDX `MIT` tags with no copyright line that were not confirmed, missing
copyright lines on other tags, and ten GPL-derived directories with no
`NOTICE`. Many `PROVENANCE` notes still say selection is pending. Where it is
unclear, the file is not MIT by this document.

Everything else keeps the licence of the code it derives from. This file does
not relicense it. What the tree actually shows, and what it does not:

- Full texts are in [`LICENSES/`](LICENSES/). [`LICENSES/README.md`](LICENSES/README.md)
  says where each copy came from, which ones are missing (BSD-3-Clause,
  Swift-exception), and that Apple's binary driver licence is not offered here.
- [`NOTICE`](NOTICE) names projects, the licence each source declares, and what
  was taken. Its header says it is a provenance record, not a legal review, and
  many entries are marked "not verified". Per-directory `NOTICE` files are more
  specific where they exist. They do not cover the tree, and they do not apply
  themselves to a file that does not identify its source.
- An SPDX line is evidence for the file it is written on. It is missing on a
  large part of the tree, and a tag without a copyright line was not treated as
  confirmed by the release checklist. Two tags in one file do not choose one.
- There is no directory layout by licence. GPL, APSL, MIT, BSD, ISC, MPL, and
  Apache-licensed code sit in the same directories. See `NOTICE` for the
  sources it was able to name.
- The PureDarwin base is mixed. `PUREDARWIN_LICENSE.txt` is BSD-2-Clause for
  that licence file only. A PureDarwin maintainer says the project also
  includes pure MPL-2.0, MPL-2.0 OR MIT, and tri-licensed files
  ([comment](https://github.com/chriskearney/siliconforest/issues/23#issuecomment-6984090313)).
  `NOTICE` had already recorded some newer files as MPL-2.0 OR BSD-2-Clause
  from their own notices, and Apple's XNU and other Darwin code as APSL-2.0
  or whatever that file's header says. This repository has not enumerated the
  tri-licences. The licence of a PureDarwin file is the header on that file
  in [upstream PureDarwin](https://github.com/PureDarwin/PureDarwin).
  `docs/roadmap/issues/SF-0-index.md` still says "BSD-2-Clause for its own
  files". That shorthand is wrong as a description of the whole base.

## Unresolved: distribution of combined binaries

This is an open question. It is not legal advice. This repository does not
answer it.

GPL-2.0-only kernel extensions ported from Asahi Linux are prelinked into the
same kernel collection as XNU, which is APSL-2.0 Apple code.
[`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md) item 4 records that
and asks, without answering, on what terms a built collection or system image
may be distributed. A PureDarwin maintainer asked the same thing:
[how kernel collections with GPL-2.0 and APSL-2.0 code would be distributed](https://github.com/chriskearney/siliconforest/issues/23#issuecomment-6984090313).

The FSF's licence list describes APSL 2.0 as a free software licence that is
incompatible with the GPL
([#apsl2](https://www.gnu.org/licenses/license-list.html#apsl2)). If that view
applies, distributing that kernel collection may not be possible under both
licences at once. No choice is recorded here. The checklist's remark that
publishing the sources side by side is common practice is not a statement that
the combined binary may be shipped.

Where Finch's Apache-2.0 terms meet GPL-2.0-only code, the same kind of
question is open. The FSF's licence list describes Apache-2.0 as incompatible
with GPL version 2
([#apache2](https://www.gnu.org/licenses/license-list.html#apache2)). Finch is
recorded as MIT OR Apache-2.0. The GPL-2.0-only code identified here is in
kernel extensions; Finch is recorded as userspace frameworks. This file does
not find a link of a Finch object into a GPL-2.0-only kext, and it does not
record a choice of Finch's MIT side or its Apache-2.0 side. Those are
different terms. A binary that takes the Apache-2.0 side and combines it with
GPL-2.0-only code is unresolved here too. A credits line is not evidence the
files were kept apart.

Item 4 also notes that the GPL-3.0-only desktop links the APSL libSystem.
That is unanswered here as well.

The note the release work left in this repository, recommending that licensing
be treated as unfinished, is [`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md)
(items 4 and 6). Item 4 leaves open the terms on which a kernel collection may
be distributed: GPL-2.0 kernel extensions prelinked with the APSL kernel. The
checklist records that the repository was made public with those items open.
The statement that no project-wide licence had been selected, and that one had
to be chosen before a public release, is preserved in the README of 7 to 9
November 2026,
[`docs/history/README-2026-11-09.md`](docs/history/README-2026-11-09.md).
Status of what is still unresolved: [`LICENSE_STATUS.md`](LICENSE_STATUS.md).

A kernel collection or a system image built from this repository combines
programs under different licences. Nothing here states that such a combination
may be distributed.
