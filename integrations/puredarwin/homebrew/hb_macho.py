# SPDX-License-Identifier: MIT
"""Mach-O reading and the two edits a Homebrew pour makes, in plain Python (host side).

Reading: load commands (linked libraries with their ordinals, build version, rpaths, fixup
format, code signature), imported symbols with the library each is bound to (two-level
namespace ordinal in the symbol table), exported symbols (export trie, re-exports named).

Editing, as `brew` does after unpacking a bottle:
  * rewrite the `@@HOMEBREW_PREFIX@@` / `@@HOMEBREW_CELLAR@@` placeholders in LC_ID_DYLIB,
    LC_LOAD_*_DYLIB and LC_RPATH (the job of install_name_tool / ruby-macho),
  * bring the ad-hoc code signature back in line with the changed bytes (the job of
    `codesign --sign - --force`): the page hashes of every CodeDirectory are recomputed in
    place. Nothing else of the signature changes (identifier, flags, special slots stay).

Only 64-bit little-endian images; a universal file is read through its arm64 slice and is
refused for editing.
"""
import hashlib
import struct

MH_MAGIC_64 = 0xfeedfacf
FAT_MAGIC, FAT_MAGIC_64 = 0xcafebabe, 0xcafebabf
CPU_ARM64 = 0x0100000c
LC_SEGMENT_64, LC_SYMTAB, LC_DYSYMTAB = 0x19, 0x2, 0xb
LC_LOAD_DYLIB, LC_ID_DYLIB, LC_LOAD_WEAK_DYLIB = 0xc, 0xd, 0x80000018
LC_REEXPORT_DYLIB, LC_LAZY_LOAD_DYLIB, LC_LOAD_UPWARD_DYLIB = 0x8000001f, 0x20, 0x80000023
LC_RPATH, LC_CODE_SIGNATURE = 0x8000001c, 0x1d
LC_DYLD_INFO, LC_DYLD_INFO_ONLY = 0x22, 0x80000022
LC_VERSION_MIN_MACOSX, LC_BUILD_VERSION = 0x24, 0x32
LC_DYLD_EXPORTS_TRIE, LC_DYLD_CHAINED_FIXUPS = 0x80000033, 0x80000034
DYLIB_KINDS = {LC_LOAD_DYLIB: 'load', LC_LOAD_WEAK_DYLIB: 'weak', LC_REEXPORT_DYLIB: 'reexport',
               LC_LAZY_LOAD_DYLIB: 'lazy', LC_LOAD_UPWARD_DYLIB: 'upward'}
FILETYPES = {1: 'object', 2: 'execute', 6: 'dylib', 7: 'dylinker', 8: 'bundle', 9: 'dylib_stub', 10: 'dsym', 11: 'kext'}
PLATFORMS = {1: 'macos', 2: 'ios', 6: 'maccatalyst'}


def is_macho(head):
    """True for a thin 64-bit image or a universal file (not a Java class: those share the
    universal magic but have a tiny 'architecture count' that is really a version)."""
    if len(head) < 8:
        return False
    if struct.unpack_from('<I', head)[0] == MH_MAGIC_64:
        return True
    magic, n = struct.unpack_from('>II', head)
    return magic in (FAT_MAGIC, FAT_MAGIC_64) and 0 < n < 20


def version(v):
    return '%d.%d' % (v >> 16, (v >> 8) & 0xff) + ('.%d' % (v & 0xff) if v & 0xff else '')


def cstr(data, off):
    end = data.find(b'\0', off)
    return data[off:end if end >= 0 else len(data)].decode('utf-8', 'replace')


def uleb(data, off):
    result = shift = 0
    while True:
        b = data[off]
        off += 1
        result |= (b & 0x7f) << shift
        shift += 7
        if not b & 0x80:
            return result, off


