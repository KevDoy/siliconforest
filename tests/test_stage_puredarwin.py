"""Exercise real git-apply in tiny disposable fixture repositories, NOT PureDarwin."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from tools import stage_puredarwin as stage
class StageTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)/'checkout';self.root.mkdir()
        self.git('init','-q')
        for path,data in ((stage.ANCHOR,'# synthetic parent\n'),(stage.HEADER,'/* synthetic header */\n')):
            p=self.root/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data)
        self.git('add','.');self.git('-c','user.name=Lab fixture','-c','user.email=fixture@example.invalid','commit','-qm','fixture')
        self.pin=patch.object(stage,'PIN',self.git('rev-parse','HEAD').strip());self.pin.start()
        self.header=patch.object(stage,'HEADER_BLOB',stage.blob((self.root/stage.HEADER).read_bytes()));self.header.start()
    def tearDown(self):self.header.stop();self.pin.stop();self.tmp.cleanup()
    def git(self,*args):
        r=subprocess.run(['git','-C',str(self.root),*args],capture_output=True,text=True,timeout=30)
        if r.returncode:raise RuntimeError(r.stderr)
        return r.stdout
    def test_dry_run_no_writes(self):
        report,patch_text=stage.prepare(self.root)
        self.assertFalse(report['executed']);self.assertFalse(report['kernel_built'])
        self.assertIn('DLMemoryProbe.cpp',patch_text)
        self.assertEqual(self.git('status','--porcelain'),'');self.assertFalse((self.root/stage.DEST).exists())
    def test_real_fixture_application(self):
        report=stage.execute(self.root,True);self.assertTrue(report['executed'])
        for path,data in stage.files().items():self.assertEqual((self.root/path).read_bytes(),data)
        self.assertIn(stage.MARKER,(self.root/stage.ANCHOR).read_text())
        self.assertEqual(self.git('rev-parse','HEAD').strip(),stage.PIN)
        self.assertFalse(report['kernel_built'])
    def test_missing_ack(self):
        with self.assertRaises(stage.StageError):stage.execute(self.root,False)
        self.assertEqual(self.git('status','--porcelain'),'')
    def test_wrong_pin(self):
        with patch.object(stage,'PIN','0'*40),self.assertRaises(stage.StageError):stage.prepare(self.root)
    def test_wrong_header(self):
        with patch.object(stage,'HEADER_BLOB','0'*40),self.assertRaises(stage.StageError):stage.prepare(self.root)
    def test_dirty_tracked_file(self):
        (self.root/stage.ANCHOR).write_text('changed\n')
        with self.assertRaises(stage.StageError):stage.prepare(self.root)
    def test_untracked_file(self):
        (self.root/'untracked').write_text('x')
        with self.assertRaises(stage.StageError):stage.prepare(self.root)
    def test_nested_path_rejected(self):
        with self.assertRaises(stage.StageError):stage.prepare(self.root/'src')
    def test_repeat_apply_rejected(self):
        stage.execute(self.root,True)
        with self.assertRaises(stage.StageError):stage.execute(self.root,True)
    def test_symlink_root(self):
        alias=Path(self.tmp.name)/'alias';alias.symlink_to(self.root,target_is_directory=True)
        with self.assertRaises(stage.StageError):stage.prepare(alias)
    def test_source_hashes(self):
        r,_=stage.prepare(self.root);self.assertEqual(len(r['files']),15)
        self.assertTrue(all(len(x)==64 for x in r['files'].values()))
if __name__=='__main__':unittest.main()
