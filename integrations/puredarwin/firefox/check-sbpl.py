#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check permission narrowing in a host-built private SBPL compiler, not kernel enforcement."""
import ctypes
import json
import struct
import sys

lib = ctypes.CDLL(sys.argv[1])
compile = lib.sbpl_compile
compile.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p,
                    ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t),
                    ctypes.POINTER(ctypes.c_char_p)]
libc = ctypes.CDLL(None)
libc.free.argtypes = [ctypes.c_void_p]


def rules(source):
    blob, size, error = ctypes.c_void_p(), ctypes.c_size_t(), ctypes.c_char_p()
    status = compile(source.encode(), None, None, ctypes.byref(blob), ctypes.byref(size), ctypes.byref(error))
    if status != 0:
        raise RuntimeError(error.value)
    data = ctypes.string_at(blob, size.value)
    libc.free(blob)
    magic, version, default, count, strings_size, reserved = struct.unpack_from('<6I', data)
    assert magic == 0x42535044 and version == 1
    strings = data[24 + count * 16:]
    result = []
    for i in range(count):
        ops, action, filter, _, offset, length = struct.unpack_from('<IBBHII', data, 24 + i * 16)
        path = strings[offset:offset + length].rstrip(b'\0').decode() if length else ''
        result.append((ops, action, filter, path))
    return result


cases = [
    ('explicit', '(allow iokit-open (iokit-connection "IOAccelerator"))', True),
    ('deny', '(deny iokit-open (iokit-connection "IOAccelerator"))', False),
    ('other-service', '(allow iokit-open (iokit-connection "IOAudioEngine"))', False),
    ('other-operation', '(allow mach-lookup (iokit-connection "IOAccelerator"))', False),
    ('unfiltered', '(allow iokit-open)', False),
    ('intersection', '(allow iokit-open (require-all (iokit-connection "IOAccelerator") (literal "/other")))', False),
    ('unchecked-context', '(with-filter (require-all (iokit-connection "other")) (allow iokit-open (iokit-connection "IOAccelerator")))', False),
]
report = []
for name, source, expected in cases:
    try:
        emitted = rules('(version 1) (deny default) ' + source)
    except RuntimeError as error:
        if name != 'unchecked-context':
            raise
        report.append(dict(name=name, passed=True, rejected=str(error)))
        continue
    grants = [r for r in emitted if r[1] == 0 and r[3] == '/dev/pdavd']
    assert bool(grants) == expected, (name, emitted)
    if grants:
        assert grants == [(5, 0, 1, '/dev/pdavd')], grants
    report.append(dict(name=name, passed=True, avd_grants=grants))
print(json.dumps(dict(scope='host compiler permission checks only', tests=report), indent=2))
