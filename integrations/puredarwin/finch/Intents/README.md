Whole handwritten Finch Intents.m and its runtime generator ported to our
Linux/Nix toolchain. The generator takes our declarations, not an SDK. Our model
catalogue covers classes explicitly used in Finch source/tests; it does not
reproduce the much larger SDK-derived catalogue. No Foundation stand-ins are
emitted. Compile passes; link needs Foundation's NSPersonNameComponents and
NSPersonNameComponentsFormatter. Not installed or runtime-tested.
`declarations.py sources/finch/userland/Intents` regenerates our header.
