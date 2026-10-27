<p align="center">
  <b>English</b> · <a href="README_RU.md">Русский</a>
</p>

<p align="center">
  <img src="program_info/PXLConeLauncher.icon/assets/logo/pxlcone-logo-full.svg" alt="PXL Cone Launcher" width="430">
</p>

<p align="center"><b>A native Minecraft launcher based on PineconeMC / Prism Launcher.</b></p>

> [!IMPORTANT]
> PXL Cone is in active development. The interface and transfer features are available in development code and previews; they are still being refined. This is not a stable release announcement.

## What works in the current development version

- Home and Library with three saved layouts: cover tiles, compact PXL hybrid cards and a detailed list. Search, groups, sorting, playtime and custom static instance covers stay in the main window.
- Instance creation from Minecraft/loader versions, existing modpack catalogs and supported import routes, using the upstream metadata and installation tasks.
- Play/Stop actions and a task panel showing active operations and their recent results.
- An in-instance content browser for mods, resource packs and shaders, using existing provider queries and dependency review. Vanilla opens resource packs; mods require a loader and shaders show their renderer prerequisite. Data packs currently link to world management.
- Cone appearance with Inter, English/Russian UI, top/side navigation, optional wallpaper and a custom-image choice.
- Settings inside the launcher with search, explicit Apply/leave validation and a saved basic/advanced mode. Technical services, tools and proxy options remain available in advanced mode; some technical screens are still being redesigned.
- PXL views for installed content, game components, worlds and live logs, sharing the existing models and operations. A compact account picker and screenshot gallery provide quick access to profiles and captured images.
- Copy import from Prism/Pinecone instance folders and PXL archive import/export.

The launcher retains the upstream instance, Microsoft/Ely.by account, loader, Modrinth/CurseForge and Java-management infrastructure. Keeping a feature in the code does not mean every combination has been tested in the redesigned interface. Platform packaging and end-to-end compatibility are still being validated.

## Sharing and moving instances

`.pxlconep` stores a pack/workspace transfer; `.pxlt` moves selected personal instances and supported portable preferences. Transfers can include selected content, worlds and screenshots. Smart archives can restore known files using their recorded exact metadata and hashes; offline archives include selected files.

These initial formats do **not** implement all of the server links, migration history and collaboration concepts in the roadmap. Account sessions, credentials and user launch commands are excluded from portable preferences.

See [the archive format and current limits](docs/PXL_ARCHIVE_FORMAT.md). Existing `.mrpack`, CurseForge and other upstream import/export routes remain available where supported. A dedicated loss-aware `.mrpack` ↔ `.pxlconep` converter is not implemented.

## Current limits

- The in-instance content catalog is being redesigned. Some content/provider tools still use upstream layouts or dialogs.
- OptiFine installation is currently documented as a manual workflow; automatic official-site download and standalone installation are not implemented.
- Task history is kept for the current session: active tasks are retained and completed history is capped.
- English and Russian are the exposed product languages. Upstream translation percentages do not measure coverage of the new PXL interface.
- Settings migration, dialog consistency, keyboard behavior and accessibility are still receiving fixes.

## Roadmap — not implemented features

Preparation for the first public release includes a short, skippable setup and feature introduction, real screenshots in both READMEs, and a PXL-style website on GitHub Pages from this repository. These are not implemented yet.

Other development directions have no fixed release date:

- Further content-management design work, including installed-project actions and world-specific data-pack browsing.
- Compatible quick-start presets with a preview of the changes; no universal performance or RAM claims.
- Smart Minecraft/loader migration, snapshots, change history and rollback.
- Linked local-server workflows and pack collaboration.
- Optional AI-assisted pack drafts using a user-configured provider, with approval of the exact content before installation.
- Per-version artwork and further banner customisation.

## Technology and platforms

The launcher uses **C++ and Qt** and reuses the PineconeMC / Prism architecture. It does not use Electron or an embedded web frontend as its UI foundation.

Windows and Linux are the primary development targets. Building on one platform does not establish compatibility on the other. Installer/package formats and a public release date are not promised here.

Build configuration lives in the root [CMakeLists.txt](CMakeLists.txt); platform workflows are in [.github/workflows](.github/workflows). Core dependencies and build options follow the upstream project, with PXL resources and UI components added alongside them.

## Attribution and license

PXL Cone is a fork of PineconeMC, which builds on Prism Launcher, PolyMC and MultiMC. Upstream attribution and notices remain in the source and About/license information. See [COPYING.md](COPYING.md) and the component-specific licenses. Bundled fonts and icons retain their own license notices.

PXL Cone Launcher is an independent community project. It is not an official product of, or endorsed by, Mojang Studios, Microsoft, Prism Launcher, PineconeMC, Modrinth, CurseForge or Ely.by. Minecraft is a trademark of Microsoft / Mojang Studios.
