"""Validate local alpha staging without installers, credentials or network."""

import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

spec = importlib.util.spec_from_file_location(
    "alpha_assets", Path(__file__).resolve().parents[1] / "scripts/prepare-alpha-assets.py"
)
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class AlphaAssetsTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.portable = self.root / "portable payload"
        self.portable.mkdir()
        for name in ("pxlcone.exe", "portable.txt", "manifest.txt", ".env", "русский файл"):
            (self.portable / name).write_bytes(b"fixture")
        self.setup = self.root / "setup.exe"
        self.setup.write_bytes(b"MZfixture")
        self.appimage = self.root / "launcher.AppImage"
        self.appimage.write_bytes(b"\x7fELFfixture")
        self.output = self.root / "release assets"

    def stage(self, version="0.1.0-alpha.1"):
        return assets.stage_assets(self.portable, self.setup, self.appimage, self.output, version)

    def test_three_assets_checksums_and_input_preservation(self):
        paths = self.stage()
        self.assertEqual(len(paths), 4)
        self.assertTrue(all(path.is_file() for path in paths))
        with zipfile.ZipFile(paths[0]) as archive:
            self.assertIn(".env", archive.namelist())
            self.assertIn("русский файл", archive.namelist())
            self.assertEqual(archive.read("pxlcone.exe"), b"fixture")
        self.assertEqual(paths[1].read_bytes(), self.setup.read_bytes())
        self.assertEqual(paths[2].read_bytes(), self.appimage.read_bytes())
        self.assertEqual((self.portable / "manifest.txt").read_bytes(), b"fixture")
        self.assertNotIn(b"\r", paths[3].read_bytes())
        for line in paths[3].read_text(encoding="utf-8").splitlines():
            digest, filename = line.split("  ", 1)
            self.assertEqual(digest, hashlib.sha256((self.output / filename).read_bytes()).hexdigest())

    def test_refuses_overwrite_and_payload_overlap(self):
        self.output.mkdir()
        marker = self.output / "keep"
        marker.write_text("untouched")
        with self.assertRaises(ValueError):
            self.stage()
        self.assertEqual(marker.read_text(), "untouched")
        self.output = self.portable / "release"
        with self.assertRaises(ValueError):
            self.stage()
        self.assertFalse(self.output.exists())

    def test_refuses_personal_data(self):
        (self.portable / "UserData").mkdir()
        with self.assertRaises(ValueError):
            self.stage()
        self.assertFalse(self.output.exists())

    def test_refuses_all_supported_config_fallbacks(self):
        for name in ("pxlcone.cfg", "elyprismlauncher.cfg", "prismlauncher.cfg", "polymc.cfg", "multimc.cfg"):
            with self.subTest(name=name):
                config = self.portable / name
                config.write_text("private settings", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Personal data"):
                    self.stage()
                self.assertFalse(self.output.exists())
                self.assertEqual(config.read_text(), "private settings")
                config.unlink()

    def test_destination_created_during_staging_is_preserved(self):
        copy = assets.shutil.copy2

        def concurrent_destination(source, destination):
            self.output.mkdir(exist_ok=True)
            (self.output / "keep").write_text("untouched")
            return copy(source, destination)

        with mock.patch.object(assets.shutil, "copy2", side_effect=concurrent_destination):
            with self.assertRaises(FileExistsError):
                self.stage()
        self.assertEqual((self.output / "keep").read_text(), "untouched")
        self.assertEqual(len(list(self.output.iterdir())), 1)

    def test_refuses_preview_payload(self):
        (self.portable / "PXL-Cone-Test-v35.exe").touch()
        with self.assertRaises(ValueError):
            self.stage()

    def test_requires_correct_inputs_and_alpha_version(self):
        with self.assertRaises(ValueError):
            self.stage("../../other")
        self.setup.write_bytes(b"not an executable")
        with self.assertRaises(ValueError):
            self.stage()
        self.setup.write_bytes(b"MZfixture")
        (self.portable / "portable.txt").unlink()
        with self.assertRaises(ValueError):
            self.stage()
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
