# PXL Cone website

The player-facing website lives in `website/`. It is a static site: no account, backend, analytics or launcher UI runtime is required. English and Russian are included. Images are the owner-approved launcher screenshots already used by the README.

## Preview locally

From the repository root:

```sh
node --test website/tests/downloads.test.mjs
python scripts/build-site.py
python -m http.server 8765 --bind 127.0.0.1 --directory build-pxl-site
```

Open `http://127.0.0.1:8765`. The build copies an explicit list of public assets, including font licensing, into the ignored output directory. It does not copy private documents, launcher profiles or test executables. Keep the output directory dedicated to this build.

`website/index.html` also opens directly from disk: its images and fonts reference existing repository assets, and its scripts do not depend on module loading from a local-file origin. Building rewrites these paths to self-contained `assets/` paths for Pages. The output directory must contain only the named website files; unrelated files cause a build error rather than being published or deleted.

Optional browser verification uses Playwright as a development-only tool: after making the `playwright` package and its Chromium browser available, run `node website/tests/browser-check.cjs` from the repository root. `PXL_CHROME_PATH` can select an already installed Chrome executable. The check starts its own temporary loopback server, tests a repository subpath at desktop/tablet/mobile widths, verifies image loading, RU/EN and reduced motion, and uses intercepted release responses for direct-download, missing-file and retry states. It does not download executables or prove a real release has been published. Screenshots go to the ignored `build-pxl-site-qa/` directory.

## Direct downloads

The public GitHub releases list supplies actual `browser_download_url` values. Prereleases are included so the first alpha is discoverable. Drafts, Debug/ARM packages, unfinished uploads, other repositories and non-release links are excluded.

The site selects the newest published release containing supported PXL files. It does not combine a Windows file from one version with a Linux file from another. Missing formats remain unavailable until the matching package is uploaded. When no files exist, the site shows a preparing state; request failures show a retry action. A download button never silently links to a release landing page.

Use these release-file conventions for the initial x64 packages:

- Windows installer: `PXL-Cone-Windows-x64-Setup-<version>.exe`.
- Windows Portable: `PXL-Cone-Windows-x64-Portable-<version>.zip`.
- Linux: `PXL-Cone-Linux-x86_64-<version>.AppImage`.

Windows visitors default to Setup, Linux visitors to AppImage. Android, iOS, macOS, ChromeOS and unknown systems get a manual desktop-platform choice; the site does not pretend to provide a native build for those systems. Browser identification can be imperfect, so platform buttons always remain visible. Setup/Portable/AppImage installation must be tested independently before publishing release files.

## GitHub Pages

The public site is deployed at https://kakixc.github.io/PXL-Cone-Launcher/. The first alpha deployment passed its download-selection and artifact-boundary tests on GitHub Actions.

Configure the repository Pages source as **GitHub Actions**, then manually run **PXL Cone website** when publication is approved. The workflow publishes only `build-pxl-site`, never the repository root. Adding this workflow does not itself publish a site. A real Pages deployment and real release-file downloads require separate verification.

The website must keep its player-oriented copy accurate. Update the first-alpha notice when the product leaves that stage; do not advertise roadmap features as available.
