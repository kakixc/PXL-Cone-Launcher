# Preparing alpha downloads

The first public alpha targets Windows x64 Portable, Windows x64 Setup and Linux x86_64 AppImage. These targets are not a statement that a public release or clean-system validation is complete.

Build each package with the same explicit PXL comparison version described in [release notifications](RELEASE_UPDATES.md). The example `0.1.0-alpha.1` below is a placeholder, not an announced release. Renaming a development preview does not change its embedded version.

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

Use the clean installed portable payload from the build, containing `elyprismlauncher.exe`, `portable.txt`, `manifest.txt`, Qt libraries and plugins. The internal executable name is retained for compatibility. Do not use a working preview folder: it contains user accounts, instances and test executables. The script rejects known personal-data paths, preview executables and symbolic links in the Windows payload, but this is not a complete privacy or malware audit. Review every package before distribution.

The script does not build an installer, run packages, sign files, contact GitHub or publish anything. The disabled `release.ymloff` is an inherited publication template and must not be enabled as a PXL release workflow without revision.

Before the owner publishes an alpha, verify all three packages on their target systems, including a data-preserving update. Check account sign-in, Java selection/download, game launch/stop, content installation/update, cancellation, import/export and small-screen behavior. Record known limitations in the release notes. Source builds and fixture tests do not replace those checks.

Upload the reviewed files themselves as assets of the matching GitHub prerelease. The website recognises these filenames and links directly to the published assets. It cannot offer a private draft as a public download. After publication, check each website download and the launcher's update notification against that actual release.

Local staging regression tests:

```text
python tests/AlphaAssets_test.py
node --test website/tests/downloads.test.mjs
```
