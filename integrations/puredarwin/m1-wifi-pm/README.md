# Wi-Fi power-save candidate

FXNET's `cf9034f6b` adds boot defaults and a runtime power-save request to
PDBwfm. The new default keeps fast power save (mode 2), with
`pm2_sleep_ret=2000` milliseconds. It adds no polling thread or timer.
The existing firmware timer does the work. This trades a longer awake period
after traffic for avoiding a DTIM wait on replies arriving after 200 ms.
Idle power and real page-load improvement require measurement.

`fxnet-pm.patch` is exactly that commit's six driver-file changes, with paths
translated into the PureDarwin SDK tree. All six old files match the deployed
source byte for byte. The build freezes that tree, including its DART code.
A clean SDK rebuild reproduces the deployed raw PDBwfm executable byte for
byte; SHA-256 `d2ce5b1a189ea221437544ec7984ecd9b150e543153d5aa7ac0aa0e86e648a36`.
The userland tool remains FXNET's `m1-bwfm/tools/pd-wifipm`.

`pm-admission.patch` adds three narrow corrections:

- Reject unknown request versions, modes and timeout ranges before `chipUp`.
- Return EIO if the firmware rejects the timeout, even if the mode command
  succeeds. This can be a partial update; GET reports desired and actual
  values separately, and desired settings remain available for the next init.
- Preserve the boot/default timeout on the first offline mode change with
  timeout zero. Zero means preserve, rather than erase, the current target.

The request is a PureDarwin interface, not Apple's private ABI. SET takes
version/mode/timeout (12 bytes); GET returns those plus two signed firmware
readbacks (20 bytes). Unavailable firmware values are -1. `bwfm_pm=0|1|2`
and `bwfm_pm2_ret=0..60000` set boot defaults. Boot timeout zero leaves the
firmware's own value; runtime timeout zero preserves the cached target.
Use `pd-wifipm fast 200` to reproduce the previous timeout and `fast 2000`
to restore the new default without rebooting. `off` selects constantly awake.
A successful init log alone does not prove the timeout iovar was accepted:
check for the refusal log and obtain firmware GET readback.

`test-pm.py` extracts the actual patched boot, init, setter/getter and request
admission code and compiles it with ASan/UBSan against fake firmware/locks.
It checks default/range behavior, AP init, command ordering, running/offline/
bus-gone states, firmware failures and malformed requests before power-up.
These checks do not establish real firmware acceptance or concurrent hardware
lifecycle safety. SDK builds compile the full real driver.

`build-kc.nix` preserves qualified kernel 46 and changes only PDBwfm: 57
executable modules, 56 identical UUIDs including the core; only one kext
source changes. PCI/USB selections, original AVD, pinch/HID and killable waits
remain as qualified. No AVE or dock hardware trial is included. The candidate
keeps the same 599-byte command line, including `pdm1tpscroll=20`.

`build-qemu-image.nix` links the actual PM driver and its existing IO80211Family
into a disposable virt collection with the upstream PCI/USB and ext4 rehearsal.
QEMU has no matching Broadcom device: boot/USB/storage results cannot establish
Wi-Fi firmware behavior or M1 DMA/interrupt correctness. Private receipts are
under `runs/sftb/wifi-pm/`; installation and native results are recorded there
and in the coordination status, separately from source/build claims.

```sh
python3 integrations/puredarwin/m1-wifi-pm/test-pm.py
nix-build --extra-experimental-features 'nix-command flakes' integrations/puredarwin/m1-wifi-pm/build-driver.nix --arg port false --no-out-link
nix-build --extra-experimental-features 'nix-command flakes' integrations/puredarwin/m1-wifi-pm/build-kc.nix --no-out-link
python3 integrations/puredarwin/upstream-12377/run-qemu.py --image-recipe integrations/puredarwin/m1-wifi-pm/build-qemu-image.nix --output runs/sftb/wifi-pm/new-qemu-run
```
