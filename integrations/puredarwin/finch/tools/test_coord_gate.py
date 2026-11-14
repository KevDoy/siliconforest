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

if __name__ == '__main__':
    unittest.main()
