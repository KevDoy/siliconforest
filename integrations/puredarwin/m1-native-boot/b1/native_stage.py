# SPDX-License-Identifier: MIT
"""The launcher's `stage` mode (native_mode.txt: `native stage ...`): boot through b1/'s stage.

Placed next to host_progress.py with native_boot.py, pdsf_pack.py and m1n1-sf-stage.bin
(b1/build.sh). launcher-native.patch calls start() once the proxy has been collected, instead
of everything the launcher otherwise does to memory, the device tree and the boot arguments.

One upload and one vector: the stage m1n1 with its payload records (configuration, the
collection, the RAM root gzipped) is written into the launcher's own part of m1n1's RAM,
m1n1 is vectored into it, and from then on this launcher only listens (and types the console
challenge) on the M1's real UART. The stage does the rest by itself: see b1/DESIGN.md.
RAM only: nothing is written to the M1's storage or NVRAM, by this file or by the stage.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import struct
import time

import native_boot
import pdsf_pack

HERE = Path(__file__).resolve().parent
IMAGE = 'm1n1-sf-stage.bin'
ALIGN, ENTRY, PAGE = 0x10000, 0x800, 0x4000
BOOTARGS_TOP = 32                       # struct boot_args: top_of_kernel_data (m1n1 src/startup.c)
CHUNK = 4 << 20
require, mark = native_boot.require, native_boot.mark


def config(native, bootargs, usb_index, seed=None, xnu_base=None):
    return pdsf_pack.config_text(
        bootargs, display=1, usb_nodes='keep' if native.usb_keep else 'remove%d' % usb_index,
        sprr=int(native.sprr), amx=int(native.amx), wdt=native.wdt,
        seed_hex=seed.hex() if seed is not None else None,
        xnu_base='%#x' % xnu_base if xnu_base is not None else None)


def start(native, backend, inputs, bootargs, raw_bootargs, layout, directory, runtime, console_factory,
          write_private):
    """Upload, vector, capture the real serial console for `runtime` seconds (or until the
    guest script's end marker). `bootargs` is the kernel command line, `raw_bootargs` the bytes
    of the boot arguments this m1n1 was started with (as collected), `layout` the launcher's
    own prediction for a proxy-loaded boot: without the word lowmem the stage is given its
    phys_base, and must then arrive at the same entry point and boot-arguments address."""
    p, iface, u = backend.p, backend.iface, backend.u
    m1n1 = (HERE / IMAGE).read_bytes()
    index = int(p.iodev_whoami()) - int(backend.proxy_module.IODEV.USB0)
    require(index in (0, 1), 'The proxy is not on a USB port')
    cpus = [token for token in bootargs.split() if token.startswith('cpus=')]
    require(cpus == ['cpus=1'] or 'pdcpuheld=1' in bootargs.split(),
            'More than one processor through the stage needs pdcpuheld=1')
    mark('stage_pack_begin')
    seed = os.urandom(256) if native.host_seed else None
    xnu_base = None if native.low_mem else layout['phys_base']
    text = config(native, bootargs, index, seed, xnu_base)
    image = pdsf_pack.image(m1n1, text, inputs.contents['kernel'], inputs.contents.get('ramdisk'))
    at = pdsf_pack.records_offset(image)
    found, _ = pdsf_pack.parse(image, at)
    require(at == len(m1n1) and all(item[4] for item in found), 'Stage image self-check')
    mark('stage_pack_end', bytes=len(image))

    # The image runs where it is uploaded (m1n1 is position independent). It is entered with
    # a copy of this m1n1's boot arguments that differs in one field: top_of_kernel_data is
    # put behind the image, which is where the stage's heap then starts.
    size = (len(image) + PAGE - 1) & ~(PAGE - 1)
    total = size + PAGE
    base = u.heap.memalign(ALIGN, total)
    require(base and base % ALIGN == 0 and backend.ba.phys_base <= base and
            base + total <= backend.ba.phys_base + backend.ba.mem_size, 'No place for the stage image')
    require(xnu_base is None or base + total + (128 << 20) <= xnu_base,
            "The stage image does not fit below the launcher's phys_base")
    old_top, = struct.unpack_from('<Q', raw_bootargs, BOOTARGS_TOP)
    require(old_top == backend.ba.top_of_kernel_data and old_top <= base,
            'Boot arguments differ from the collected ones')
    require(len(raw_bootargs) <= PAGE, 'Boot arguments larger than a page')
    entered = bytearray(raw_bootargs.ljust(PAGE, b'\0'))
    struct.pack_into('<Q', entered, BOOTARGS_TOP, base + total)
    entered = bytes(entered)

    mark('stage_upload_begin', bytes=len(image))
    for offset in range(0, len(image), CHUNK):
        iface.writemem(base + offset, image[offset:offset + CHUNK])
    if size > len(image):
        iface.writemem(base + len(image), bytes(size - len(image)))
    iface.writemem(base + size, entered)
    mark('stage_upload_end')
    # The stage checks every record's CRC itself; the code that does it is read back here.
    require(iface.readmem(base, len(m1n1)) == m1n1 and iface.readmem(base + size, PAGE) == entered,
            'Stage image readback differs')
    p.dc_cvau(base, total)
    p.ic_ivau(base, len(m1n1))

    record = dict(stage=True, hypervisor=False, base=base, entry=base + ENTRY, bootargs=base + size,
                  image_bytes=len(image), image_sha256=hashlib.sha256(image).hexdigest(),
                  m1n1_sha256=hashlib.sha256(m1n1).hexdigest(),
                  records=[dict(type=pdsf_pack.NAMES[kind], gzip=bool(flags & pdsf_pack.F_GZIP), stored=stored, raw=raw)
                           for kind, flags, stored, raw, _, _ in found],
                  config=[line for line in text.decode().splitlines() if not line.startswith('seed-hex=')],
                  seed='controller CSPRNG, in the configuration record' if seed is not None else 'SEP, by the stage',
                  usb_port=index, console=native_boot.UART_DEVICE, watchdog_seconds=native.wdt,
                  expected=(None if native.low_mem else dict(
                      xnu_base='%#x' % layout['phys_base'], entry='%#x' % layout['entry'],
                      bootargs='%#x' % (layout['guest_base'] + layout['bootargs_off']))),
                  watchdog_armed_by='the stage, just before it enters the kernel' if native.wdt else None)
    if native.vdm_serial:
        record['serial_mode_ok'] = native_boot.serial_mode()
    console = console_factory(native_boot.UART_DEVICE)
    console.dev = native_boot.PacedSerial(console.dev)
    try:
        write_private(directory, 'native-start.json', record)
        print('NATIVE_STAGE_START ' + json.dumps(record, default=hex), flush=True)
        mark('native_vector')
        # m1n1 replies, shuts its USB, display console and MMU down and jumps (src/main.c).
        # This is the last request of the boot: nothing is sent over the proxy afterwards.
        p.request(p.P_VECTOR, base + ENTRY, base + size, 0, 0, 0, 0)
        mark('native_vector_acknowledged')
        deadline = time.monotonic() + runtime
        # As native_boot.start(): a session started by launchd (multiuser) types no command,
        # so the console session does not look for the end marker itself.
        untyped, done = 'multiuser' in native.words, False
        while time.monotonic() < deadline and not done and console.thread.is_alive():
            time.sleep(.2)
            done = console.done_seen or (untyped and re.search(
                rb'(?:^|\n)PD_BENCH_DONE\n', bytes(console.proof.output[-4096:]).replace(b'\r', b'')) is not None)
        if done:
            time.sleep(1.0)
        mark('native_capture_end', done=done, console_bytes=len(console.proof.output))
    finally:
        error = None
        try:
            console.close()
        except Exception as failure:
            error = f'{type(failure).__name__}: {failure}'
        write_private(directory, 'capture-core.json', dict(
            status='NATIVE_XNU_CONSOLE_CAPTURED', native=True, hypervisor=False, stage=True, event=None,
            console='physical uart0 on ' + native_boot.UART_DEVICE,
            console_capture_finalized=error is None, console_close_error=error,
            diagnostics_complete=False, hardware_boot_observed=False))
