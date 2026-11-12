#!/usr/bin/env python3
"""Disk images for the QEMU desktop loop (arm64 `virt`, the same binaries as the M1).

Two layers, both under runs/qemu-dev/ (never committed):

  template   the base image of build-image.nix (upstream's arm64-virt-minimal image, GPT: ESP +
             ext4 root, with our kernel collection and loader) with
               * the kernel's IOKitInitializeTime waits shortened (the M1 launcher's patch),
               * our boot arguments,
               * the slow-changing userland the M1 RAM root carries (fonts, xkb data, the
                 patched libSystem, audio server, terminfo, desk picture).
             Rebuilt only when one of those inputs changes (a hash of them names it).
  run image  a reflink copy of the template plus the files that change while working:
             compositor, terminal, session script, any --file extras. About a second.

Nothing is mounted: the ESP is edited with mtools, the root with debugfs, both at the
partition's offset inside the image file.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys

LAB = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = LAB / 'runs/qemu-dev'
SECTOR = 512
NIX = ['nix-build', '--extra-experimental-features', 'nix-command',
       '--extra-experimental-features', 'flakes', '--no-out-link']
ENV = dict(os.environ, PATH=str(Path.home() / '.nix-profile/bin') + ':' + os.environ['PATH'],
           NIX_CONFIG='build-users-group = ')
# no_lock_timeouts=1: upstream's switch for emulated guests on a busy host, where a descheduled
# vCPU thread otherwise reads as a spinlock deadlock (panic seen here at host load 40).
BOOTARGS = {'debug': 'debug=0x218 -nogzalloc_mode keepsyms=1 serial=3 gopconsole=1 serial_video_mirror=0 -noprogress no_interrupt_masked_debug=1 no_lock_timeouts=1',
            'release': 'serial=3 gopconsole=1 serial_video_mirror=0 -noprogress keepsyms=1 no_lock_timeouts=1'}


def sh(argv, **kw):
    return subprocess.run([str(a) for a in argv], check=True, text=True, capture_output=True, env=ENV, **kw).stdout


REFRESH = False


def nix(directory, *args):
    """A Nix build's output path. Evaluating a flake costs tens of seconds each time, so the
    answers are remembered in runs/qemu-dev/base/inputs.json until --refresh."""
    cache = OUT / 'base/inputs.json'
    known = json.loads(cache.read_text()) if cache.exists() else {}
    key = ' '.join([str(directory)] + [str(a) for a in args])
    if REFRESH or key not in known or not Path(known[key]).exists():
        known[key] = sh(NIX + list(args), cwd=LAB / directory).strip().splitlines()[-1]
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(json.dumps(known, indent=1) + '\n')
    return known[key]


def digest(path):
    with open(path, 'rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def partitions(image):
    """[(first byte, bytes)] of the GPT partitions, in table order."""
    with open(image, 'rb') as f:
        f.seek(SECTOR)
        header = f.read(92)
        assert header[:8] == b'EFI PART', 'not a GPT image'
        lba, count, size = struct.unpack_from('<QII', header, 72)
        f.seek(lba * SECTOR)
        out = []
        for _ in range(count):
            entry = f.read(size)
            if entry[:16] != bytes(16):
                first, last = struct.unpack_from('<QQ', entry, 32)
                out.append((first * SECTOR, (last - first + 1) * SECTOR))
    return out


def tool(name):
    """mtools from the Nix store (not installed on the host)."""
    found = shutil.which(name)
    if found:
        return found
    expr = '(import (builtins.getFlake "git+file://%s/sources/puredarwin-baseline?submodules=1&shallow=1&rev=%s").inputs.nixpkgs { system = "x86_64-linux"; }).mtools' % (LAB, PD_REV)
    return str(Path(nix('.', '--impure', '-E', expr), 'bin', name))


TEMPLATE_VERSION = 3    # raise when the template's construction changes
PD_REV = 'e7aa00a60f0b56ef35aa24252e81467787bf71ea'


def base_image(kernel):
    """Upstream's arm64-virt-minimal image with our kernel collection and loader
    (build-image.nix); built once by Nix. `release` has upstream's RELEASE kernel (several
    times faster under TCG), `debug` the DEBUG kernel."""
    override = os.environ.get('QD_BASE_IMAGE')
    if override:
        return Path(override)
    out = Path(nix('integrations/puredarwin/qemu-dev', 'build-image.nix', '--argstr', 'kernelConfig', kernel))
    return next(out.glob('*.img'))


def patch_time_waits(kernel, rtc, quiet):
    """As m1-bringup/host_progress.py patch_time_waits: the 30 s wait for an IORTC resource
    nobody publishes and the 30 s registry-quiet wait before the root mount, in the two code
    shapes seen so far."""
    def once(pattern, start=0, end=None):
        end = len(kernel) if end is None else end
        at = kernel.find(pattern, start, end)
        return at if at >= 0 and kernel.find(pattern, at + 4, end) < 0 else None
    out = bytearray(kernel)
    # DEBUG: mov w8,#30; stur w8,[x29,#-8]; stur wzr,[x29,#-4] ... ldr x8,[x8] (a literal {30,0})
    first = once(bytes.fromhex('c8038052' 'a8831fb8' 'bfc31fb8'))
    if first is not None:
        second = once(bytes.fromhex('080140f9' 'e9430091' 'e90700f9' 'e80b00f9'), first, first + 0x80)
        if second is None:
            return kernel, False
        out[first:first + 4] = struct.pack('<I', 0x52800008 | rtc << 5)        # mov w8,#rtc
        out[second:second + 4] = struct.pack('<I', 0xd2800008 | quiet << 5)    # mov x8,#quiet
        return bytes(out), True
    # DEVELOPMENT and RELEASE: mov w19,#30; str x19,[sp,#8] ... str x19,[sp]; bl; mov x1,sp.
    # One register feeds both waits.
    first = once(bytes.fromhex('d3038052' 'f30700f9'))
    if first is not None:
        second = once(bytes.fromhex('f30300f9'), first + 8, first + 0x40)
        if second is None or kernel[second + 8:second + 12] != bytes.fromhex('e1030091'):
            return kernel, False
        out[first:first + 4] = struct.pack('<I', 0x52800013 | max(rtc, quiet) << 5)   # mov w19,#each
        return bytes(out), True
    return kernel, False


def stable_payload(mesa):
    """Destination -> source for the slow-changing files. Mirrors tools/gpu_desktop.sh and the
    M1 RAM root's base mapping, without anything private (firmware) or M1-only (Wi-Fi join)."""
    files = {}
    base = json.loads((LAB / 'artifacts/m1-desktop-010/apcie-port-candidate-003/mapping.json').read_text())['files']
    skip = ('/usr/bin/darwin-desktop-test', '/usr/bin/darwin-apcie-port-observe', '/usr/bin/darwin-trackpad-enum',
            '/usr/bin/chonkstep-darwin')
    for e in base:
        if e['destination'] not in skip and not e['destination'].startswith('/Library/Firmware/'):
            files[e['destination']] = (e['source'], e.get('executable', False))
    cd = LAB / 'integrations/puredarwin/chonkstep-darwin'
    for d in ('/usr/share/terminfo/x/xterm-256color', '/usr/share/terminfo/78/xterm-256color'):
        files[d] = (cd / 'terminfo/xterm-256color', False)
    files['/usr/share/backgrounds/siliconforest/siliconforest.png'] = (cd / 'backgrounds/siliconforest.png', False)
    sound = 'integrations/puredarwin/m1-sound'
    a = Path(nix(sound + '/sysaudio', 'build-sysaudio.nix', '-A', 'all'))
    play = Path(nix(sound, 'build-pdplay.nix'))
    for rel in ('usr/sbin/coreaudiod', 'usr/bin/pdtone', 'usr/bin/pdvolume'):
        files['/' + rel] = (a / rel, True)
    files['/usr/bin/pdplay'] = (play / 'usr/bin/pdplay', True)
    for f in ('CoreAudio', 'AudioToolbox', 'AudioUnit'):
        rel = 'System/Library/Frameworks/%s.framework/Versions/A/%s' % (f, f)
        files['/' + rel] = (a / rel, True)
    if mesa:
        t = Path(nix('integrations/puredarwin/m1-mesa-asahi', 'agx-offscreen-test.nix'))
        for l in ('libEGL.1.dylib', 'libGLESv2.2.dylib', 'libgallium-26.3.0-devel.dylib', 'libdrm.2.dylib'):
            files['/usr/local/lib/mesa-asahi/' + l] = (t / 'usr/local/lib/mesa-asahi' / l, True)
    return {d: (Path(s).resolve(strict=True), x) for d, (s, x) in files.items()}


