# SPDX-License-Identifier: MIT
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('coord_gate', Path(__file__).with_name('coord-gate.py'))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class HoldTests(unittest.TestCase):
    def test_quoted_marker_is_not_completion(self):
        text = "T VK M1 back in SiliconForest\nU VK Please stay off the M1 until 'VK kernel test done'.\n"
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNotNone(gate.pending_hold(text+"V VK Waiting for 'VK kernel test done'.\n"))
        self.assertIsNone(gate.pending_hold(text+"V VK kernel test done: returned to daily\n"))

    def test_second_hold_requires_new_completion(self):
        text = "T VK Please stay off the M1 until 'VK M1 back in SiliconForest'.\nU VK M1 back in SiliconForest\n"
        self.assertIsNone(gate.pending_hold(text))
        self.assertIsNotNone(gate.pending_hold(text+"V VK Stay off SSH until 'VK M1 back in SiliconForest'.\n"))

    def test_alternative_completions(self):
        text = "T VK Please stay off the M1 until 'VK teardown session done' or 'VK M1 back in SiliconForest'.\n"
        for marker in ['teardown session done', 'M1 back in SiliconForest']:
            self.assertIsNone(gate.pending_hold(text+'U VK '+marker+' (daily)\n'))

    def test_explicit_codex_hold(self):
        text = 'T CODEX received coordinator M1 SSH hold; do not connect\n'
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNone(gate.pending_hold(text+'U COCOA4 M1 back in SiliconForest\n'))

    def test_core_install_blocks_new_programs(self):
        text = "T COCOA4 core.lock: GUARDED INSTALL now; done when I log 'COCOA4 core install finished'\n"
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNotNone(gate.pending_hold(text+"U CODEX waiting for COCOA4 core install finished\n"))
        self.assertIsNone(gate.pending_hold(text+'U COCOA4 core install finished: verified\n'))

    def test_reboot_announcement_and_actual_return(self):
        text = 'T GFX2 in ~2 min: M1 REBOOT for kernel cycle\n'
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNone(gate.pending_hold(text+'U GFX2 M1 back in SiliconForest after kernel cycle\n'))

    def test_gpu_run_requires_its_owner_completion(self):
        text = 'T GFX2 risky GPU test over SSH: avd-run\n'
        self.assertIsNotNone(gate.pending_hold(text+'U VK risky GPU test avd-run finished: rc=0\n'))
        self.assertIsNone(gate.pending_hold(text+'U GFX2 risky GPU test avd-run finished: rc=0\n'))

    def test_overlapping_holds_are_independent(self):
        text = 'T PERF core.lock: GUARDED INSTALL now\nU GFX2 in ~2 min: M1 REBOOT\n'
        self.assertIsNotNone(gate.pending_hold(text+'V GFX2 M1 back in SiliconForest\n'))
        self.assertIsNone(gate.pending_hold(text+'V GFX2 M1 back in SiliconForest\nW PERF core install finished: checked\n'))

    def test_qemu_rehearsal_is_not_live_install(self):
        text = 'T PERF core.lock: QEMU rehearsal; the guarded install follows later and will be announced\n'
        self.assertIsNone(gate.pending_hold(text))

    def test_cancelled_reboot_and_historical_mentions(self):
        text = "T COCOA4 Stay off SSH until 'M1 back in SiliconForest'\nU COCOA4 CANCELLED the visit: not run, no reboot\n"
        self.assertIsNone(gate.pending_hold(text))
        self.assertIsNone(gate.pending_hold(text+'V TERM5 correction: I missed the guarded-install notice\n'))

    def test_future_guarded_install_announcement_blocks(self):
        text = 'T PERF in 2 minutes: guarded core install on the M1\n'
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNone(gate.pending_hold(text+'U PERF core install finished: 18 checks\n'))

    def test_gpu_session_announcement_survives_individual_run(self):
        text = 'T GFX2 in ~2 min: risky GPU tests with UART capture\n'
        text += 'U GFX2 risky GPU test over SSH: first\nV GFX2 risky GPU test first finished: rc=0\n'
        self.assertIsNotNone(gate.pending_hold(text))
        self.assertIsNone(gate.pending_hold(text+'W GFX2 gui.lock released: all tests done\n'))

    def test_owner_withdrawn_session_needs_new_announcement(self):
        text = 'T GFX2 in 2 min: risky GPU tests\nU GFX2 my announced GPU tests did not run; re-announced when they run\n'
        self.assertIsNone(gate.pending_hold(text))
        self.assertIsNotNone(gate.pending_hold(text+'V GFX2 in 2 min: risky GPU tests\n'))

    def test_recovered_boot_ends_interrupted_gpu_session(self):
        text = 'T GFX2 in 2 min: risky GPU tests\nU GFX2 risky GPU test over SSH: interrupted\n'
        self.assertIsNone(gate.pending_hold(text+'V GFX2 M1 back in SiliconForest (other agents may use SSH again)\n'))

if __name__ == '__main__':
    unittest.main()
