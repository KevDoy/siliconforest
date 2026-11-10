# GPU desktop session on the M1: compositor on GL and page flips, driven by synthetic input

Boot `ws05` (10 November 2026). Collection: Wi-Fi + PDAgx + PDDcp on the patched kernel
(`integrations/puredarwin/m1-dcp/build-kc.nix --arg agx true --arg wifi true --arg slim true`).
Command: `WM_GL_STEP=session tools/wm_gl_boot.sh <new-name>`. Compositor from the local `wm-darwin` branch
(commit 2c140fe, not pushed; carried as `chonkstep-source.patch.gz`), installed as the default desktop binary.
Raw lines: `runs/m1-desktop-010/pcie-enum-attempt-ws05/guest/console.bin` (`WMSTAT`, `WMAUTO`).

The desktop binary chose `renderer=gl output=kms` by itself, started two terminals, and then fed itself synthetic
input through the same entry native HID records take (`CHONKSTEP_DARWIN_AUTOPILOT=1`; compositor-generated events,
not physical input):

| Phase | Result |
|---|---|
| Pointer sweep, 8 s | 479 frames, 59.8 fps, 0 missed refreshes, composite 1.63 ms average |
| Window drag, 4 s | window moved 240,117 for a pointer move of 240,120; 52.9 fps, 1 missed refresh |
| Typing `echo wmgl` + Enter into the terminal | 20 key events; the terminal's own buffer changed in 924 pixels |
| Screenshot check after each phase | GL frame read back equals the CPU renderer's frame of the same scene: 0 of 4,096,000 pixels differ, four times |

First frame: 148 ms composite and 174 ms present (texture uploads, first pipeline compiles, the blocking mode set).
Steady state: about 1.6 ms composite and 0.23 ms for the commit call. One composite of 43 ms and one commit of 24 ms
occurred; causes not examined.

The `CODE SIGNING: cs_invalid_page … allowing` line appears for the compositor as it does for `launchd` and
`launchctl` on every boot; the address lies in the page before the run-time-loaded libdrm mapping.

Not established: nobody watched the panel and no physical key or trackpad input was used in this boot; Wi-Fi was
not joined (firmware not staged in this test); applications still render on the CPU; sessions are capped at 180 s
by the bench.
