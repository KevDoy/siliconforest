Whole pinned Finch ColorSync implementation (four C files) with our source-derived
declarations and PureDarwin's CFRuntime ABI. Its exact Skia m155 skcms module is
fetched as open source and built locally; no renderer, SDK or Apple profile files.
`fetchzip` hashes the unpacked source because Gitiles gzip metadata varies.

Candidate has 293 own exports and retains all 11 names from the installed thin
ColorSync wrapper. Clean root import scan. The original Linux-built CLI profile/
transform test exits 0 on M1 and exactly matches all 152 result lines on the M5.
The canonical existing library stays intact; the candidate remains under sffinch,
and a concrete integration request (hash/evidence) is filed with Claude.

This changes profiles from the thin wrapper's name strings to actual ICC CF
objects; do not mix old string objects into new APIs. Device preference writes,
external CMM plugins and installation of the source-generated profile files are
not tested. Finch's empty CMM enumeration/code-fragment limitations remain.
Public numeric facts checked against the maintainer's Go API:
https://pkg.go.dev/github.com/deploymenttheory/go-bindings-macosplatform@v0.20.1/bindings/internal/raw/frameworks/colorsync
