#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate a tiny original one-page PDF; no external fonts or image assets."""
from pathlib import Path
import sys
p = Path(sys.argv[1])
if p.exists():
    raise ValueError('Fixture destination must be new')
stream = b'BT /F1 6 Tf 10 20 Td (Finch PDF fixture) Tj ET\n'
objects = [b'<< /Type /Catalog /Pages 2 0 R >>',
    b'<< /Type /Pages /Count 1 /Kids [3 0 R] >>',
    b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 72 72] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>',
    b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>',
    b'<< /Length '+str(len(stream)).encode()+b' >>\nstream\n'+stream+b'endstream',
    b'<< /Title (Finch fixture) >>']
data = bytearray(b'%PDF-1.4\n%\xe2\xe3\xcf\xd3\n')
offsets = [0]
for i, obj in enumerate(objects, 1):
    offsets.append(len(data)); data += str(i).encode()+b' 0 obj\n'+obj+b'\nendobj\n'
xref = len(data)
data += b'xref\n0 7\n0000000000 65535 f \n'
for offset in offsets[1:]: data += f'{offset:010d} 00000 n \n'.encode()
data += f'trailer\n<< /Size 7 /Root 1 0 R /Info 6 0 R >>\nstartxref\n{xref}\n%%EOF\n'.encode()
p.write_bytes(data)
