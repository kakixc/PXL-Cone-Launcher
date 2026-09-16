# PXL Cone Launcher

> **A fast, user-friendly Minecraft launcher based on PineconeMC.**  
> Modern UI. Powerful instance management. Easy sharing. Easy migration.

> [!IMPORTANT]
> **PXL Cone is currently in the planning stage.**
> There is no public release yet. This repository is being prepared before active development begins.

---

## What is PXL Cone?

PXL Cone is a planned open-source Minecraft launcher based on **PineconeMC** and, through it, the Prism Launcher ecosystem.

The goal is simple:

> Keep the power and performance of PineconeMC/Prism, but make everyday use much easier.

PXL Cone is planned as a native **C++ / Qt** application for Windows and Linux, without replacing the launcher core with Electron or a browser-based UI.

It is intended for both normal players and advanced users: simple by default, powerful when needed.

---

## Why another Minecraft launcher?

Prism Launcher and PineconeMC are extremely powerful, but their interface and some workflows can feel technical or inconvenient to new users.

PXL Cone aims to improve the parts users interact with every day without throwing away the mature launcher core underneath.

The project is not intended to be just a reskin.

The biggest planned differences are:

- a modern, cleaner Library and instance interface;
- easier importing from other launchers;
- cross-platform migration between Windows and Linux;
- one-file pack sharing with `.pxlconep`;
- a dedicated transfer/backup format with `.pxlt`;
- a normal offline ZIP export that is useful outside the Prism/Pinecone ecosystem;
- better backup, update and error-handling workflows;
- native performance and low idle overhead;
- all advanced controls remain available;
- the cats stay.

---

## `.pxlconep` — send a pack as one file

One of the main planned features is **PXL Cone Pack**.

Example:

```text
Create-Survival.pxlconep
```

Send it through Telegram, Discord, email or any other messenger.

The receiver opens the file in PXL Cone, checks what will be installed, and clicks **Install**.

Two modes are planned:

### Smart Pack

A small package containing exact Minecraft, loader and content metadata.

PXL Cone downloads the exact required versions during installation.

### Offline Pack

A larger package containing the selected mods, resource packs, shaders, configs and other files directly.

Useful when the receiver has no internet access or when a fully self-contained copy is needed.

---

## `.pxlt` — move your Minecraft setup

PXL Transfer is planned for moving your own Minecraft data between computers and operating systems.

Examples:

```text
Windows -> Fedora
Fedora -> Windows
PC -> laptop
old PC -> new PC
```

A transfer may include selected:

- instances;
- worlds;
- configs;
- options;
- custom icons;
- local content;
- screenshots;
- launcher organization/settings.

Thin transfers can reference downloadable content instead of copying every mod.

Offline transfers can bundle everything needed for restoration.

---

## Universal Offline ZIP

PXL Cone is also planned to export a **normal Minecraft-content ZIP**.

Example:

```text
My-Pack-Offline.zip
├── mods/
├── config/
├── resourcepacks/
├── shaderpacks/
├── saves/
├── options.txt
└── README.txt
```

This is intentionally different from a launcher-specific Prism/Pinecone instance archive.

The archive should remain understandable and useful even if the recipient does not use PXL Cone.

Standard exports such as Modrinth `.mrpack` and supported CurseForge formats are also planned to remain available.

---

## Modern UI without losing advanced features

The UI direction is inspired by modern launchers such as the Modrinth App, but PXL Cone will have its own identity.

Planned main areas:

```text
Library
Discover
Downloads / Tasks
Accounts
Settings
Instance Details
```

Common actions should be easy to find.

Technical controls should still exist under advanced settings instead of being removed.

---

## PineconeMC foundation

PXL Cone is planned as a **PineconeMC fork**, rather than a new launcher engine written from scratch.

That means the project can build on existing work such as:

- multiple isolated Minecraft instances;
- Microsoft account support;
- Ely.by support inherited from PineconeMC;
- Fabric / Forge / NeoForge / Quilt support where available upstream;
- Modrinth and CurseForge integration;
- Java/runtime handling;
- instance import/export;
- mature Minecraft launch logic.

The intention is to keep authentication and core launch behavior close to upstream whenever possible.

---

## Windows + Linux

PXL Cone is planned around a single cross-platform C++/Qt codebase.

### Windows

Planned downloads:

```text
Windows x64 Installer
Windows x64 Portable ZIP
```

### Linux

Planned downloads:

```text
RPM                 Fedora / RPM-based distributions
DEB                 Ubuntu / Debian-based distributions
pkg.tar.zst         Arch Linux
Flatpak
AppImage
```

Linux is intended to be a first-class platform, not a later port.

macOS may be considered after the Windows/Linux version is stable.

---

## Import and migration

The planned Migration Center should make it easy to detect and import existing Minecraft installations.

Initial targets:

```text
PineconeMC
Prism Launcher
Modrinth App
CurseForge
MultiMC-compatible instances
Vanilla .minecraft
```

The goal is to avoid manually copying random folders and rebuilding every instance after changing launcher or operating system.

---

## Planned later features

After the core launcher is stable, possible additions include:

- snapshots and rollback;
- automatic world backup before risky updates;
- duplicate-content detection;
- storage analyzer;
- improved network diagnostics;
- rule-based Crash Doctor;
- optional AI-assisted crash explanations;
- optional cloud backup/sync services.

AI features will not be required to use the launcher.

---

## Performance philosophy

PXL Cone should stay lightweight.

The current plan is to keep the native Qt/C++ architecture and avoid turning the launcher into an Electron application.

Main principles:

```text
Fast startup
Low idle CPU usage
No always-running embedded Chromium
Asynchronous downloads
Cached artwork
No unnecessary network work at startup
No UI freezes during long operations
```

Performance claims will be benchmarked before being used in marketing.

---

## Open source

PXL Cone is planned to remain open source in accordance with the licenses of its upstream projects.

The launcher client will not require a PXL account for basic use.

Optional online services may be added later, but the core launcher should remain usable without them.

---

## Project status

```text
Planning / specification     ██████████  Active
Initial fork                 ░░░░░░░░░░  Not started
UI prototype                 ░░░░░░░░░░  Not started
Migration / sharing          ░░░░░░░░░░  Not started
Public alpha                 ░░░░░░░░░░  Not released
```

Current priority order for PXL projects:

```text
1. PixelNet
2. Pixel City
3. PXL Cone
```

PXL Cone development will begin when the higher-priority projects reach the intended milestones.

---

## Follow development

If you are interested in the project:

- **Star** the repository to bookmark it;
- **Watch** the repository for development activity;
- follow project announcements in **GitHub Discussions** when enabled;
- follow the PXL / Pixel Service channels for demos and development updates.

A public release date has not been announced yet.

---

## Contributing

Contribution guidelines will be added when active development begins.

Ideas and UX feedback will be especially useful once the first prototype exists.

Please avoid opening bug reports for features that do not exist yet.

---

## Disclaimer

PXL Cone Launcher is an independent community project.

It is not an official product of, sponsored by, or endorsed by Mojang Studios, Microsoft, Prism Launcher, PineconeMC, Modrinth, CurseForge or Ely.by.

Minecraft is a trademark of Microsoft / Mojang Studios.

---

## License

The final licensing and attribution files will follow the requirements of the PineconeMC / Prism Launcher upstream code used by the project.

The intended launcher codebase is based on GPL-licensed upstream software, so corresponding source and license notices will be provided with distributed builds.
