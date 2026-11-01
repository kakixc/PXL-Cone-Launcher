#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('linux_bundle', Path(__file__).parents[1] / 'scripts/normalize-linux-bundle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


@unittest.skipUnless(os.name == 'posix', 'Linux bundle symlinks require POSIX')
class LinuxBundleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'appdir'
        self.prefix = '/opt/Qt/6.8.3/gcc_64'
        self.lib = self.root / 'shared/lib'
        self.qt = self.lib / self.prefix.lstrip('/')
        for directory in ('bin', 'shared/bin', 'share/ElyPrismLauncher'):
            (self.root / directory).mkdir(parents=True)
        (self.qt / 'lib').mkdir(parents=True)
        (self.qt / 'plugins/platforms').mkdir(parents=True)
        (self.root / 'bin/pxlcone').write_text('launcher')
        (self.root / 'share/ElyPrismLauncher/JavaCheck.jar').write_text('helper')
        (self.lib / 'libQt6Core.so.6.4.2').write_text('system Qt')
        (self.qt / 'lib/libQt6Core.so.6.8.3').write_text('SDK Qt')
        (self.qt / 'lib/libQt6Core.so.6').symlink_to('libQt6Core.so.6.8.3')
        (self.qt / 'plugins/platforms/libqoffscreen.so').write_text('SDK plugin')

    def test_selects_sdk_without_local_prefix_and_preserves_assets(self):
        backup = self.base / 'backup'
        module.normalize_bundle(self.root, self.prefix, backup)
        self.assertEqual((self.lib / 'libQt6Core.so.6').read_text(), 'SDK Qt')
        self.assertFalse((self.lib / 'libQt6Core.so.6.4.2').exists())
        self.assertEqual((backup / 'libQt6Core.so.6.4.2').read_text(), 'system Qt')
        self.assertEqual((self.lib / 'qt/plugins/platforms/libqoffscreen.so').read_text(), 'SDK plugin')
        self.assertFalse((self.lib / 'opt').exists())
        self.assertNotIn('/opt/Qt', (self.lib / 'lib.path').read_text())
        self.assertEqual((self.root / 'shared/share/ElyPrismLauncher/JavaCheck.jar').read_text(), 'helper')
        self.assertIn('Prefix = ../lib/qt', (self.root / 'shared/bin/qt.conf').read_text())

    def test_backup_inside_payload_is_rejected_before_mutation(self):
        with self.assertRaisesRegex(ValueError, 'outside'):
            module.normalize_bundle(self.root, self.prefix, self.root / 'backup')
        self.assertEqual((self.lib / 'libQt6Core.so.6.4.2').read_text(), 'system Qt')

    def test_plugin_links_survive_sdk_relocation(self):
        directory = self.qt / 'plugins/platforms'
        (directory / 'relative.so').symlink_to('../../lib/libQt6Core.so.6.8.3')
        (directory / 'absolute.so').symlink_to(self.qt / 'lib/libQt6Core.so.6.8.3')
        module.normalize_bundle(self.root, self.prefix, self.base / 'backup')
        for name in ('relative.so', 'absolute.so'):
            plugin = self.lib / 'qt/plugins/platforms' / name
            self.assertFalse(plugin.is_symlink())
            self.assertEqual(plugin.read_text(), 'SDK Qt')

    def test_external_sdk_symlink_is_rejected(self):
        (self.qt / 'plugins/platforms/external.so').symlink_to(self.base / 'outside')
        (self.base / 'outside').write_text('not part of the bundle')
        with self.assertRaisesRegex(ValueError, 'external symbolic link'):
            module.normalize_bundle(self.root, self.prefix, self.base / 'backup')
        self.assertFalse((self.base / 'backup').exists())

    def test_existing_backup_is_preserved(self):
        backup = self.base / 'backup'
        backup.mkdir()
        (backup / 'keep').write_text('keep')
        with self.assertRaisesRegex(ValueError, 'Preserve existing'):
            module.normalize_bundle(self.root, self.prefix, backup)
        self.assertEqual((backup / 'keep').read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
