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
    def test_versioned_graphics_cohort_keeps_storage_dispatch_and_broker_together(self):
        graphics = self.root / 'overrides/nv12-v1'
        self.manifest.update(system_ui=True, graphics_override=str(graphics),
                             surface_override=str(self.root / 'overrides/surface-old/Frameworks'))
        command, env = self.plan()
        self.assertEqual(command[1], str(graphics / 'surface/bin/iosurfaced'))
        self.assertEqual(env['DYLD_FRAMEWORK_PATH'].split(':')[0], str(graphics / 'surface/Frameworks'))
        self.assertIn(str(graphics / 'gl/Frameworks'), env['DYLD_FRAMEWORK_PATH'].split(':'))
        self.assertIn(str(graphics / 'gl/lib'), env['DYLD_LIBRARY_PATH'].split(':'))
        for path in [str(self.root / 'gl/lib'), str(self.root / 'gl/Frameworks'),
                     str(self.root / 'overrides/surface-old/Frameworks')]:
            self.assertNotIn(path, env['DYLD_FRAMEWORK_PATH'].split(':') + env['DYLD_LIBRARY_PATH'].split(':'))

    def test_graphics_cohort_refuses_mixed_system_or_foreign_prefix(self):
        self.manifest.update(system_ui=True)
        for value in ['/System/Library', str(self.root / 'overrides'),
                      str(self.root / 'overrides/nv12/../other')]:
            self.manifest['graphics_override'] = value
            with self.assertRaises(ValueError):
                self.plan()
        self.manifest.update(graphics_override=str(self.root / 'overrides/nv12-v1'), system_gpu_providers=True)
        with self.assertRaises(ValueError):
            self.plan()

    def test_each_observer_in_a_combined_diagnostic_is_validated(self):
        os.environ['SFFX_DIAGNOSTIC_OBSERVER'] = '/private/var/db/sffx/hal.dylib:/private/var/db/sffx/loss.dylib'
        _, env = self.plan(['--profile', '/private/var/db/sffx/control/profile'])
        self.assertTrue(env['DYLD_INSERT_LIBRARIES'].endswith(os.environ['SFFX_DIAGNOSTIC_OBSERVER']))
        for suffix in ['/tmp/foreign.dylib', '/private/var/db/sffx/../foreign.dylib', '']:
            os.environ['SFFX_DIAGNOSTIC_OBSERVER'] = '/private/var/db/sffx/hal.dylib:' + suffix
            with self.assertRaises(ValueError):
                self.plan(['--profile', '/private/var/db/sffx/control/profile'])

    def test_system_gpu_selection_removes_private_surface_and_gl_and_uses_system_broker(self):
        self.manifest.update(system_ui=True, system_gpu_providers=True,
                             surface_override=str(self.root / 'overrides/surface-old/Frameworks'))
        command, env = self.plan()
        self.assertEqual(command[1], '/usr/libexec/iosurfaced')
        self.assertIn(str(self.root / 'surface/bin/surface-session'), command)
        for field in ['DYLD_FRAMEWORK_PATH', 'DYLD_LIBRARY_PATH']:
            for path in env[field].split(':'):
                self.assertFalse(any(part in Path(path).parts for part in ['surface', 'gl', 'overrides']))
        self.assertIn('/System/Library/Frameworks', env['DYLD_FRAMEWORK_PATH'].split(':'))

    def test_system_gpu_selection_requires_the_system_ui_cohort(self):
        self.manifest['system_gpu_providers'] = True
        with self.assertRaises(ValueError):
            self.plan()

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

    def test_system_ui_cannot_reselect_legacy_private_overrides(self):
        self.manifest.update(system_ui=True,
            ui_override=str(self.root / 'overrides/ui-c17-v1'),
            coretext_override=str(self.root / 'overrides/memory-font-v1/Frameworks'),
            onyx_override=str(self.root / 'overrides/glyph-generation-v1/Frameworks'))
        _, env = self.plan()
        paths = env['DYLD_FRAMEWORK_PATH'].split(':') + env['DYLD_LIBRARY_PATH'].split(':')
        for path in paths:
            self.assertFalse(any(part in Path(path).parts for part in
                ['appkit', 'quartz', 'onyx', 'coretext', 'windowserver', 'overrides']))
        self.assertIn('/System/Library/Frameworks', paths)
        self.assertIn(str(self.root / 'surface/Frameworks'), paths)
        self.assertIn(str(self.root / 'gl/Frameworks'), paths)
        self.assertEqual(env['FONTCONFIG_FILE'], str(self.root / 'fonts/fonts.conf'))
        self.assertIn(str(self.root / 'sandbox/libsffx-sandbox.dylib'), env['DYLD_INSERT_LIBRARIES'])

    def test_system_ui_requires_boolean_and_installed_layout(self):
        for value in ['system', 1, None]:
            self.manifest['system_ui'] = value
            with self.assertRaises(ValueError):
                self.plan()
        self.manifest.update(system_ui=True, system_layout=False)
        with self.assertRaises(ValueError):
            self.plan()

    def test_system_ui_keeps_the_qualified_iosurface_lifetime_provider(self):
        provider = self.root / 'overrides/surface-lookup-v1/Frameworks'
        self.manifest.update(system_ui=True,surface_override=str(provider))
        _, env = self.plan()
        self.assertEqual(env['DYLD_FRAMEWORK_PATH'].split(':')[0],str(provider))
        self.assertNotIn(str(self.root / 'appkit/Frameworks'),env['DYLD_FRAMEWORK_PATH'].split(':'))


if __name__ == '__main__':
    unittest.main()
