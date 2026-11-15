# PDAppleAVE source port, in progress

The Linux/Nix builder owns source compilation; CODEX-TB owns the installed
kernel base and boot slots. Diagnostic AVE kext SDK compilation passes; no kernel collection was built
or installed and no hardware encode has run. See SF-AD and `runs/m1-coord/requests-for-CODEX-TB.md`.

`kernel/upstream` contains unchanged GPL command/ADT parsers from the pinned
upstream in NOTICE. `pdave_preload.c` rejects unknown firmware identities,
missing cold reservations, malformed/overlapping ranges and malformed Acis
input. Its DPE callback adapter ports upstream RMW, enable and masked readback
order. It does not map/power hardware or establish that memory was reserved.
The owned command/ADT copies pass2259+1270 upstream host checks; the new
identity/preload/DPE tests and private-path/extractor checks also pass. These
use synthetic/source-defined cases only, no private Apple inputs:

```
bash integrations/puredarwin/m1-ave/host-test/run.sh
```

First live metadata query found pre-loaded AVE with TEXT0xcc000 and
DATA0x128000. The chosen iBoot identity maps exactly to13.5 in pinned m1n1,
consistent with the installed stub's documented13.5 installer selection.
This is not the M1's newer macOS installation. Actual TEXT AppleAVE2FW/RTKit
build tags and early cold DATA preservation are still to be measured before
command submission. No nearest-version ABI override is permitted.

Owner-authorized DPE reproduction, run from the repository root:

```
# Run this read-only script ON SiliconForest; retain JSON privately here.
python3 integrations/puredarwin/m1-ave/tools/preload-metadata.py
# Arguments below are private owner-selected provenance, not repository config.
python3 integrations/puredarwin/m1-ave/tools/fetch-kernelcache.py \
  --url "$APPLE_PUBLIC_IPSW_URL" --build "$APPLE_BUILD" \
  --directory private/ave/new-source
PATH="$LZFSE_TOOL_BIN:$PATH" python3 integrations/puredarwin/m1-ave/tools/extract-dpe.py \
  --kernelcache private/ave/new-source/kernelcache.release.mac13g \
  --source "$APPLE_PUBLIC_IPSW_URL" --build "$APPLE_BUILD" --os-version 13.5 \
  --metadata private/ave/live-adt-metadata.json --output private/ave/new-acis.bin
```

`fetch-kernelcache.py` accepts only HTTPS Apple CDN IPSW URLs. It fetches bounded
ZIP ranges, checks manifest model/version/build, and materializes ONLY the
manifest and J293 kernelcache. All raw ranges, member bytes and per-input
source/build/SHA256 records stay under git-ignored private/ave/. It does not
claim a whole-IPSW hash for a partial fetch. `extract-dpe.py` uses the vendored
GPL extractor with notices and restricts Acis layout to0/1/39 entries. Install
lzfse through our Nix package (`pkgs.lzfse`), not on either Mac.

The first matching table has been extracted and is held privately pending
preload identity/reservation evidence. No firmware image was fetched. No table
has been transferred or installed. Future table installation is allowed ONLY
on the M1 SiliconForest root, with its own private install ledger. Firmware
TEXT/DATA must ONLY be adopted from this machine's iBoot preload. Never upload
those segments to Linux/M5 or reconstruct them from a firmware file.

A diagnostic-only PDAppleAVE service and /dev/pdave INFO now compile against
the PureDarwin SDK. pdave=1 reads metadata and bounded TEXT identity only when
ADT puts both segments in DRAM below XNU physBase; it neither maps DATA nor
programs hardware, and reports caps=0. It does not assert stage1/2 preserved
cold DATA. pdave-info is the Linux-built client. Other boot modes refuse.

Remaining port: early reservation handoff, encoding ioctls,
PMGR/DART/DAPF, actual scratch-register/ring IPC (not RTKit messaging), H264 IPPP
session/teardown, VideoToolbox compression with software fallback. First hardware
milestone is30x1280x720 test-pattern frames, ffmpeg decode, PSNR>=40dB, then
fps/CPU comparison. Source/host checks do not establish any of those results.
