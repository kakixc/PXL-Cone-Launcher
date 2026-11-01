# Preparing alpha downloads

The first public alpha targets Windows x64 Portable, Windows x64 Setup and Linux x86_64 AppImage. These targets are not a statement that a public release or clean-system validation is complete.

Build each package with the same explicit PXL comparison version described in [release notifications](RELEASE_UPDATES.md). The example `0.1.0-alpha.1` below is a placeholder, not an announced release. Renaming a development preview does not change its embedded version.

The manual **Build** workflow accepts an optional `release-version` input, for example `0.1.0-alpha.1`, together with `build-type: Release`. This embeds the PXL comparison version and uses it in CI artifact labels. A nonempty version is rejected for Debug builds or if it does not use `x.y.z-alpha.N`. Leave it empty for an ordinary development build. The reusable workflow exposes the same input. Running Build produces CI artifacts; it does not publish a GitHub release.

After building and testing the packages, stage their files locally:

```text
python scripts/prepare-alpha-assets.py --version 0.1.0-alpha.1 --portable path/to/clean-portable-payload --setup path/to/PXL-Cone-Setup.exe --appimage path/to/PXL-Cone-Linux-x86_64.AppImage --output path/to/new-alpha-assets
```

The output parent must exist, and the output directory must be new and outside the portable payload. The script leaves its inputs unchanged and creates:

- `PXL-Cone-Windows-x64-Portable-0.1.0-alpha.1.zip`
- `PXL-Cone-Windows-x64-Setup-0.1.0-alpha.1.exe`
- `PXL-Cone-Linux-x86_64-0.1.0-alpha.1.AppImage`
- `SHA256SUMS`

The destination is reserved exclusively after staging. A copy failure can leave an incomplete new output directory; treat the command as failed and do not distribute those files. `SHA256SUMS` is copied last. An existing destination is never replaced, including one created while staging is running.

Use the clean installed portable payload from the build, containing `pxlcone.exe`, `portable.txt`, `manifest.txt`, Qt libraries and plugins. Older development previews used `elyprismlauncher.exe`; new clean builds default to `pxlcone`. Existing CMake caches can retain the old explicitly selected name, so check `Launcher_APP_BINARY_NAME=pxlcone` before release packaging. Do not use a working preview folder: it contains user accounts, instances and test executables. The script rejects known personal-data paths, preview executables and symbolic links in the Windows payload, but this is not a complete privacy or malware audit. Review every package before distribution.

Windows packaging also runs `scripts/check-windows-portable.ps1` to check the launcher, Java helpers, Qt platform plugin, license notices and absence of known personal-data paths. MSVC portable builds include the compiler's redistributable release runtime DLLs beside the executable; Setup retains its central redistributable installation. The portable runtime must be kept current with application updates. See Microsoft's [runtime deployment guidance](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files). These file checks do not replace launching the package on a clean Windows system.

New non-portable installations use a separate `PXLConeLauncher` data directory rather than the inherited ElyPrism directory. Existing portable `UserData` and `portable.txt` locations keep their precedence. An explicit `--dir` wins over environment overrides; `PXLCONE_DATA_DIR` wins over the legacy `ELYPRISMLAUNCHER_DATA_DIR`. If the selected data directory contains `elyprismlauncher.cfg` but no `pxlcone.cfg`, the existing settings loader copies the old settings to the new filename without deleting the old file. An existing `pxlcone.cfg` takes precedence. Data in an old preview's non-portable directory are not automatically moved or merged: use an explicit `--dir` to keep that selected directory, or the copy-only import flow for instances. Keep the old directory until its data have been checked in the new build. System file association ownership still requires separate validation before release.

The Windows installer offers PXL archive associations for `.pxlconep` and `.pxlt`, which open the existing import preview rather than installing immediately. Existing extension defaults are retained; `.zip` and `.mrpack` receive only an Open With entry. Browser links from CurseForge and Prism are an unchecked optional component, and an existing handler is preserved. Registry writes are per-user. Uninstallation checks that a command still points to the same PXL installation before removing its association. These source rules still need a disposable Windows installation/coexistence test, including Windows' Default Apps selection.

The script does not build an installer, run packages, sign files, contact GitHub or publish anything. The disabled `release.ymloff` is an inherited publication template and must not be enabled as a PXL release workflow without revision.

An artifact-enabled build also compiles the standalone updater. Validate this target separately from a local build with an empty `Launcher_BUILD_ARTIFACT`: the latter does not compile it. The standalone process uses the shared progress dialog without the main launcher's task-center integration. A successful build does not validate application replacement, rollback or installer execution.

Before the owner publishes an alpha, validate the Windows packages, including a data-preserving update. Check account sign-in, Java selection/download, game launch/stop, content installation/update, cancellation, import/export and small-screen behavior. Record known limitations in the release notes. Source builds and fixture tests do not replace those checks.

Linux AppImage may be offered as an **experimental, manually untested** alpha package. Repeat this limitation in the release notes; do not claim desktop Linux validation. Inspect the actual final AppImage contents before upload: exclude accounts and session files, user configuration, instances, worlds, screenshots, logs and local development/test data. Build it from a clean install payload. The staging script checks the AppImage header but does not inspect its contents, so staging success alone is not a privacy check. If no AppImage is available, this inspection remains pending.

Upload the reviewed files themselves as assets of the matching GitHub prerelease. The website recognises these filenames and links directly to the published assets. It cannot offer a private draft as a public download. After publication, check each website download and the launcher's update notification against that actual release.

Local staging regression tests:

```text
python tests/AlphaAssets_test.py
node --test website/tests/downloads.test.mjs
```