class MachO:
    def __init__(self, data, path=''):
        self.path = path
        self.fat = False
        self.base = 0
        if struct.unpack_from('>I', data)[0] in (FAT_MAGIC, FAT_MAGIC_64):
            self.fat = True
            wide = struct.unpack_from('>I', data)[0] == FAT_MAGIC_64
            n = struct.unpack_from('>I', data, 4)[0]
            found = None
            for i in range(n):
                if wide:
                    cpu, _, off, size = struct.unpack_from('>iiQQ', data, 8 + i * 32)
                else:
                    cpu, _, off, size = struct.unpack_from('>iiII', data, 8 + i * 20)
                if cpu == CPU_ARM64:
                    found = (off, size)
            if not found:
                raise ValueError('no arm64 slice')
            self.base = found[0]
            data = data[found[0]:found[0] + found[1]]
        self.data = data
        magic, self.cputype, self.cpusubtype, self.filetype, self.ncmds, self.sizeofcmds, self.flags, _ = struct.unpack_from('<IiiIIIII', data)
        if magic != MH_MAGIC_64:
            raise ValueError('not a 64-bit Mach-O')
        self.twolevel = bool(self.flags & 0x80)     # MH_TWOLEVEL; Go links flat: a symbol may come from any linked library
        self.commands = []          # (cmd, offset, size)
        self.dylibs = []            # dict(kind, name, compat, current) in ordinal order
        self.id = None
        self.rpaths = []
        self.platform = self.minos = self.sdk = None
        self.chained = self.dyld_info = False
        self.codesig = self.symtab = self.dysymtab = self.exports = None
        self.first_section = None
        off = 32
        for _ in range(self.ncmds):
            cmd, size = struct.unpack_from('<II', data, off)
            self.commands.append((cmd, off, size))
            if cmd in DYLIB_KINDS or cmd == LC_ID_DYLIB:
                noff, _ts, cur, compat = struct.unpack_from('<IIII', data, off + 8)
                d = dict(kind=DYLIB_KINDS.get(cmd, 'id'), name=cstr(data, off + noff), current=version(cur), compat=version(compat))
                if cmd == LC_ID_DYLIB:
                    self.id = d
                else:
                    self.dylibs.append(d)
            elif cmd == LC_RPATH:
                self.rpaths.append(cstr(data, off + struct.unpack_from('<I', data, off + 8)[0]))
            elif cmd == LC_BUILD_VERSION:
                plat, minos, sdk = struct.unpack_from('<III', data, off + 8)
                self.platform, self.minos, self.sdk = PLATFORMS.get(plat, str(plat)), version(minos), version(sdk)
            elif cmd == LC_VERSION_MIN_MACOSX:
                minos, sdk = struct.unpack_from('<II', data, off + 8)
                self.platform, self.minos, self.sdk = 'macos', version(minos), version(sdk)
            elif cmd == LC_CODE_SIGNATURE:
                self.codesig = struct.unpack_from('<II', data, off + 8)
            elif cmd == LC_SYMTAB:
                self.symtab = struct.unpack_from('<IIII', data, off + 8)
            elif cmd == LC_DYSYMTAB:
                self.dysymtab = struct.unpack_from('<IIIIII', data, off + 8)
            elif cmd == LC_DYLD_CHAINED_FIXUPS:
                self.chained = True
            elif cmd == LC_DYLD_EXPORTS_TRIE:
                self.exports = struct.unpack_from('<II', data, off + 8)
            elif cmd in (LC_DYLD_INFO, LC_DYLD_INFO_ONLY):
                self.dyld_info = True
                f = struct.unpack_from('<10I', data, off + 8)
                if f[9]:
                    self.exports = (f[8], f[9])
            elif cmd == LC_SEGMENT_64:
                nsects = struct.unpack_from('<I', data, off + 64)[0]
                for i in range(nsects):
                    s = off + 72 + i * 80
                    ssize, soff = struct.unpack_from('<QI', data, s + 40)
                    stype = struct.unpack_from('<I', data, s + 64)[0] & 0xff
                    if soff and ssize and stype not in (1, 12, 18):      # zerofill kinds have no file bytes
                        self.first_section = soff if self.first_section is None else min(self.first_section, soff)
            off += size

    # ---- reading ---------------------------------------------------------------------
    def imports(self):
        """[(symbol, ordinal, weak)]: undefined external symbols. ordinal 1.. indexes
        self.dylibs; 0 self, 254 flat lookup, 255 main executable."""
        if not self.symtab or not self.dysymtab:
            return []
        symoff, nsyms, stroff, _strsize = self.symtab
        iundef, nundef = self.dysymtab[4], self.dysymtab[5]
        out = []
        for i in range(iundef, min(iundef + nundef, nsyms)):
            strx, ntype, _sect, desc, _value = struct.unpack_from('<IBBHQ', self.data, symoff + i * 16)
            if ntype & 0xe0 or (ntype & 0x0e) != 0 or not ntype & 1:
                continue
            out.append((cstr(self.data, stroff + strx), (desc >> 8) & 0xff, bool(desc & 0x40)))
        return out

    def exported(self):
        """({symbol}, {symbol: (ordinal, imported name)}) from the export trie: symbols this
        image defines, and symbols it re-exports individually from another library."""
        own, re = set(), {}
        if not self.exports:
            return own, re
        start, size = self.exports
        trie = self.data[start:start + size]
        stack = [(0, '')]
        seen = set()
        while stack:
            off, prefix = stack.pop()
            if off in seen or off >= len(trie):
                continue
            seen.add(off)
            tsize, p = uleb(trie, off)
            if tsize:
                flags, q = uleb(trie, p)
                if flags & 0x08:                         # EXPORT_SYMBOL_FLAGS_REEXPORT
                    ordinal, q = uleb(trie, q)
                    name = cstr(trie, q)
                    re[prefix] = (ordinal, name or prefix)
                else:
                    own.add(prefix)
            p += tsize
            n = trie[p]
            p += 1
            for _ in range(n):
                end = trie.index(b'\0', p)
                edge = trie[p:end].decode('utf-8', 'replace')
                child, p = uleb(trie, end + 1)
                stack.append((child, prefix + edge))
        return own, re

    def signature(self):
        """None, or dict(flags, identifier, adhoc, linker_signed, hash types, code limit)."""
        if not self.codesig:
            return None
        off, size = self.codesig
        blob = self.data[off:off + size]
        if len(blob) < 12 or struct.unpack_from('>I', blob)[0] != 0xfade0cc0:
            return dict(error='no embedded signature superblob')
        count = struct.unpack_from('>I', blob, 8)[0]
        out = dict(directories=[], cms=False)
        for i in range(count):
            typ, boff = struct.unpack_from('>II', blob, 12 + i * 8)
            magic, length = struct.unpack_from('>II', blob, boff)
            if magic == 0xfade0c02:
                ver, flags, hash_off, ident_off, nspecial, ncode, limit, hsize, htype, _plat, pshift = struct.unpack_from('>IIIIIIIBBBB', blob, boff + 8)
                out['directories'].append(dict(at=off + boff, version=ver, flags=flags, hash_offset=hash_off, slots=ncode,
                                               code_limit=limit, hash_size=hsize, hash_type=htype, page_shift=pshift))
                out.setdefault('flags', flags)
                out.setdefault('identifier', cstr(blob, boff + ident_off))
            elif magic == 0xfade0b01 and length > 8:
                out['cms'] = True
        out['adhoc'] = bool(out.get('flags', 0) & 0x2)
        out['linker_signed'] = bool(out.get('flags', 0) & 0x20000)
        return out


