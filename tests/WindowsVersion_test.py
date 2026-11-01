"""Configure the real EXE resource template without compiling the launcher."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get("PXL_TEST_CMAKE") or shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake unavailable")
class WindowsVersionTest(unittest.TestCase):
    def configure(self, version, template="pxlcone.rc.in"):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            output = directory / "pxlcone.rc"
            script = directory / "check.cmake"
            script.write_text(
                f'include("{ROOT.as_posix()}/cmake/PxlWindowsVersion.cmake")\n'
                'pxl_windows_version("${TEST_VERSION}")\n'
                'set(Launcher_DisplayName "PXL Cone Launcher")\n'
                'set(Launcher_CommonName "ElyPrismLauncher")\n'
                'set(Launcher_InstallID "PXLConeLauncher")\n'
                f'configure_file("{ROOT.as_posix()}/program_info/{template}" '
                f'"{output.as_posix()}" @ONLY)\n', encoding="utf-8")
            result = subprocess.run([CMAKE, f"-DTEST_VERSION={version}", "-P", str(script)],
                                    capture_output=True, text=True)
            return result, output.read_text() if output.exists() else ""

    def test_alpha_version_in_actual_resource(self):
        result, resource = self.configure("0.1.0-alpha.3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("FILEVERSION     0,1,0,0", resource)
        self.assertIn("PRODUCTVERSION  0,1,0,0", resource)
        self.assertIn('VALUE "FileVersion", "0.1.0-alpha.3"', resource)
        self.assertIn('VALUE "ProductVersion", "0.1.0-alpha.3"', resource)
        self.assertNotIn("12.0.0", resource)

    def test_development_does_not_report_upstream_version(self):
        result, resource = self.configure("0.0.0-dev")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('VALUE "FileVersion", "0.0.0-dev"', resource)
        self.assertIn("FILEVERSION     0,0,0,0", resource)

    def test_tag_prefix_and_build_metadata(self):
        result, resource = self.configure("v1.2.3-beta.2+build.7")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("FILEVERSION     1,2,3,0", resource)
        self.assertIn('VALUE "ProductVersion", "1.2.3-beta.2+build.7"', resource)

    def test_stable_version(self):
        result, resource = self.configure("1.2.3")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('VALUE "FileVersion", "1.2.3"', resource)

    def test_invalid_or_unrepresentable_version_is_rejected(self):
        for version in ("preview", '1.2.3"bad', "65536.1.0", "0.65536.0", "0.1.65536", "01.2.3", "1.02.3", "1.2.03"):
            with self.subTest(version=version):
                result, resource = self.configure(version)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("PXL Windows", result.stderr)
                self.assertEqual(resource, "")

    def test_installer_version_and_installed_apps_display(self):
        result, installer = self.configure("0.1.0-alpha.3", "win_install.nsi.in")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('VIProductVersion "0.1.0.0"', installer)
        self.assertIn('VIFileVersion "0.1.0.0"', installer)
        self.assertIn('"ProductVersion" "0.1.0-alpha.3"', installer)
        self.assertIn('"DisplayVersion" "0.1.0-alpha.3"', installer)
        self.assertIn('"VersionMajor" "0"', installer)
        self.assertIn('"VersionMinor" "1"', installer)
        self.assertNotIn("12.0.0", installer)

    def test_installer_does_not_select_upstream_uninstall_key(self):
        result, installer = self.configure("0.1.0-alpha.3", "win_install.nsi.in")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('InstallDir "$LOCALAPPDATA\\Programs\\PXLConeLauncher"', installer)
        self.assertIn('!define UNINST_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PXLConeLauncher"', installer)
        self.assertIn('InstallDirRegKey HKCU "Software\\PXLConeLauncher" "InstallDir"', installer)
        self.assertIn('/PxlShellAssociations.nsh"', installer)
        self.assertNotIn("ElyPrismLauncher", installer)


if __name__ == "__main__":
    unittest.main()
