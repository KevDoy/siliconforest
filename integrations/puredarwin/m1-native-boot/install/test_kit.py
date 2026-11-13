#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Host test of the install kit (run build.sh first): the installer's OWN code (the copy in
out/mac-kit/installer, Asahi installer e93743d) is run on this workstation against our
installer data and package, with macOS's tools replaced by recorders. It shows what the
installer will ask diskutil and newfs_msdos for, what it copies onto the boot partition and
what it writes as the stub's boot object. Nothing is partitioned, mounted or formatted here.

    python3 -I -B test_kit.py [out directory]
"""
import hashlib, json, os, pathlib, subprocess, sys, tempfile, types, unittest

HERE = pathlib.Path(__file__).resolve().parent
OUT = pathlib.Path(sys.argv.pop(1)) if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else HERE / 'out'
KIT = OUT / 'mac-kit' / 'installer'
sys.path.insert(0, str(KIT))
sys.path.insert(0, str(HERE.parent / 'b1'))
os.chdir(KIT)
os.environ.update(DISTRO='SiliconForest', REPO_BASE=str(KIT))
import util, m1n1, osinstall, diskutil      # the installer's modules, as shipped in the kit
import pdsf_pack

FREE_BLOCKS, STUB = 741417, util.align_down(util.psize('2.5GB'), util.psize('1MiB'))
PARTUUID = '0FC63DAF-0000-4000-8000-000053464254'


class Recorder:
    """Stands in for the installer's DiskUtil: records the calls, hands back made-up partitions."""
    def __init__(self, tmp):
        self.calls, self.tmp = [], tmp

    def addPartition(self, after, fs, label, size):
        self.calls.append(('addPartition', after, fs, label, size))
        return diskutil.Partition(name='disk0s99', offset=0, size=size, free=False, type='EFI', uuid=PARTUUID)

    def mount(self, name):
        self.calls.append(('mount', name))
        path = os.path.join(self.tmp, 'mnt-' + name); os.makedirs(path, exist_ok=True)
        return path


