# Detached PCI xHCI DMA helper

This is source preparation for the dock, excluded from every boot collection.
`build-dock-usb-dma-objects.nix` checks the helper against the actual native
USB SDK and exports only its object. It does not bind an xHCI controller or
make the old generic PCI allocation path safe.

The helper requires the actual shared PCIe-C IOMapper lookup and a fresh
readiness response for this endpoint. The native mapper now has an explicit registration method for up to four
actual dock xHCI endpoints on the same host, reserving their real SID0
requesters without resetting its root or IOVA allocator. No factory calls
that method yet or binds the actual mapper to USB devices; current boot
profiles therefore still refuse USB. It must never
initialize the direct USB1 DART or replace the active I210 root.

Each allocation owns whole16KiB physical pages and a real IODMACommand.
The driver must use `dmaAddress()`, explicitly call `retire()` after producer
completion/halt, then release its reference only on success. The command and
mapper retain the descriptor during mapping: implicit descriptor destruction
cannot break that ownership cycle. Every existing USB ring/payload release
site must therefore be converted before using this class. Failed prepare,
segment validation, complete or inactive-clear retains backing until reboot.

Still required: real shared-domain endpoint registration, retained per-MMIO
physical access guards, native BAR resources and posted-write mappings,
registered/armed MSI, checked late BME/run, all USB release sites and actual
IRQ/work/producer teardown. No Ethernet, keyboard, storage or hot-unplug
result follows from this source or a successful object build.

The native ARM64 object build passes; no controller or hardware test ran.
