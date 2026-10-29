"""Public artifact boundaries and relative asset paths; Python standard library only."""
import importlib.util
from html.parser import HTMLParser
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("build_site", ROOT / "scripts/build-site.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class Links(HTMLParser):
    def __init__(self):
        super().__init__()
        self.local = []

    def handle_starttag(self, tag, attributes):
        for key, value in attributes:
            if key in ("href", "src") and value and not value.startswith(("https:", "#")):
                self.local.append(value)


class SiteBuildTests(unittest.TestCase):
    def test_generated_artifact_has_only_allowed_public_files_and_valid_links(self):
        with tempfile.TemporaryDirectory(dir=ROOT, prefix="build-site-test-") as folder:
            output = Path(folder)
            builder.build(output)
            files = {item.relative_to(output).as_posix() for item in output.rglob("*") if item.is_file()}
            self.assertEqual(files, set(builder.FILES) | {".nojekyll"})
            parser = Links()
            parser.feed((output / "index.html").read_text(encoding="utf-8"))
            for link in parser.local:
                self.assertTrue((output / link).is_file(), link)
                self.assertNotIn("..", Path(link).parts)

    def test_source_html_assets_exist_before_build(self):
        parser = Links()
        parser.feed((ROOT / "website/index.html").read_text(encoding="utf-8"))
        for link in parser.local:
            self.assertTrue((ROOT / "website" / link).is_file(), link)

    def test_unrelated_output_files_are_not_published_or_deleted(self):
        with tempfile.TemporaryDirectory(dir=ROOT, prefix="build-site-test-") as folder:
            output = Path(folder)
            unrelated = output / "private.txt"
            unrelated.write_text("do not copy or delete", encoding="utf-8")
            with self.assertRaises(ValueError):
                builder.build(output)
            self.assertEqual(unrelated.read_text(encoding="utf-8"), "do not copy or delete")

    def test_repository_root_is_not_a_build_destination(self):
        with self.assertRaises(ValueError):
            builder.build(ROOT)

    def test_unexpected_empty_directory_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT, prefix="build-site-test-") as folder:
            output = Path(folder)
            (output / "unrelated-directory").mkdir()
            with self.assertRaises(ValueError):
                builder.build(output)
            self.assertTrue((output / "unrelated-directory").is_dir())

    def test_unexpected_directory_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT, prefix="build-site-test-") as folder, \
                tempfile.TemporaryDirectory(dir=ROOT, prefix="build-site-test-") as target:
            output = Path(folder)
            link = output / "private-link"
            try:
                link.symlink_to(target, target_is_directory=True)
            except OSError as error:
                if getattr(error, "winerror", None) == 1314:
                    self.skipTest("This Windows account cannot create symlinks")
                raise
            with self.assertRaises(ValueError):
                builder.build(output)
            self.assertTrue(link.is_symlink())


if __name__ == "__main__":
    unittest.main()