def quote(value):
    value = str(value)
    assert not any(c in value for c in '\n\r\x00"'), value
    return '"' + value + '"'


def write_root(image, files):
    """Add or replace regular files in the image's ext4 root with debugfs (no mount)."""
    offset, _ = partitions(image)[1]
    target = '%s?offset=%d' % (image, offset)
    existing_dirs, commands = set(), []

    def exists(path):
        r = subprocess.run(['debugfs', '-R', 'stat ' + quote(path), target], capture_output=True, text=True)
        return 'Type:' in r.stdout
    for dest in sorted(files):
        for parent in reversed(PurePosixPath(dest).parents):
            p = str(parent)
            if p != '/' and p not in existing_dirs:
                if not exists(p):
                    commands.append('mkdir ' + quote(p))
                existing_dirs.add(p)
    for dest, (source, executable) in sorted(files.items()):
        commands += ['rm ' + quote(dest), 'write %s %s' % (quote(source), quote(dest)),
                     'sif %s mode 0%o' % (quote(dest), 0o100000 | (0o755 if executable else 0o644))]
    r = subprocess.run(['debugfs', '-w', '-f', '-', target], input='\n'.join(commands) + '\n',
                       capture_output=True, text=True, check=True)
    bad = [l for l in (r.stdout + r.stderr).splitlines()
           if l.strip() and not l.startswith(('debugfs', 'Allocated inode')) and 'File not found by ext2_lookup' not in l]
    if bad:
        raise SystemExit('debugfs: ' + '\n'.join(bad[:10]))


