# SPDX-License-Identifier: MIT
# The SiliconForest tree every recipe of the Cocoa stream builds from: upstream PureDarwin e7aa00a6 (the
# revision this stream is pinned to) plus one generated commit, the mechanical rename (branch
# siliconforest/e7aa00a6 of sources/puredarwin-baseline, made by tools/sf_rename.py fork).
(builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=00d5848dca1fc90c8877becf593034ed85bb7b44").packages.x86_64-linux