class Kit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.load(open('installer_data.json'))
        cls.template = cls.data['os_list'][0]
        cls.tmp = tempfile.mkdtemp()
        cls.dutil = Recorder(cls.tmp)
        cls.ran = []
        cls.real_run = subprocess.run
        subprocess.run = lambda args, **kw: cls.ran.append(args) or types.SimpleNamespace(stdout=b'', returncode=0)
        cls.osins = osinstall.OSInstaller(cls.dutil, cls.data, cls.template)
        cls.osins.load_package()
        cls.stub = types.SimpleNamespace(icon_path=os.path.join(cls.tmp, 'VolumeIcon.icns'),
                                         boot_obj_path=os.path.join(cls.tmp, 'boot.bin'))
        cls.osins.partition_disk('disk0s98', None)
        cls.osins.install(cls.stub)
        subprocess.run = cls.real_run

    def test_one_entry_named_siliconforest(self):
        self.assertEqual(len(self.data['os_list']), 1)
        self.assertEqual(self.osins.name, 'SiliconForest')
        self.assertFalse(self.osins.expandable)            # no question about sizes
        self.assertFalse(self.osins.needs_firmware)        # no firmware collection for Linux
        self.assertEqual(self.osins.idata_targets, [])     # nothing of the installer's data on our partition
        self.assertNotIn('extras', self.template)

    def test_fits_the_free_space(self):
        free = FREE_BLOCKS * 4096
        need = STUB + self.osins.min_recommended_size
        self.assertEqual(STUB, 2499805184)
        self.assertEqual(self.osins.min_size, 500 << 20)
        # main.py action_install_into_free: offered when align_down(free, 1 MiB) >= STUB + min_recommended_size
        self.assertGreaterEqual(util.align_down(free, 1 << 20), need)
        # and with room for an alignment of either partition's start to 1 MiB
        self.assertGreaterEqual(free - need, 2 << 20)

    def test_what_diskutil_is_asked(self):
        # one partition after the stub: type EFI, not formatted by diskutil, 500 MiB
        self.assertEqual([c for c in self.dutil.calls if c[0] == 'addPartition'],
                         [('addPartition', 'disk0s98', '%EFI%', '%noformat%', 500 << 20)])
        self.assertEqual(self.ran, [['newfs_msdos', '-F', '32', '-v', 'SFBOOT - Si', '/dev/rdisk0s99']])

    def test_boot_partition_content(self):
        mnt = pathlib.Path(self.tmp) / 'mnt-disk0s99'
        files = sorted(str(p.relative_to(mnt)) for p in mnt.rglob('*') if p.is_file())
        self.assertEqual(files, ['sf/boot.bin'])
        data = (mnt / 'sf/boot.bin').read_bytes()
        self.assertEqual(data, (OUT / 'boot.bin').read_bytes())
        found, end = pdsf_pack.parse(data, pdsf_pack.records_offset(data))
        self.assertEqual([i[0] for i in found], [pdsf_pack.CONFIG, pdsf_pack.KERNEL])
        self.assertTrue(all(i[4] for i in found))
        self.assertLessEqual(len(data), pdsf_pack.ESP_IMAGE_LIMIT)

    def test_boot_object_is_stage1_with_the_chainload_line(self):
        stage1 = (OUT / 'sf-stage1.bin').read_bytes()
        obj = open(self.stub.boot_obj_path, 'rb').read()
        tail = ('chosen.asahi,efi-system-partition=%s\nchainload=%s;sf/boot.bin\n' % ((PARTUUID.lower(),) * 2)).encode() + bytes(4)
        self.assertEqual(obj, stage1 + tail)
        # (the installer's extract_vars, used only by its "upgrade m1n1" and "rebuild firmware" actions, which
        # are not on our menu, does not find the lines: this m1n1 has its stack marker more than once)
        self.assertGreater(obj.count(b'STACKBOT'), 1)
        self.assertTrue(m1n1.get_version(self.stub.boot_obj_path).endswith('-siliconforest-stage1-v1'))
        # the bench's test image is the same construction with another identifier
        test = (OUT / 'sf-stage1-test.bin').read_bytes()
        self.assertEqual(test[:len(stage1)], stage1)
        self.assertEqual(len(test), len(obj))

    def test_icon(self):
        icon = open(self.stub.icon_path, 'rb').read()
        self.assertEqual(icon[:4], b'icns')
        self.assertEqual(int.from_bytes(icon[4:8], 'big'), len(icon))
        self.assertEqual(icon, (KIT / 'logo.icns').read_bytes())

    def test_stages(self):
        s1, s2 = (OUT / 'sf-stage1.bin').read_bytes(), (OUT / 'sf-stage2.bin').read_bytes()
        for image in (s1, s2):
            self.assertEqual(image[0x808:0x810], b'AURALOAD')
            self.assertIn(b'SF_STOPPED', image)                 # the installed variant: stops, no proxy, no restart
        self.assertNotIn(b'PDSF: ', s1)                          # stage 1 has no kernel loader
        self.assertIn(b'PDSF: ', s2)
        # only the configuration record differs between boot.bin and the bench's image
        a, b = (OUT / 'boot.bin').read_bytes(), (OUT / 'boot-bench.bin').read_bytes()
        at = pdsf_pack.records_offset(a)
        fa, fb = pdsf_pack.parse(a, at)[0], pdsf_pack.parse(b, at)[0]
        self.assertEqual(a[:at], b[:at]); self.assertEqual(a[:at], s2)
        ka, kb = a[fa[1][5]:fa[1][5] + fa[1][2]], b[fb[1][5]:fb[1][5] + fb[1][2]]
        self.assertEqual(hashlib.sha256(ka).digest(), hashlib.sha256(kb).digest())
        ta, tb = a[fa[0][5]:fa[0][5] + fa[0][2]].decode(), b[fb[0][5]:fb[0][5] + fb[0][2]].decode()
        self.assertEqual(tb, ta.replace(' pdapciebridge=1\n', ' pdapciebridge=1 sfsession_seconds=150 sfmark=1\n'))
        args = ta.splitlines()[0].split()
        self.assertIn('pdnvme_ttl=0', args); self.assertIn('rd=uuid', args); self.assertIn('pdnvme_write=1', args)
        for word in args:
            self.assertFalse(word.startswith(('sfbench', 'sfsession_seconds', 'sfmark', 'pdsfstop', 'pdpoweroff', 'pdpmu')), word)

    def test_menu_is_short(self):
        source = (KIT / 'main.py').read_text()
        self.assertIn('actions = {k: v for k, v in actions.items() if k in ("p", "f")}', source)

    def test_manifest(self):
        for line in (OUT / 'mac-kit' / 'MANIFEST.sha256').read_text().splitlines():
            digest, name = line.split('  ', 1)
            self.assertEqual(hashlib.sha256((OUT / 'mac-kit' / name).read_bytes()).hexdigest(), digest, name)


if __name__ == '__main__':
    unittest.main()