def load(path):
    with open(path, 'rb') as f:
        return MachO(f.read(), str(path))


# ---- editing -------------------------------------------------------------------------
def rewrite_names(data, replace):
    """Return (new bytes, changed) with replace(str)->str applied to every install name and
    rpath. Load commands are re-laid-out; the result must still fit before the first
    section (bottles are linked with -headerpad_max_install_names)."""
    m = MachO(data)
    if m.fat:
        raise ValueError('universal file: not edited')
    out, changed = bytearray(), False
    for cmd, off, size in m.commands:
        raw = data[off:off + size]
        if cmd in DYLIB_KINDS or cmd in (LC_ID_DYLIB, LC_RPATH):
            noff = struct.unpack_from('<I', raw, 8)[0]
            old = cstr(raw, noff)
            new = replace(old)
            if new != old:
                changed = True
                body = raw[:noff] + new.encode() + b'\0'
                body += b'\0' * (-len(body) % 8)
                raw = body[:4] + struct.pack('<I', len(body)) + body[8:]
        out += raw
    if not changed:
        return data, False
    limit = m.first_section if m.first_section is not None else 32 + m.sizeofcmds
    if 32 + len(out) > limit:
        raise ValueError('load commands do not fit: need %d, have %d' % (32 + len(out), limit))
    new = bytearray(data)
    new[32:32 + max(len(out), m.sizeofcmds)] = bytes(out) + b'\0' * max(0, m.sizeofcmds - len(out))
    struct.pack_into('<I', new, 20, len(out))
    return bytes(new), True


HASHES = {1: lambda b: hashlib.sha1(b).digest(), 2: lambda b: hashlib.sha256(b).digest(),
          3: lambda b: hashlib.sha256(b).digest()[:20], 4: lambda b: hashlib.sha384(b).digest()}


def resign(data):
    """Recompute the code-page hashes of every CodeDirectory in place. Returns (bytes,
    slots changed, note). Only for ad-hoc signatures: a signature with a CMS blob (a real
    identity) cannot be repaired this way and is reported instead."""
    m = MachO(data)
    sig = m.signature()
    if sig is None:
        return data, 0, 'unsigned'
    if 'error' in sig:
        return data, 0, sig['error']
    if sig['cms'] or not sig['adhoc']:
        return data, 0, 'not ad-hoc (identity-signed): left alone'
    new = bytearray(data)
    changed = 0
    for d in sig['directories']:
        h = HASHES.get(d['hash_type'])
        if h is None:
            return data, 0, 'unknown hash type %d' % d['hash_type']
        page = 1 << d['page_shift'] if d['page_shift'] else d['code_limit']
        for i in range(d['slots']):
            start = i * page
            digest = h(bytes(new[start:min(start + page, d['code_limit'])]))[:d['hash_size']]
            at = d['at'] + d['hash_offset'] + i * d['hash_size']
            if new[at:at + d['hash_size']] != digest:
                new[at:at + d['hash_size']] = digest
                changed += 1
    return bytes(new), changed, 'ad-hoc'


def verify(data):
    """Number of code pages whose hash does not match (0 = the signature covers the bytes)."""
    return resign(data)[1]
