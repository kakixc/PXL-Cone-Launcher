"""Check CI configure arguments using a stub cmake; no project build is run."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/configure-pxl-build.sh"
BASH = os.environ.get("PXL_TEST_BASH") or shutil.which("bash")


@unittest.skipUnless(BASH, "Bash unavailable")
class BuildConfigurationTest(unittest.TestCase):
    def configure(self, version, build_type="Release", preset="fixture preset"):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            cmake = directory / "cmake"
            with cmake.open("w", encoding="utf-8", newline="\n") as stream:
                stream.write('#!/usr/bin/env bash\nprintf "%s\\n" "$@" > "$PXL_TEST_CAPTURE"\n')
            cmake.chmod(0o755)
            captured = directory / "arguments"
            environment = os.environ.copy()
            environment.update(CMAKE_PRESET=preset, PXL_RELEASE_VERSION=version, BUILD_TYPE=build_type,
                               PXL_TEST_CAPTURE=captured.as_posix())
            # A Windows drive-letter PATH is not a POSIX PATH segment. Pass the
            # fixture directory separately and let the selected Bash convert it.
            environment["PXL_TEST_COMMANDS"] = directory.as_posix()
            command = 'export PATH="$PXL_TEST_COMMANDS:$PATH"; bash "$1"'
            if os.name == "nt":
                command = 'export PATH="$(cygpath -u "$PXL_TEST_COMMANDS"):$PATH"; bash "$1"'
            result = subprocess.run([BASH, "-c", command, "fixture", SCRIPT.as_posix()],
                                    env=environment, capture_output=True, text=True)
            arguments = captured.read_text().splitlines() if captured.exists() else []
            return result, arguments

    def test_development_build_keeps_existing_configuration(self):
        result, arguments = self.configure("", "Debug")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(arguments, ["--preset", "fixture preset", "-DBUILD_TESTING=ON"])

    def test_alpha_is_embedded_as_one_cmake_argument(self):
        result, arguments = self.configure("0.1.0-alpha.1")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(arguments[-1], "-DLauncher_RELEASE_VERSION=0.1.0-alpha.1")

    def test_invalid_version_never_invokes_cmake(self):
        for version in ("v0.1.0-alpha.1", "01.1.0-alpha.1", "0.1.0-alpha.01", "0.1.0;echo bad", "$(echo bad)"):
            with self.subTest(version=version):
                result, arguments = self.configure(version)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(arguments, [])
                self.assertIn("PXL release version must use", result.stderr)

    def test_alpha_requires_release(self):
        result, arguments = self.configure("0.1.0-alpha.1", "Debug")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(arguments, [])
        self.assertIn("requires a Release build", result.stderr)

    def test_missing_preset_never_invokes_cmake(self):
        result, arguments = self.configure("", preset="")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(arguments, [])
        self.assertIn("CMake preset is required", result.stderr)


if __name__ == "__main__":
    unittest.main()
