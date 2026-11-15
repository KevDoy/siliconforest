# External DCP / DPTX port (SF-AF milestone 2, in progress)

`src/pd_dcpext_plan.c` validates the real J293/T8103 ADT resource layout
before an external DCP instance can be published. It checks dcpext, five
dispext windows, DCP/display DARTs, bandwidth scratch/doorbell, mailbox IRQs
and the two external firmware carve-outs (73/74), and restricts firmware to
the installed 13.5-compatible ABI. Unknown/partial data leaves the output
unchanged. It performs no MMIO and invents no firmware IOVAs or EDID.

The target is port 1, physical ATC/DPTX index 1, remote DCP core 0, one
external stream. The display PIODMA uses stream 4 of the display DART; it
must not be confused with the second ADT register bank. The resource plan
is not yet a synthetic device tree or a driver attachment.

The actual directly ported T8103 DP AUX/link-rate and Thunderbolt pixel
clock programs live in `../m1-atcphy`; Thunderbolt DP-IN lifecycle/handshake
lives in `../m1-thunderbolt`. Their host tests do not establish a trained
link, powered external DCP, scanout or a second compositor output.

Next integration:

- publish external DCP/mailbox/DART/clock/firmware and port-1 route nodes;
- bind `/arm-io/dcpext` to its own RTKit device, rather than the internal
  DCP's hardcoded mailbox path; serialize platform initialization;
- supply real generic-PHY, crossbar and Type-C mux providers to the existing
  Linux DPTX endpoint code, with cable-generation/lifetime checks;
- extend display DMA beyond its current first-`iommus` binding so internal
  and external scanout see the same framebuffer IOVA in both DARTs;
- preserve mappings/backing if a DART invalidate fails; current DCP DMA
  free paths ignore this failure and are insufficient for external teardown;
- start the M1 external IOMFB pipeline, then observe HPD/training/EDID/mode
  and a connector before DESK4's multi-output compositor integration.

The `dcpext_scanout` stubs in the present DCP bundle concern later-chip
native external pipelines; T8103's normal IOMFB path is already compiled.
That does not make the missing PHY/topology/DMA plumbing optional.

`host-test/run.sh` uses synthetic ADT resources and ASan/UBSan: valid layout,
223 absent/truncated property cases, overflow and unsupported ABI/chip
refusals. No live firmware/address fixture is checked in.
