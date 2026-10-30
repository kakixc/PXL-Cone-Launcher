"""Linux package manifest regression tests; no dependencies beyond Python."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
import shutil
import subprocess
import tarfile

spec = importlib.util.spec_from_file_location(
    "package_manifest", Path(__file__).resolve().parents[1] / "scripts/write-package-manifest.py"
)
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class PackageManifestTest(unittest.TestCase):
    def test_paths_hidden_files_and_repeat(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "sub folder").mkdir()
            for name in (".env", "launcher", "sub folder/русский файл", "sub folder/plugin.so"):
                (root / name).write_text("fixture", encoding="utf-8")
            manifest.write_manifest(root)
            data = (root / "manifest.txt").read_bytes()
            self.assertFalse(data.startswith(b"\xef\xbb\xbf"))
            self.assertEqual(set(data.decode("utf-8").splitlines()), {
                ".env", "launcher", "manifest.txt", "sub folder/русский файл", "sub folder/plugin.so"
            })
            manifest.write_manifest(root)
            self.assertEqual((root / "manifest.txt").read_bytes(), data)

    def test_symlinks_are_listed_without_following_directories(self):
        with tempfile.TemporaryDirectory() as temporary, tempfile.TemporaryDirectory() as outside:
            root = Path(temporary)
            (Path(outside) / "external").write_text("fixture")
            try:
                (root / "linked directory").symlink_to(outside, target_is_directory=True)
                (root / "broken link").symlink_to(root / "missing")
            except OSError:
                self.skipTest("Symlink creation unavailable on this system")
            manifest.write_manifest(root)
            self.assertEqual(set((root / "manifest.txt").read_text().splitlines()), {
                "linked directory", "broken link", "manifest.txt"
            })

    def test_invalid_root(self):
        with tempfile.TemporaryDirectory() as temporary:
            file = Path(temporary) / "file"
            file.touch()
            with self.assertRaises(ValueError):
                manifest.write_manifest(file)
            with self.assertRaises(FileNotFoundError):
                manifest.write_manifest(Path(temporary) / "missing")

    @unittest.skipUnless(shutil.which("tar"), "tar unavailable")
    def test_archive_includes_hidden_root_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "package"
            root.mkdir()
            (root / ".env").write_text("fixture")
            (root / "launcher").write_text("fixture")
            manifest.write_manifest(root)
            archive = Path(temporary) / "package.tar.gz"
            subprocess.run(["tar", "-czf", str(archive), "-C", str(root), "."], check=True)
            with tarfile.open(archive) as packaged:
                self.assertIn("./.env", packaged.getnames())
                self.assertIn("./manifest.txt", packaged.getnames())

    def test_manifest_symlink_cannot_overwrite_another_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "package"
            root.mkdir()
            target = Path(temporary) / "other"
            target.write_text("untouched")
            try:
                (root / "manifest.txt").symlink_to(target)
            except OSError:
                self.skipTest("Symlink creation unavailable on this system")
            with self.assertRaises(ValueError):
                manifest.write_manifest(root)
            self.assertEqual(target.read_text(), "untouched")


if __name__ == "__main__":
    unittest.main()
