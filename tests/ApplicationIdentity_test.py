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
MAKENSIS = os.environ.get("PXL_TEST_MAKENSIS") or shutil.which("makensis")
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

    def test_windows_registration_preserves_existing_defaults(self):
        installer = (self.build / "program_info/win_install.nsi").read_text(encoding="utf-8")
        helpers = (self.build / "program_info/PxlShellAssociations.nsh").read_text(encoding="utf-8")
        self.assertIn('!define PXL_PROGID "PXLConeLauncher.App"', helpers)
        self.assertIn('!define PXL_EXE "pxlcone.exe"', helpers)
        self.assertIn('!define PXL_IMPORT_COMMAND \'"$INSTDIR\\${PXL_EXE}" -I "%1"\'', helpers)
        for extension in (".pxlconep", ".pxlt"):
            self.assertIn(f'!insertmacro PXL_REGISTER_EXTENSION "{extension}" true', installer)
        for extension in (".zip", ".mrpack"):
            self.assertIn(f'!insertmacro PXL_REGISTER_EXTENSION "{extension}" false', installer)
        optional = installer.split('Section /o "Browser links from CurseForge and Prism"', 1)[1].split("SectionEnd", 1)[0]
        for scheme in ("curseforge", "prismlauncher"):
            self.assertIn(f'!insertmacro PXL_REGISTER_PROTOCOL_IF_FREE "{scheme}"', optional)
            self.assertEqual(installer.count(f'!insertmacro PXL_REGISTER_PROTOCOL_IF_FREE "{scheme}"'), 1)
        self.assertIn('ReadRegStr $R0 HKCR "${EXT}" ""\n    ${If} $R0 == ""', helpers)
        self.assertIn('ReadRegStr $R0 HKCR "${SCHEME}\\shell\\open\\command" ""', helpers)
        self.assertNotIn("APP_TEARDOWN", installer)
        self.assertNotIn("UserChoice", helpers)
        for line in helpers.splitlines():
            if line.lstrip().startswith(("WriteReg", "DeleteReg")):
                self.assertIn(" HKCU ", line)

    def test_windows_uninstall_checks_each_command_owner(self):
        installer = (self.build / "program_info/win_install.nsi").read_text(encoding="utf-8")
        helpers = (self.build / "program_info/PxlShellAssociations.nsh").read_text(encoding="utf-8")
        uninstall = installer.split('Section -un.ShellAssoc', 1)[1].split("SectionEnd", 1)[0]
        self.assertLess(uninstall.index('PXL_REMOVE_EXTENSION_IF_OWNED'), uninstall.index('PXL_REMOVE_APP_IF_OWNED'))
        for macro in ("PXL_REMOVE_EXTENSION_IF_OWNED", "PXL_REMOVE_PROTOCOL_IF_OWNED", "PXL_REMOVE_APP_IF_OWNED"):
            body = helpers.split(f"!macro {macro}", 1)[1].split("!macroend", 1)[0]
            self.assertIn("ReadRegStr $R0 HKCU", body)
            self.assertIn("${If} $R0 == '${PXL_IMPORT_COMMAND}'", body)
            self.assertLess(body.index("${If} $R0 == '${PXL_IMPORT_COMMAND}'"), body.index("DeleteReg"))

    @unittest.skipUnless(MAKENSIS, "NSIS unavailable; generated templates are checked separately")
    def test_nsis_preprocesses_real_association_macros_without_an_installer(self):
        fixture = self.build / "association-syntax.nsi"
        output = self.build / "must-not-be-generated.exe"
        header = self.build / "program_info/PxlShellAssociations.nsh"
        fixture.write_text(
            '!include "LogicLib.nsh"\n'
            f'!include "{header.as_posix()}"\n'
            'Name "PXL association syntax fixture"\n'
            f'OutFile "{output.as_posix()}"\n'
            'Section\n'
            '  !insertmacro PXL_REGISTER_APP\n'
            '  !insertmacro PXL_REGISTER_EXTENSION ".pxlconep" true\n'
            '  !insertmacro PXL_REGISTER_EXTENSION ".mrpack" false\n'
            '  !insertmacro PXL_REGISTER_PROTOCOL_IF_FREE "pxlcone"\n'
            'SectionEnd\n'
            'Section "Uninstall"\n'
            '  !insertmacro PXL_REMOVE_EXTENSION_IF_OWNED ".pxlconep"\n'
            '  !insertmacro PXL_REMOVE_PROTOCOL_IF_OWNED "pxlcone"\n'
            '  !insertmacro PXL_REMOVE_APP_IF_OWNED\n'
            'SectionEnd\n', encoding="utf-8")
        result = subprocess.run([MAKENSIS, "-SAFEPPO", str(fixture)], cwd=self.build,
                                capture_output=True, text=True, errors="replace", timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(output.exists(), "Preprocessing must not produce an installer")


if __name__ == "__main__":
    unittest.main()
