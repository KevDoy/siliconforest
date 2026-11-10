# Display coprocessor: first contact from Darwin (SF-H H1)

Boot `dc04` (10 November 2026), `PDRtkitProbe` with `pdrtkitprobe=3`, collection from
`integrations/puredarwin/m1-gpu-rtkit/build-dcp-probe-kc.nix`, hypervisor-side read with `PCIE_DCP_PROBE=dcp`.
Sequence taken from Omacom `linux-aurora` `integration/aurora` at `6a08a08e` (`dcp.c:3996`, `:4014`).
Raw lines: `runs/m1-desktop-010/pcie-enum-attempt-dc04/guest/console.bin` (`PDRtkitProbe: dcp`).

Measured:
- Before any kernel access: `DISP0_FE` and `DISP0_CPU0` powered, ASC `CPU_CONTROL` 0x10 (RUN set), status running and idle, both mailbox FIFOs empty.
- The firmware answered the wake: RTKit protocol 12, 21 endpoints. System: 0x00 0x01 0x02 0x03 0x04 0x08 0x0a. Application: 0x20 system, 0x21, 0x22, 0x23 disp0/iBoot, 0x24, 0x25-0x27 (unnamed in Linux), 0x28 dpavserv, 0x29 av, 0x2a dptx, 0x2b hdcp, 0x2c, 0x37 iomfb.
- Two buffer requests arrived and were deliberately not served:
  - crashlog (0x01): 0x10000 bytes at iova 0xf00267000, placed by the firmware. Linux translates it through the `dcp` DART and sends no reply.
  - ioreport (0x04): 0x4000 bytes, iova 0. The AP must allocate, map through the `dcp` DART and reply.
- Because the ioreport request was not answered, the firmware never acknowledged the IOP power state and `apple_rtkit_wake` timed out (-60). This was the predicted outcome without a DART.
- Interrupts: 4 receive interrupts on AIC 430, all handled. No crash report. Registers after: unchanged, FIFOs empty.
- The guest ran to the end of the session and the M1 returned to Linux.

Not established: nobody was watching the panel, so whether the picture stayed up while the coprocessor sat half-woken is unknown. The oslog request m1n1 saw (0x3ff8 bytes at 0xf0025f000) did not arrive before the stall.

What H2 has to provide, as measured: translation of firmware-placed iovas through the `dcp` DART (which m1n1 reports as locked), and allocation plus mapping of at least one 0x4000-byte buffer in that DART, with a reply.
