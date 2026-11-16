# dma-buf fence audit against the deployed kernel

The request to add missing sync-file ioctls was based on an older source audit.
The actual kernel 47 AGX/DCP trees already include layer patches 0021, 0022,
0024 and 0026: export/import ioctls, poll callbacks, cross-kext sync-file
recognition and owner dispatch for the ioctl argument buffer. Their reference
is `sources/gpu-plan/linux-aurora`, commit `6a08a08e`, specifically
`drivers/dma-buf/dma-buf.c` and `sync_file.c`. No replacement success stub or
new fence implementation has been added here.

On 2026-11-16 the existing `m1-vulkan/test/sync-probe.c` was run on the actual
47 kernel with an exact runtime guard. It returned zero failures: timeline
operations, sync-file export/import and POLLIN, dma-buf export/import and
POLLIN/POLLOUT, and SCM_RIGHTS transfer all passed. Clone slots remained three
used, zero failures. This probe uses signalled fences and no GPU jobs. The
missing-ioctl premise is disproven; pending-fence ordering is a separate test.
CODEX-FX withdrew its unsupported missing-ioctl runtime claim.

Tiny existing probe (take the GUI/bench locks; no window or input):

```sh
env DYLD_LIBRARY_PATH=/private/var/db/sfvk/lib /private/var/db/sfvk/bin/sync-probe
```

`test-source.py` compiles the actual frozen ioctl implementation under
ASan/UBSan with fake provider fault/refcount controls. It checks flags,
READ-versus-WRITE filtering, descriptor reservation and EFAULT/ENOMEM/EMFILE
cleanup, and pending-fence references. Stub fences are appropriate only for
an empty reservation. This does not test GPU completion, poll wake-ups,
concurrent reservation updates or real descriptor lifetime.

`pending-probe.c` is a separate native probe, not a kernel change. A real
Vulkan transfer waits on an initially-unsignalled host timeline. Its completion
sync file is imported as a dma-buf writer fence and exported as a reader's
reservation snapshot. A separately executed child receives both descriptors
through SCM_RIGHTS, must observe them not ready, then polls and verifies every
word of the GPU-written 4 KiB pattern after the parent signals the timeline.
A five-second release watchdog prevents the probe deliberately leaving a job
waiting on its host signal. The queue is drained before storage teardown.
The original completion fd is closed after obtaining the reservation snapshot,
which must retain the fence. This first probe tests a GPU producer and CPU
consumer; it does not yet qualify a subsequent GPU consumer, IOSurface broker
leases, resize, image reuse or producer death.

The probe builds against the existing staged Vulkan loader, whose renamed
`libvulkan.1.dylib` requires an explicit load-command rewrite and code-signature
rehash. The first native attempt failed in dyld before reaching the driver
because it requested the original versioned loader name; that failure is
retained, not a fence failure. Earlier SDK preparation failures (deprecated
linker flag, SDK feature macros and unavailable dsymutil) are also retained.
The fresh attempt created a real Vulkan device, but the five-second release
watchdog fired before the child zero-time poll, which then found the fence
ready. That pending gate failed and is retained. A timed variant now records
queue submission, sync-file export, reservation import/export, spawn and child
poll separately to distinguish a synchronous operation from startup delay.
No pending-fence success or kernel root cause is claimed yet.

Do not remove CGL's `glFinish` or completion/VideoToolbox leases based on an
already-signalled probe. CODEX-FX and TERM11 have the live UAPI result and can
prepare the Mesa/EGL native-fence and window-export paths, with activation
following real producer/consumer data and lifetime checks. No new kernel boot
is required merely to introduce these already-live ioctl commands.
