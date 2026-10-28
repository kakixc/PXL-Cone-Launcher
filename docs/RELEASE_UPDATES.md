# Launcher release notifications

PXL Cone checks the public GitHub releases of `kakixc/PXL-Cone-Launcher` asynchronously. When a newer release is detected, the header shows **Update available** and opens its details. Manual checks are available under **Settings → About**. The panel renders the release body as Markdown and offers a link to that release's downloads. It does not replace the running executable or install a package automatically. Linux package-manager installations should be updated using their normal package workflow.

Automatic checks default to daily, with the first check ten seconds after startup. They can be disabled in the updates panel. A detected release is cached across restarts and revalidated against the installed version and repository. Background failures do not open modal alerts; opening the panel shows the error and allows retrying. No GitHub token is required. Drafts, invalid version tags and older/equal versions are ignored. Stable builds exclude prereleases; builds whose comparison version includes a prerelease suffix include them. The latest 100 published release records are considered.

## Release build configuration

The comparison version must match the version of PXL Cone being packaged, independently of the inherited Pinecone development display version. Use a numeric three-part tag, optionally with a prerelease suffix, for example `v0.1.0-alpha.1`. A leading `v` is accepted. Before distributing an alpha with separate PXL numbering, explicitly set the comparison version:

    cmake -S . -B build -DLauncher_RELEASE_VERSION=0.1.0-alpha.1 -DLauncher_RELEASE_REPOSITORY=kakixc/PXL-Cone-Launcher

Without an override, an exact Git checkout of a numeric release tag uses that tag automatically. Other unreleased previews compare from `0.0.0-dev` and include prereleases, so the inherited Pinecone 12.x display version cannot hide the first PXL alpha. The panel labels this baseline Development preview. Release archives without Git information must override it with their actual published tag, otherwise the same release will keep being offered. An existing CMake cache may retain an earlier explicitly configured comparison version; verify the configuration before packaging. GitHub's release body is the changelog. No public release is created by configuring or running a development build.

Release notes support headings, emphasis, lists, code and links through Qt's GitHub Markdown dialect. Raw HTML is disabled. Images and local resources are not loaded; HTTP/HTTPS links open only after a click. Download navigation is constructed from the configured repository and validated tag rather than a release-supplied external URL.

## Verification

Build and run the `LauncherUpdates` test target with `QT_QPA_PLATFORM=offscreen`. Its controlled network replies cover version/channel selection, coalescing concurrent requests, failures, persistence, notification state and Markdown. These tests do not substitute for checking a real published release and its downloadable packages before shipping.
