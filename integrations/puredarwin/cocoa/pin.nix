# SPDX-License-Identifier: MIT
# The PureDarwin tree every recipe of the Cocoa stream builds from: the lab's pinned commit
# (sources/puredarwin-baseline, e7aa00a6 on upstream's branch upstream-xnu-12377).
(builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea").packages.x86_64-linux
