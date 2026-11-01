"""Configure real packaging templates in isolation; never install/register them."""
import configparser
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get("PXL_TEST_CMAKE") or shutil.which("cmake")
APP_ID = "io.github.kakixc.PXLConeLauncher"


@unittest.skipUnless(CMAKE, "CMake unavailable")
class ApplicationIdentityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        directory = Path(cls.temporary.name)
        cls.build = directory / "build"
        (directory / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.22)\n'
            'project(IdentityFixture LANGUAGES NONE)\n'
            'set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)\n'
            f'list(APPEND CMAKE_MODULE_PATH "{ROOT.as_posix()}/cmake")\n'
            'set(Launcher_APP_BINARY_NAME "pxlcone")\n'
            'set(Launcher_RELEASE_VERSION "0.1.0-alpha.7")\n'
            'set(Launcher_VERSION_NAME "12.0.0-develop")\n'
            'set(Launcher_VERSION_NAME4 "12.0.0.0")\n'
            'set(Launcher_BUILD_TIMESTAMP "2026-11-01")\n'
            f'add_subdirectory("{ROOT.as_posix()}/program_info" program_info)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/identity.txt" '
            '"${Launcher_Name}\n${Launcher_RuntimeName}\n${Launcher_ConfigFile}\n${Launcher_AppID}\n")\n',
            encoding="utf-8")
        result = subprocess.run([CMAKE, "-S", str(directory), "-B", str(cls.build)],
                                capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def test_internal_target_and_pxl_product_names(self):
        self.assertEqual((self.build / "identity.txt").read_text().splitlines(),
                         ["ElyPrismLauncher", "PXLConeLauncher", "pxlcone.cfg", APP_ID])

    def test_desktop_and_appstream_use_same_pxl_identity(self):
        desktop = configparser.ConfigParser(interpolation=None)
        desktop.read(self.build / "program_info" / f"{APP_ID}.desktop", encoding="utf-8")
        entry = desktop["Desktop Entry"]
        self.assertEqual(entry["Name"], "PXL Cone Launcher")
        self.assertEqual(entry["Icon"], APP_ID)
        self.assertEqual(entry["StartupWMClass"], "PXLConeLauncher")
        self.assertEqual(entry["Exec"], "pxlcone %U")
        metadata = ET.parse(self.build / "program_info" / f"{APP_ID}.metainfo.xml").getroot()
        self.assertEqual(metadata.findtext("id"), APP_ID)
        self.assertEqual(metadata.findtext("launchable"), f"{APP_ID}.desktop")
        self.assertEqual(metadata.find("releases/release").get("version"), "0.1.0-alpha.7")
        self.assertTrue((self.build / "program_info" / f"{APP_ID}.xml").is_file())
        self.assertFalse((self.build / "program_info/io.github.elyprismlauncher.ElyPrismLauncher.desktop").exists())

    def test_appimage_links_follow_generated_identity(self):
        action = (ROOT / ".github/actions/package/linux/action.yml").read_text(encoding="utf-8")
        self.assertIn(f'ln -s {APP_ID}.metainfo.xml "$INSTALL_APPIMAGE_DIR"/share/metainfo/{APP_ID}.appdata.xml', action)
        self.assertIn(f'ln -s share/applications/{APP_ID}.desktop "$INSTALL_APPIMAGE_DIR"', action)
        self.assertIn(f'ln -s share/icons/hicolor/256x256/apps/{APP_ID}.png "$INSTALL_APPIMAGE_DIR"', action)
        self.assertNotIn("io.github.elyprismlauncher.ElyPrismLauncher", action)

    def test_native_formats_have_pxl_icons_and_desktop_handler(self):
        namespace = {"mime": "http://www.freedesktop.org/standards/shared-mime-info"}
        metadata = ET.parse(self.build / "program_info" / f"{APP_ID}.xml").getroot()
        desktop = configparser.ConfigParser(interpolation=None)
        desktop.read(self.build / "program_info" / f"{APP_ID}.desktop", encoding="utf-8")
        handlers = desktop["Desktop Entry"]["MimeType"].split(";")
        for mime_type, extension in (("application/vnd.pxl-cone.pack+zip", "*.pxlconep"),
                                     ("application/vnd.pxl-cone.transfer+zip", "*.pxlt")):
            entry = metadata.find(f'mime:mime-type[@type="{mime_type}"]', namespace)
            self.assertIsNotNone(entry)
            self.assertEqual(entry.find("mime:glob", namespace).get("pattern"), extension)
            self.assertEqual(entry.find("mime:icon", namespace).get("name"), APP_ID)
            self.assertEqual(entry.find("mime:sub-class-of", namespace).get("type"), "application/zip")
            self.assertIn(mime_type, handlers)
        self.assertEqual(desktop["Desktop Entry"]["Exec"], "pxlcone %U")


if __name__ == "__main__":
    unittest.main()
