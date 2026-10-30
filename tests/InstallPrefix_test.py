"""Exercise the real logging/deployment install rules with a relocated prefix."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CMAKE = os.environ.get("PXL_TEST_CMAKE") or shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake unavailable")
class InstallPrefixTest(unittest.TestCase):
    def check_relocated_install(self, mingw):
        source = (ROOT / "launcher/CMakeLists.txt").read_text(encoding="utf-8")
        logging = re.search(r'install\(\s*FILES "qtlogging.ini"\s*DESTINATION [^\n]+\s*COMPONENT bundle\s*\)', source)
        self.assertIsNotNone(logging, "Real logging install rule not found")
        injection = re.search(r'install\(CODE "\s*set\(QT_PLUGINS_DIR.*?COMPONENT bundle\)', source, re.S)
        self.assertIsNotNone(injection, "Real MinGW deployment variables not found")
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            project = directory / "source"
            project.mkdir()
            (project / "qtlogging.ini").write_text("[Rules]\nfixture=true\n")
            (project / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.24)\nproject(PrefixCheck NONE)\n'
                'set(RESOURCES_DEST_DIR ".")\n'
                + (injection.group(0) + "\n" if mingw else "")
                + logging.group(0) + "\n", encoding="utf-8")
            build = directory / "build"
            configured = directory / "configured-prefix"
            relocated = directory / "relocated package"
            result = subprocess.run([CMAKE, "-S", str(project), "-B", str(build),
                                     f"-DCMAKE_INSTALL_PREFIX={configured.as_posix()}"], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            # Forbid absolute destinations before any files can be installed.
            result = subprocess.run([CMAKE, f"-DCMAKE_INSTALL_PREFIX={relocated.as_posix()}",
                                     "-DCMAKE_INSTALL_COMPONENT=bundle",
                                     "-DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON",
                                     "-P", str(build / "cmake_install.cmake")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(configured.exists(), "Install escaped the relocated package")
            self.assertEqual((relocated / "qtlogging.ini").read_text(), "[Rules]\nfixture=true\n")

    def test_logging_follows_install_time_prefix(self):
        self.check_relocated_install(False)

    def test_mingw_deployment_keeps_install_time_prefix(self):
        self.check_relocated_install(True)


if __name__ == "__main__":
    unittest.main()
