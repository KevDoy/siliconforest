# SPDX-License-Identifier: MIT
"""Host policy checks for safe installed-runtime launch plans, not native GUI proof."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('daily_launch', Path(__file__).with_name('daily-launch.py'))
daily = importlib.util.module_from_spec(spec)
spec.loader.exec_module(daily)


class LaunchPlanPolicy(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.manifest = dict(target=str(self.root), system_layout=True, private_fonts=True, gpu_presentation=True)
        self.environment = patch.dict(os.environ, {}, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def plan(self, args=None):
        (self.root / 'release.json').write_text(json.dumps(self.manifest))
        return daily.plan(self.root, args or [])

    def test_ssh_environment_cannot_select_an_absent_linux_display(self):
        os.environ.update(XDG_RUNTIME_DIR='/var/run/user/0', WAYLAND_DISPLAY='wayland-stale')
        _, env = self.plan()
        self.assertEqual(env['XDG_RUNTIME_DIR'], '/private/var/run/gpu-desktop')
        self.assertEqual(env['WAYLAND_DISPLAY'], 'wayland-0')
        self.assertEqual(os.environ['WAYLAND_DISPLAY'], 'wayland-stale')

    def test_coherent_ui_selection_and_system_coretext(self):
        ui = self.root / 'overrides/ui-c17-v1'
        self.manifest.update(ui_override=str(ui), coretext_source='system')
        _, env = self.plan()
        self.assertIn(str(ui / 'appkit/Frameworks'), env['DYLD_FRAMEWORK_PATH'].split(':'))
        self.assertEqual(env['DYLD_LIBRARY_PATH'].split(':')[0], str(ui / 'windowserver/usr/lib'))
        self.assertNotIn(str(self.root / 'coretext/Frameworks'), env['DYLD_FRAMEWORK_PATH'].split(':'))
        self.assertEqual(env['FONTCONFIG_FILE'], str(self.root / 'fonts/fonts.conf'))

    def test_ui_selection_cannot_escape_the_owned_runtime(self):
        for ui in ['/System/Library/Frameworks', str(self.root / 'overrides/../appkit'), str(self.root / 'overrides')]:
            self.manifest['ui_override'] = ui
            with self.assertRaises(ValueError):
                self.plan()

    def test_onyx_selection_replaces_only_the_private_glyph_provider(self):
        provider = self.root / 'overrides/glyph-generation-v1/Frameworks'
        self.manifest['onyx_override'] = str(provider)
        _, env = self.plan()
        frames = env['DYLD_FRAMEWORK_PATH'].split(':')
        self.assertIn(str(provider), frames)
        self.assertNotIn(str(self.root / 'onyx/Frameworks'), frames)
        self.assertIn(str(self.root / 'appkit/Frameworks'), frames)
        self.assertIn(str(self.root / 'coretext/Frameworks'), frames)

    def test_onyx_selection_cannot_escape_the_owned_runtime(self):
        for value in ['/System/Library/Frameworks', str(self.root / 'overrides/../onyx/Frameworks'),
                      str(self.root / 'overrides'), str(self.root / 'overrides/glyph-generation-v1')]:
            self.manifest['onyx_override'] = value
            with self.assertRaises(ValueError):
                self.plan()

    def test_observer_requires_an_explicit_private_profile(self):
        os.environ['SFFX_DIAGNOSTIC_OBSERVER'] = '/private/var/db/sffx/audio-test/observer.dylib'
        for args in [[], ['--profile', '/var/root/Library/Application Support/Firefox/Profiles/siliconforest']]:
            with self.assertRaises(ValueError):
                self.plan(args)
        _, env = self.plan(['--profile', '/private/var/db/sffx/audio-test/profile'])
        self.assertTrue(env['DYLD_INSERT_LIBRARIES'].endswith(':' + os.environ['SFFX_DIAGNOSTIC_OBSERVER']))

    def test_disabled_sandboxes_are_still_refused(self):
        os.environ['MOZ_DISABLE_GPU_SANDBOX'] = '1'
        with self.assertRaises(RuntimeError):
            self.plan()


if __name__ == '__main__':
    unittest.main()