def template(args):
    up = base_image(args.kernel)
    files = stable_payload(args.mesa)
    bootargs = BOOTARGS[args.kernel] + ' pdgopmode=' + args.resolution + (' -v' if args.verbose_boot else '') + (' ' + args.bootargs if args.bootargs else '')
    key = hashlib.sha256(json.dumps([str(up), bootargs, args.time_waits, TEMPLATE_VERSION,
                                     sorted((d, str(s), s.stat().st_size, s.stat().st_mtime_ns, x)
                                            for d, (s, x) in files.items())]).encode()).hexdigest()[:16]
    out = OUT / 'base' / ('template-%s-%s.img' % (args.kernel, key))
    if out.exists():
        return out
    (OUT / 'base').mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.tmp')
    tmp.unlink(missing_ok=True)
    sh(['cp', '--reflink=auto', up, tmp])
    os.chmod(tmp, 0o644)
    esp = '%s@@%d' % (tmp, partitions(tmp)[0][0])
    work = OUT / 'base' / ('esp-' + key)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir()
    sh([tool('mcopy'), '-n', '-i', esp, '::/EFI/BOOT/kernel', work / 'kernel'])
    kernel, patched = patch_time_waits((work / 'kernel').read_bytes(), *args.time_waits)
    if not patched:
        print('qd_image: IOKitInitializeTime wait signature not found; the boot takes 40 s longer', file=sys.stderr)
    (work / 'kernel').write_bytes(kernel)
    (work / 'boot-args.txt').write_text(bootargs)
    sh([tool('mcopy'), '-o', '-i', esp, work / 'kernel', '::/EFI/BOOT/kernel'])
    sh([tool('mcopy'), '-o', '-i', esp, work / 'boot-args.txt', '::/EFI/BOOT/boot-args.txt'])
    shutil.rmtree(work)
    write_root(tmp, files)
    offset, _ = partitions(tmp)[1]
    subprocess.run(['e2fsck', '-fn', '%s?offset=%d' % (tmp, offset)], check=True, capture_output=True)
    for old in (OUT / 'base').glob('template-%s-*.img' % args.kernel):
        old.unlink()
    tmp.rename(out)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--output', type=Path, required=True, help='run image to write (under runs/qemu-dev)')
    p.add_argument('--file', action='append', default=[], metavar='SOURCE=/DEST', help='file for the run layer (executable)')
    p.add_argument('--mesa', action='store_true', help='also stage the M1 Mesa libraries (unused in QEMU)')
    p.add_argument('--verbose-boot', action='store_true', help='-v: launchd and kernel log on the serial console')
    p.add_argument('--bootargs', default='', help='extra kernel boot arguments')
    p.add_argument('--resolution', default='1280x800', help='framebuffer size, one of the firmware\'s ramfb modes (default 1280x800, the M1 panel at 200%%)')
    p.add_argument('--time-waits', type=lambda s: tuple(int(v) for v in s.split(',')), default=(1, 2),
                   help='rtc,quiet seconds for the two IOKitInitializeTime waits (default 1,2; a RELEASE kernel uses the larger for both)')
    p.add_argument('--kernel', choices=('release', 'debug'), default='release', help='upstream kernel configuration (default release)')
    p.add_argument('--refresh', action='store_true', help='ask Nix again for the inputs (after changing the audio server, say)')
    args = p.parse_args()
    global REFRESH
    REFRESH = args.refresh
    out = args.output.resolve()
    assert out.is_relative_to(OUT.resolve()), 'run image must be under runs/qemu-dev'
    base = template(args)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.unlink(missing_ok=True)
    sh(['cp', '--reflink=auto', base, out])
    files = {}
    for item in args.file:
        source, dest = item.split('=', 1)
        files[dest] = (Path(source).resolve(strict=True), True)
    if files:
        write_root(out, files)
    print(json.dumps(dict(image=str(out), template=str(base), files={d: str(s) for d, (s, _) in files.items()})))


if __name__ == '__main__':
    main()
