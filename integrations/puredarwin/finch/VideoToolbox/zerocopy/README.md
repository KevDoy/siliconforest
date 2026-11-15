# AVD capture sharing, staged source

The current installed VT decoder copies the PDAVD read-only NV12 picture into
its CoreVideo buffer. CODEX-FX owns IOSurface/CGL GPU allocation and import; this
stream owns the producer. A GPU-backed destination with a CPU row copy is not
an end-to-end zero-copy decoder. No such claim is made here.

The optional ABI1 extension in m1-avd preserves existing structs/ioctls/defaults:
PDAVD_CAP_EXTERNAL_BUFFERS (bit16) advertises PDAVD_IOC_IMPORT. One request names
ALL context capture buffers, before any MAP or decode/HOLD. Unsupported kernels
return ENOTTY; recording mode returns ENOTSUP. The producer must explicitly opt
in and retain its mappings until context destruction.

Admission requires the same owner, exact count, zero reserved fields,16KiB
aligned writable current-task ranges, complete driver sizeimage/private-tail
capacity, no arithmetic/virtual overlap and no physical-page aliases. The
kernel prepares/maps every descriptor first, rejects overlap with all existing
AVD DART pages, then maps unused DART IOVAs. Only a fully prepared set reaches
an allocation-free core commit. Failure releases the new pins/maps and retains
the previous capture set. Old buffers are freed only after successful commit.
One-time zero initialization matches original capture allocation; no picture
bytes are copied by the import. Page pinning is InOut (writable reference),
as implemented by the retained IOGeneralMemoryDescriptor. Actual GPU-map pinning
and cache coherence still require a kernel-owner hardware trial.

The host validator tests ABI528 bytes, malformed counts/flags/sizes/addresses,
virtual overlap and10000 physical-page sort/alias/search cases under ASan/UBSan.
The actual core replay tests both ordinary and imported-model capture sets,
a late invalid entry without partial commit, repeat-import refusal and refusal
after decode/HOLD. H264/HEVC/VP9 each300pictures and multi-slice H26460pictures
complete with zero model allocations left. Each instruction log repeats exactly
within its mode; imported IOVAs differ from ordinary allocation by design.
These are model results, not hardware decode or GPU imports. Initial invocations
accidentally passed relative recordings to a script that changed directory;
their vacuous results are retained, not called successful replays. The script
now resolves explicit inputs first and rejects missing inputs.

Linux/Nix PDAppleAVD kext SDK build passes; no collection was built or installed.
CODEX-TB owns kernel review/inclusion/boot. The initial v1 artifact is superseded for hardware review by the safety
follow-up described below; kernel-owner
handover is requests-for-CODEX-TB.md. AVE remains paused and untouched.

Next producer contract (not yet qualified): GPU-backed NV12 IOSurface,420v/420f,
header/data offset16384, Yplane offset0 and UVplane driver uv_offset, driver
stride and private-tail capacity, visible cropped dimensions. CoreVideo wraps
the IOSurface without a row copy. A retained AVFrame lease and producer usecount
must bridge into Firefox's cross-process MacIOSurface usecount, with CGL holding
its count until GPU completion. Count zero alone is not a GPU fence. Pool
pressure, process death and flush/reset lifetime must be tested before enabling
this path. CODEX-FX is implementing the single NV12 IOSurface/CGL path.

Evidence: runs/sffinch/avd-external-host-{legacy,import}-v2.log and the host test
binary/import source here. No1080p60 result, browser smoothness or GPU-copy
claim follows from these builds/model tests.

Safety follow-up: a DART invalidate timeout now poisons the mapper, retains
IOVA reservations and all prepared mappings/descriptors, and rejects further
map/allocate/decode work until reboot. Partial rollback has the same policy.
Context ownership retains task identity in addition to PID. The actual source
extraction fixture injects failures and checks no completion/release or IOVA
reuse before invalidation; this is logical host coverage, not hardware safety.

The public lifetime probe originally used GetUseCount to decide recycling. A
real native cross-process test caught early AVBuffer release while the child
still held its surface. That failure is retained. The corrected bridge uses
IOSurfaceIsInUse: native GetUseCount is process-local, whereas IsInUse observes
any process ([public API definitions](https://developer.apple.com/documentation/iosurface/iosurface-functions?language=objc)).
With an explicit child-held handshake, the parent sees local count0 and in-use1
while retaining the frame; after child release it sees in-use0 and releases
exactly once. This does not prove a GPU fence or prevent a consumer that first
acquires its hold after producer release. Consumer handoff ordering and
completion-held counts remain prerequisites. FX owns the one IOSurface/CGL
implementation; its shared numeric count differs from native local diagnostics.
