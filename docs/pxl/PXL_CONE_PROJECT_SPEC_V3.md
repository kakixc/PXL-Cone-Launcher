# PXL Cone Launcher — Project Spec v3

> **Spec version:** 3.0  
> **Working name:** PXL Cone Launcher  
> **Tagline:** A fast, user-friendly PineconeMC-based Minecraft launcher.  
> **Base project:** PineconeMC (`ElyPrismLauncher/Launcher`)  
> **UI direction:** modern and simple like the Modrinth App, while keeping the power, speed and low overhead of PineconeMC/Prism.

---

## 1. Project idea

PXL Cone Launcher is an open-source fork of **PineconeMC**, not a rewrite of Minecraft launcher logic from scratch.

The goal is to keep everything that makes PineconeMC/Prism useful:

- multi-instance architecture;
- Minecraft version management;
- Fabric / Forge / NeoForge / Quilt support;
- Modrinth and CurseForge integration;
- Microsoft accounts;
- PineconeMC's Ely.by OAuth integration;
- automatic Java/runtime handling already present upstream;
- portable and cross-platform behavior;
- low idle resource usage;
- native Qt/C++ core;
- the PineconeMC cat button / cats.

But replace the intimidating/dated workflow with a cleaner UI and better migration, backup, export and diagnostics.

The project should feel like:

**Modrinth App UX + PineconeMC feature set + Prism-level control.**

---

## 2. Why fork PineconeMC instead of Prism Launcher

PineconeMC already contains the account/authentication work we want:

- Ely.by account support;
- OAuth2 login instead of asking the launcher for an Ely.by password;
- Ely.by-specific Minecraft profile handling;
- Ely.by authlib patches;
- Microsoft accounts still supported;
- PineconeMC-specific connectivity/mirror work;
- PineconeMC-specific Java/runtime improvements.

This avoids reimplementing and maintaining those changes ourselves.

Important PineconeMC source areas currently include:

```text
launcher/minecraft/auth/AuthSession.h
launcher/minecraft/auth/steps/ElyStep.cpp
launcher/minecraft/auth/steps/ElyDeviceCodeStep.cpp
launcher/minecraft/auth/steps/MinecraftProfileStepEly.cpp
launcher/minecraft/update/ElyPatchTask.cpp
launcher/ui/dialogs/ElyLoginDialog.cpp
launcher/ui/pages/global/AccountListPage.cpp
launcher/LaunchController.cpp
launcher/Application.cpp
CMakeLists.txt
```

**Rule:** do not rewrite authentication during the first stages of the project. Keep PineconeMC auth code as close to upstream as possible.

---

## 3. Existing projects / competition

### PineconeMC

The direct upstream.

At the time this document was written, the public repository has roughly 1k GitHub stars and dozens of forks. It is a PineconeMC/Prism-style Qt launcher rather than a complete modern UX redesign.

### Halky Launcher

A very close idea already exists: **Halky Launcher**.

It advertises:

- a modern UI;
- Ely.by support;
- custom authentication;
- standard Prism/MultiMC functionality.

However, it is currently based on **Freesm Launcher / Prism lineage**, not directly on PineconeMC, and is a very small project.

This makes it important as a UX reference and competitor, but it does **not** remove the reason for a PineconeMC-native fork.

### XMCL

XMCL is another important competitor/reference:

- modern UX;
- Modrinth + CurseForge;
- Ely.by;
- multi-instance;
- disk-efficient shared resources;
- multiple authentication systems.

It is Electron/TypeScript based, while PXL Cone should deliberately stay on the PineconeMC C++/Qt foundation to preserve the lightweight native architecture.

### Refract

Refract is a modern launcher built with Tauri + React and already focuses on modern instance management, worlds/backups, content management and imports.

Again, use it as a UX/product reference, not as the technical base.

### Modrinth App

Primary design inspiration.

Useful ideas:

- simple Play/Library screen;
- obvious content installation;
- good onboarding;
- clear instance cards;
- integrated browsing;
- less technical information until it is needed.

Do **not** blindly clone its UI. PXL Cone should have its own visual identity.

---

## 4. Naming

### Recommended name

# **PXL Cone Launcher**

Reasons:

- `PXL` matches the existing PXL branding direction;
- `Cone` hints at PineconeMC ancestry;
- still sounds like its own product;
- avoids using `PineconeMC Fork` as the product name;
- avoids the terrible search/name collision of plain **Pixel Launcher**, which is already strongly associated with Google's Android launcher.

Suggested repository description:

> **PXL Cone is a fast, user-friendly Minecraft launcher based on PineconeMC, with modern instance management, Ely.by support, smart migration and portable modpack backups.**

Suggested About/README wording:

> PXL Cone Launcher is based on PineconeMC and ultimately on Prism Launcher. It is an independent project and is not endorsed by PineconeMC, Prism Launcher, Mojang, Microsoft or Ely.by.

Possible package/application IDs:

```text
app.pxlcone.launcher
org.pxlcone.Launcher
```

Choose one convention once and do not keep renaming it.

---

## 5. Core design principles

### 5.1 Fast first

Do not replace Qt Widgets with Electron just to get modern visuals.

Targets:

- native C++/Qt application;
- fast cold start;
- minimal idle CPU usage;
- no permanent embedded Chromium process;
- no web UI required to open the library;
- lazy-load network content;
- lazy-load screenshots and large cover images;
- cache icons and project metadata;
- never block the main UI thread on downloads.

A modern UI does not require a web stack.

### 5.2 Simple by default, powerful when needed

Default interface should expose:

- Play;
- Instances / Library;
- Discover;
- Downloads / Tasks;
- Accounts;
- Settings.

Advanced settings remain available, but should not dominate normal use.

### 5.3 Never remove Prism/Pinecone power features just to make the UI clean

If a feature is too technical for the main screen:

- move it to `Advanced`;
- place it in an instance settings page;
- place it in a context menu;
- add search in settings.

Do not delete it.

### 5.4 Offline-friendly

The launcher should still be useful when network services are unavailable.

- existing installed instances must remain visible;
- worlds must remain accessible;
- local instance settings must remain editable;
- offline launch should not perform unnecessary account refreshes;
- cached artwork should work offline;
- transfer archives can optionally include all required files.

---

## 6. Main UI

## 6.1 Home / Library

Replace the traditional toolbar-heavy home screen with a content-oriented layout.

Possible layout:

```text
┌──────────────────────────────────────────────────────────────┐
│ PXL Cone      Library    Discover    Downloads      Account │
├───────────────┬──────────────────────────────────────────────┤
│ Groups        │ Search instances...                  + Add  │
│               │                                              │
│ Favorites     │ [Create 1.21.8]   [Vanilla]   [Tech Pack]   │
│ Modpacks      │ [icon]             [icon]      [icon]        │
│ Vanilla       │                                              │
│ Servers       │                                              │
│               │                                              │
├───────────────┴──────────────────────────────────────────────┤
│ Selected instance: Create 1.21.8                  PLAY      │
└──────────────────────────────────────────────────────────────┘
```

Instance card information:

- icon;
- instance name;
- Minecraft version;
- loader;
- optional modpack name;
- last played;
- small status indicator;
- Play button appears on hover/selection.

Do not put 15 action buttons on every card.

Secondary actions belong in:

- right-click menu;
- `...` menu;
- instance details page.

---

## 6.2 Instance details

Tabs/sections:

- Overview;
- Mods;
- Worlds;
- Resource Packs;
- Shaders;
- Screenshots;
- Java & Performance;
- Settings;
- Logs.

Overview should contain:

- large Play button;
- Minecraft version;
- loader;
- Java status;
- RAM;
- last played;
- quick actions;
- update state.

---

## 6.3 Discover

Single discovery flow for:

- modpacks;
- mods;
- shaders;
- resource packs.

Sources:

- Modrinth;
- CurseForge where API/legal constraints permit.

Filters:

- Minecraft version;
- loader;
- category;
- source;
- installed/not installed;
- update available.

---

## 7. Preserve the cats

**Mandatory:** do not remove the PineconeMC cat feature/button.

PineconeMC currently even exposes a setting to disable the Cat Button, which means it is an intentional part of the upstream UX.

Requirements:

- cats remain available;
- default can stay compatible with upstream behavior;
- UI redesign must have a place for the Cat Button;
- any upstream cat packs/behavior should continue to work;
- never treat it as dead code during cleanup.

---

## 8. Major problems PXL Cone should solve

### 8.1 Old/intimidating UI

Solution:

- redesigned home/library;
- less toolbar clutter;
- consistent spacing and typography;
- clear hierarchy;
- advanced options hidden until needed;
- useful empty states;
- first-run onboarding.

### 8.2 Poor migration between launchers/computers

Solution:

Create a dedicated **Migration Center**.

Functions:

- detect existing launcher libraries;
- import instances;
- export one instance;
- export selected instances;
- export the entire launcher profile;
- export only worlds;
- transfer to another operating system;
- show estimated archive size before exporting.

Launchers to target first:

1. PineconeMC;
2. Prism Launcher;
3. Modrinth App;
4. CurseForge;
5. official `.minecraft`;
6. MultiMC-compatible instances.

Do not hardcode Windows-only absolute paths into exported packs.

---

## 9. PXL Transfer format

Working extension:

```text
.pxlt
```

Technically it can be a ZIP container with a documented manifest.

Example:

```text
My-PC-Transfer.pxlt
├── manifest.json
├── instances/
│   ├── survival/
│   │   ├── instance.json
│   │   ├── content.json
│   │   ├── worlds/
│   │   ├── config/
│   │   └── local-content/
│   └── create-pack/
│       └── ...
├── icons/
└── checksums.json
```

### 9.1 `manifest.json`

Suggested fields:

```json
{
  "formatVersion": 1,
  "createdBy": "PXL Cone",
  "createdAt": "ISO-8601",
  "mode": "thin",
  "instances": [
    {
      "id": "create-pack",
      "name": "Create Pack",
      "minecraftVersion": "1.21.1",
      "loader": {
        "type": "fabric",
        "version": "..."
      }
    }
  ]
}
```

### 9.2 `content.json`

Do **not** identify downloadable mods only by filename.

Store as much deterministic data as possible:

```json
{
  "type": "mod",
  "name": "Sodium",
  "source": "modrinth",
  "projectId": "...",
  "versionId": "...",
  "downloadUrl": "...",
  "sha256": "...",
  "fileName": "sodium-....jar",
  "side": "client"
}
```

Priority for restoring a file:

1. exact provider + version ID;
2. known download URL;
3. exact cryptographic hash lookup if a provider supports it;
4. bundled local file;
5. ask the user.

Never silently install a different version just because its name looks similar.

---

## 10. PXL file formats

PXL Cone should deliberately have more than one export/share format because each solves a different problem.

### `.pxlconep` — PXL Cone Pack

`P` = **Pack**.

Primary purpose:

- send a modpack/instance to a friend through Telegram, Discord, email or any messenger;
- one-click install when PXL Cone is installed;
- preserve exact versions;
- support both tiny online packs and fully offline packs.

Example:

```text
Create-Survival.pxlconep
```

When associated with PXL Cone, double-clicking should open an install preview:

```text
Create Survival

Minecraft 1.21.1
Fabric
87 mods
1 world
Estimated installed size: 640 MB

[Install]
```

A `.pxlconep` file should internally be a documented ZIP-compatible container with a custom extension.

Possible structure:

```text
Create-Survival.pxlconep
├── manifest.json
├── content.json
├── overrides/
│   ├── config/
│   ├── options.txt
│   └── servers.dat
├── worlds/          # optional
├── local-files/     # optional
├── bundled-content/ # offline mode only
└── icon.png
```

Two modes:

#### Smart Pack

Small file.

Contains:

- exact Minecraft version;
- exact loader + version;
- exact Modrinth/CurseForge project/version IDs;
- hashes;
- configs;
- options if selected;
- server list if selected;
- user-created icons;
- worlds only if selected;
- non-downloadable/local content only when required.

The recipient's PXL Cone reconstructs the pack by downloading exact versions.

#### Offline Pack

Same `.pxlconep` extension, but all selected content required for installation is bundled.

Can include:

- mods;
- resource packs;
- shaders;
- configs;
- worlds;
- local/custom files;
- icons;
- other selected instance files.

Use when the recipient may not have internet access or when exact files may disappear from upstream providers.

The manifest must state whether the pack is `smart` or `offline`.

---

### `.pxlt` — PXL Transfer

`T` = **Transfer**.

Purpose is not primarily sharing a public modpack.

It is for:

- moving your own instances between computers;
- Windows -> Linux migration;
- Linux -> Windows migration;
- transferring multiple instances at once;
- backing up PXL Cone data;
- moving worlds/settings/custom local data.

`.pxlt` may contain multiple instances.

Example:

```text
Danil-PC-2026.pxlt
├── manifest.json
├── instances/
│   ├── survival/
│   └── create-pack/
├── icons/
└── checksums.json
```

Modes:

- Thin transfer;
- Offline transfer.

Do not bundle account passwords or reusable OAuth secrets by default.

---

## 11. Thin transfer vs Offline transfer

### Thin transfer

Best for moving between PCs while keeping the archive small.

Include:

- worlds;
- configs;
- options;
- server list;
- screenshots if selected;
- custom local files;
- instance metadata;
- exact mod/resource/shader references;
- hashes;
- user-created icons.

Do not include downloadable files when they can be reconstructed exactly.

On import:

1. read manifest;
2. recreate instance;
3. download exact Minecraft/loader/runtime components;
4. download exact known mod versions;
5. verify hashes;
6. copy local data/worlds;
7. report anything that could not be restored.

### Offline transfer

Checkbox:

> **Include everything for offline restore**

Include all required instance content for the selected transfer scope.

Use compression and deduplication where practical.

---

## 12. Export Center

The new Export Center must clearly explain the difference between native PXL formats, ecosystem formats and a truly generic offline ZIP.

### Export as

- **PXL Cone Pack `.pxlconep` — Smart**
- **PXL Cone Pack `.pxlconep` — Offline**
- **PXL Transfer `.pxlt` — Thin**
- **PXL Transfer `.pxlt` — Offline**
- **Modrinth `.mrpack`**
- **CurseForge pack `.zip`**
- **Universal Offline ZIP `.zip`**
- **World only `.zip`**
- **Configuration only `.zip`**

Do not call Prism/Pinecone's existing instance ZIP "universal".

A Prism/Pinecone instance ZIP may contain launcher-specific metadata/layout and should be treated as a launcher-specific export.

---

## 13. Universal Offline ZIP

This is a separate v2 requirement.

PXL Cone must provide a **normal ZIP that does not depend on Prism Launcher, PineconeMC or PXL Cone metadata to be useful**.

Goal:

> The user can extract the ZIP manually or point another launcher at the included Minecraft files without needing PXL Cone.

This is intentionally different from Prism/Pinecone's launcher-specific instance ZIP.

### Structure

Example:

```text
Create-Survival-Offline.zip
├── mods/
├── config/
├── resourcepacks/
├── shaderpacks/
├── saves/              # optional
├── screenshots/        # optional
├── options.txt         # optional
├── servers.dat         # optional
├── local/              # optional additional selected files
└── README.txt
```

Important principles:

- root should represent normal Minecraft-side content, not PXL/Prism's internal instance folder;
- no PXL-specific file is required to use the archive;
- no absolute paths;
- no launcher-specific path names unless the user explicitly selected an advanced launcher export;
- extracted files should be understandable by a human;
- README explains where the folders belong;
- export UI clearly warns that Minecraft version, loader and Java may still need to be installed separately in third-party launchers.

### Optional compatibility metadata

The ZIP may optionally include non-required informational metadata, for example:

```text
pxl-info.json
```

But:

- the archive must remain usable if another launcher ignores this file;
- all actual Minecraft-side files must still be in conventional folders.

### Offline behavior

The archive contains the selected mod/resource/shader files themselves rather than references.

Therefore it can work without redownloading them.

### Worlds

Worlds may be:

- included;
- excluded;
- exported separately.

Default for a modpack intended for public sharing should be **worlds excluded** unless the user explicitly enables them.

### Why this format exists

Use cases:

- friend uses a launcher PXL Cone does not support;
- friend wants to copy files manually;
- source mod versions may disappear;
- internet unavailable;
- archive needs to be understandable years later.

This format prioritizes compatibility over minimum archive size.

---

## 14. Launcher-specific ZIP vs Universal Offline ZIP

Do not merge these concepts.

### Launcher-specific instance ZIP

May contain:

- instance config;
- launcher metadata;
- launcher icon keys;
- instance-specific structure;
- Prism/Pinecone/PXL settings.

Useful for importing into compatible launcher families.

### Universal Offline ZIP

Contains conventional Minecraft-side files.

Useful almost anywhere, but may require the recipient to select the right:

- Minecraft version;
- loader;
- loader version;
- Java version.

The Export Center should label these formats clearly so normal users cannot accidentally choose the wrong one.

---

## 15. Suggested share workflow

Right-click an instance:

```text
Share / Export
```

Then quick choices:

```text
Send to a friend
  PXL Cone Pack (small)
  PXL Cone Pack (offline)

Use another launcher
  Modrinth Pack
  CurseForge Pack
  Universal Offline ZIP

Backup / move my PC
  PXL Transfer (small)
  PXL Transfer (offline)

Advanced...
```

This is friendlier than showing file extensions first.

Advanced mode can expose exact format names and detailed include/exclude controls.

---

## 15.1 File association

Register `.pxlconep` and `.pxlt` with PXL Cone on supported desktop operating systems.

Desired behavior:

- double click `.pxlconep` -> Pack Install Preview;
- double click `.pxlt` -> Transfer Restore Preview;
- never immediately install/overwrite data without showing the user what will happen.

For messenger downloads, opening the file should feel like opening an installer/package rather than manually browsing for an import file.

---

## 16. Icon handling

Follow this rule:

### Remote/reconstructable icon

If an icon belongs to Modrinth/CurseForge/known source:

```json
{
  "kind": "remote",
  "url": "...",
  "sha256": "..."
}
```

Do not bundle it in Thin mode unless required.

### User icon

If the user selected a local image or created/cropped an icon:

- include it in `icons/`;
- reference it by relative path;
- store a hash.

### Missing remote icon

Importer should:

1. try the recorded URL;
2. fall back to provider API/project icon;
3. use a default icon;
4. never abort the entire import just because an icon is missing.

---

## 17. Worlds

Worlds are irreplaceable user data and must be treated differently from downloadable mods.

Features:

- world-only export;
- automatic backup before destructive operations;
- optional compression;
- world metadata preview;
- date/size/last played;
- restore to same or new instance;
- detect duplicate world folder names;
- never overwrite a world without confirmation.

Future:

- incremental backups;
- snapshot history;
- automatic backup before modpack update.

---

## 18. Deduplicated local content

Optional later optimization.

Instead of storing identical mod files repeatedly:

```text
content-store/
└── sha256/
    └── ab/
        └── abcdef....jar
```

Instances may use:

- hard links when safe;
- reflinks/copy-on-write where available;
- normal copies as fallback.

Never make instance integrity depend on symlink behavior that breaks on Windows or removable drives.

This feature should come **after** the normal launcher is stable.

---

## 19. Smart import

When importing an unknown ZIP:

Inspect for:

- `modrinth.index.json`;
- CurseForge `manifest.json`;
- MultiMC/Prism instance metadata;
- `.minecraft` structure;
- `mods/`;
- `saves/`;
- `config/`.

Then show:

> We think this is a Modrinth pack.  
> Minecraft 1.21.1 • Fabric • 83 mods  
> Import as a new instance?

If confidence is low, ask rather than guessing.

---

## 20. Crash Doctor

Later major feature.

After a failed launch, do not show only an exit code.

Pipeline:

1. collect exit code;
2. parse Minecraft log;
3. parse crash report if present;
4. look for known deterministic patterns;
5. identify likely conflicting/missing mods;
6. show plain-language explanation;
7. offer safe actions.

Example:

> Minecraft failed because Mod A requires Fabric API 0.XX or newer.  
> Installed: 0.YY  
> Required: 0.XX  
> **[Update dependency]**

### AI policy

Do not send logs to an AI service automatically.

Default diagnosis should be local rules.

If an optional AI explanation is added later:

- explicit user action;
- show what data will be uploaded;
- redact obvious access tokens, usernames and local paths where practical;
- provide an offline mode;
- launcher remains fully usable without AI.

---

## 21. Network / mirror diagnostics

PineconeMC already has connectivity/mirror behavior.

Improve presentation rather than rewriting it immediately.

Instead of:

> Network request failed.

Show:

```text
Pinecone metadata      OK        82 ms
GitHub mirror          OK       140 ms
Modrinth               OK        71 ms
CurseForge             ERROR
Ely.by                  OK        95 ms
Microsoft auth          OK       104 ms
```

Actions:

- Retry;
- Copy diagnostics;
- Change mirror;
- Open advanced network settings.

Never freeze the whole UI while testing services.

---

## 22. Account UX

Account page should clearly separate:

- Microsoft;
- Ely.by;
- supported offline/local account modes inherited from upstream.

For Ely.by:

- keep PineconeMC OAuth flow;
- do not create a fake password form;
- do not store Ely.by passwords;
- keep tokens in the same secure/compatible mechanism used by upstream until there is a strong reason to change it.

Do not mix account-system redesign with the initial visual redesign.

---

## 23. Settings UX

Top-level sections:

```text
General
Appearance
Accounts
Minecraft
Java & Performance
Storage
Downloads & Network
Integrations
Privacy
Advanced
```

Add settings search.

Advanced contains the technical switches users expect from Prism/Pinecone.

---

## 24. Performance rules

### Do

- stay on Qt 6 / C++;
- use existing Pinecone models/controllers;
- virtualize/optimize large lists where needed;
- load thumbnails asynchronously;
- cache scaled icons;
- debounce search;
- use background tasks for filesystem scans;
- avoid blocking network calls in UI thread;
- keep animations short and optional;
- respect reduced-motion preference if possible.

### Do not

- embed Chromium just for the home screen;
- rewrite the launcher in Electron;
- create a local Node/Python server to drive the UI;
- poll APIs constantly;
- animate everything;
- download all project metadata at startup;
- scan every world/mod recursively on every frame or every selection change.

---

## 25. Source files likely to matter first

The current PineconeMC tree has a traditional Qt Widgets UI.

### Main window

```text
launcher/ui/MainWindow.cpp
launcher/ui/MainWindow.h
launcher/ui/MainWindow.ui
```

This is the biggest early hotspot.

Do not immediately rewrite the entire ~large `MainWindow.cpp` in one Codex prompt.

Instead:

1. preserve existing signals/actions;
2. introduce new widgets/components;
3. move presentation logic out gradually;
4. keep existing launch/update/business logic intact.

### Instance list/view

```text
launcher/ui/instanceview/
```

Likely target for:

- modern instance cards;
- selection behavior;
- grouping;
- keyboard navigation.

### Global/settings pages

```text
launcher/ui/pages/
launcher/ui/pages/global/
```

### Mod platform/import UI

```text
launcher/ui/pages/modplatform/ImportPage.cpp
launcher/ui/pages/modplatform/ImportPage.ui
```

Current upstream already recognizes/imports formats including CurseForge flows. Extend existing import infrastructure instead of starting from an empty implementation.

### Current export UI

```text
launcher/ui/dialogs/ExportInstanceDialog.cpp
launcher/ui/dialogs/ExportInstanceDialog.h
launcher/ui/dialogs/ExportInstanceDialog.ui

launcher/ui/dialogs/ExportPackDialog.cpp
launcher/ui/dialogs/ExportPackDialog.h
```

The current main window exposes separate export paths including:

- generic ZIP;
- Modrinth pack;
- CurseForge/Flame pack.

Build the new Export Center on top of these mechanisms where possible.

### Authentication

```text
launcher/minecraft/auth/
launcher/minecraft/auth/steps/ElyStep.cpp
launcher/minecraft/auth/steps/ElyDeviceCodeStep.cpp
launcher/minecraft/auth/steps/MinecraftProfileStepEly.cpp
launcher/minecraft/update/ElyPatchTask.cpp
launcher/ui/dialogs/ElyLoginDialog.cpp
launcher/ui/pages/global/AccountListPage.cpp
```

Avoid unnecessary changes.

### Application / launching

```text
launcher/Application.cpp
launcher/LaunchController.cpp
```

Treat as core logic. UI work should not casually modify these.

### Build

```text
CMakeLists.txt
launcher/CMakeLists.txt
buildconfig/
program_info/
```

These will be needed for:

- rebranding;
- package name;
- application ID;
- icons;
- API/client IDs;
- new source files.

---

## 26. Suggested new PXL Cone files/modules

Do not put every new feature back into `MainWindow.cpp`.

Suggested structure:

```text
launcher/ui/pxl/
├── HomePage.cpp
├── HomePage.h
├── InstanceCard.cpp
├── InstanceCard.h
├── InstanceDetailsPage.cpp
├── InstanceDetailsPage.h
├── NavigationSidebar.cpp
├── NavigationSidebar.h
├── TaskCenter.cpp
├── TaskCenter.h
├── MigrationCenter.cpp
├── MigrationCenter.h
├── ExportCenter.cpp
└── ExportCenter.h

launcher/transfer/
├── TransferManifest.cpp
├── TransferManifest.h
├── TransferExporter.cpp
├── TransferExporter.h
├── TransferImporter.cpp
├── TransferImporter.h
├── ContentReference.cpp
├── ContentReference.h
├── HashVerifier.cpp
└── HashVerifier.h

launcher/migration/
├── LauncherDetector.cpp
├── LauncherDetector.h
├── PrismImporter.cpp
├── PrismImporter.h
├── ModrinthImporter.cpp
├── ModrinthImporter.h
├── CurseForgeImporter.cpp
├── CurseForgeImporter.h
├── VanillaImporter.cpp
└── VanillaImporter.h

launcher/diagnostics/
├── CrashAnalyzer.cpp
├── CrashAnalyzer.h
├── KnownIssueRules.cpp
├── KnownIssueRules.h
├── NetworkDiagnostics.cpp
└── NetworkDiagnostics.h
```

Names can change after inspecting upstream conventions.

Follow PineconeMC/Prism coding style instead of forcing an unrelated architecture.

---

## 27. Upstream strategy

This matters a lot.

Configure Git remotes like:

```text
origin    -> PXL Cone repository
pinecone  -> ElyPrismLauncher/Launcher
```

Potentially keep Prism as an additional read-only reference remote.

Goal:

- regularly pull PineconeMC security/auth/Minecraft compatibility updates;
- keep PXL-specific UI code isolated;
- minimize edits to upstream core files;
- prefer adapters/new modules over rewriting core;
- document every unavoidable core patch.

A beautiful fork that becomes impossible to merge with upstream after three months is a failed architecture.

---

## 28. Git branches

Suggested:

```text
main
develop
feature/ui-shell
feature/instance-cards
feature/transfer-format
feature/migration
feature/export-center
feature/crash-doctor
upstream-sync/*
```

Do not let Codex make a 20,000-line mega commit.

---

## 29. Codex difficulty

### Rebrand only

**Low difficulty**

Examples:

- name;
- icons;
- package IDs;
- text;
- About dialog.

### Modernize styles while keeping layout

**Low to medium difficulty**

Qt stylesheets, spacing, icons and small widget rearrangement are manageable.

### New Modrinth-like home/library while preserving existing logic

**Medium difficulty**

The difficult part is not drawing cards. The difficult part is correctly wiring:

- existing models;
- selections;
- groups;
- launch actions;
- running state;
- tasks;
- context actions;
- accessibility;
- keyboard navigation.

### Complete instance-details redesign

**Medium to high difficulty**

Many existing pages need to be reused or embedded cleanly.

### Migration Center

**Medium to high difficulty**

Filesystem differences and partial/corrupt instances create edge cases.

### Thin `.pxlt` transfer system

**High difficulty**

Needs:

- deterministic manifests;
- provider IDs;
- hashes;
- partial failure handling;
- cross-platform paths;
- missing/removed mod handling;
- security validation of archive paths.

### “Works in every other launcher” export

**High difficulty if interpreted literally.**

No custom manifest can force other launchers to understand it.

The correct solution is multi-format export to standards they already support.

### Crash Doctor

**High difficulty**

Start with deterministic rules. AI can come later.

---

## 30. How to use Codex on this project

Codex is well suited to this project **if tasks are small and testable**.

Bad prompt:

> Redesign PineconeMC like Modrinth and fix everything.

Good task:

> Build PineconeMC unchanged on Fedora/Windows. Document the exact build command and dependencies. Do not modify source behavior.

Then:

> Create a new `InstanceCard` Qt widget that displays the existing instance icon, name, Minecraft version and running state. Do not replace InstanceView yet. Add it behind a compile-time/dev-only experiment.

Then:

> Replace only the visual instance grid on the home screen using the existing model. Preserve all existing actions and keyboard behavior. Run tests and build before committing.

Then:

> Add a transfer manifest parser/writer with unit tests. Do not add UI yet.

### Rules for Codex

1. Inspect relevant upstream files before editing.
2. Build the unmodified project first.
3. One subsystem per task.
4. Never “clean up” unrelated Pinecone authentication code.
5. Never remove features because the new UI does not expose them yet.
6. Add tests for serialization/migration logic.
7. Keep commits small.
8. Build after every meaningful change.
9. Treat warnings in archive extraction/path handling seriously.
10. Before merging upstream, compare Pinecone changes carefully.

---

## 31. Security requirements for transfer archives

Archive import is a security-sensitive feature.

Must defend against:

- `../` path traversal;
- absolute paths in archives;
- symlink escapes;
- huge decompression bombs;
- duplicate conflicting paths;
- invalid UTF/path handling;
- writing outside the chosen instance directory.

Never extract an untrusted archive with a naive “unzip everything” implementation.

For Thin mode:

- verify downloaded hashes when known;
- use HTTPS provider endpoints;
- do not execute content during import;
- show failed verification clearly.

---

## 32. First-run onboarding

First launch:

1. language;
2. theme;
3. detect Java;
4. account choice;
5. detect existing launchers;
6. offer import;
7. finish.

Example:

> We found 7 Prism instances and 3 Modrinth instances.  
> Import them into PXL Cone?

Options:

- Import all;
- Choose;
- Later.

No scary wall of configuration.

---

## 33. Import from another OS/computer

The transfer format must use logical/relative paths.

Never store:

```text
C:\Users\Danil\AppData\Roaming\...
/home/danil/.local/share/...
```

as required restore destinations.

Instead store:

```text
instanceRoot
minecraftRoot
saves/world1
config/example.toml
```

Importer maps those paths to the destination OS.

---

## 34. Full profile migration

Later feature:

Export:

- selected instances;
- worlds;
- settings;
- launcher groups;
- custom icons;
- optional accounts **without exporting raw secrets by default**;
- Java preferences;
- UI settings.

### Accounts

Do not casually place reusable OAuth refresh tokens into a portable ZIP.

Default migration should require reauthentication on the new PC.

If secure credential migration is ever implemented, it requires a separate threat model and encryption design.

---

## 35. Update safety

Before updating a managed pack:

- detect world presence;
- optionally create backup/snapshot;
- show changed mods;
- identify removed local modifications;
- allow rollback.

Example:

```text
Update Create Ultimate
1.4.1 -> 1.5.0

+ 12 updated
+ 2 added
- 1 removed

[Create world backup] ✓

Update
```

---

## 36. Accessibility

Modern should not mean mouse-only.

Requirements:

- keyboard navigation;
- visible focus;
- sensible tab order;
- screen-reader labels where Qt supports them;
- scalable UI;
- no critical information encoded only by color;
- high-contrast compatibility;
- reduced-motion option if animations are introduced.

---

## 37. Things NOT to do in v1

Do not try to ship all of this at once.

Not v1:

- cloud account system;
- cloud world sync;
- paid SaaS;
- social network;
- friends/chat;
- P2P mod distribution;
- built-in browser;
- AI everywhere;
- launcher rewrite in Rust/JS/Python;
- custom Minecraft authentication protocol;
- custom mod-hosting platform;
- full deduplicated content store;
- auto-fixing every crash.

First make a polished launcher.

---

## 38. MVP

A realistic first useful release:

### MVP 0 — clean fork

- fork PineconeMC;
- build Windows + Linux;
- rebrand to PXL Cone;
- own application IDs/icons;
- preserve all Pinecone features;
- preserve cats;
- automated CI builds.

### MVP 1 — new shell

- new navigation;
- modern library;
- modern instance cards;
- modern dialogs/theme;
- settings search;
- unchanged core launcher behavior.

### MVP 2 — migration

- import Prism/Pinecone;
- import Modrinth;
- import CurseForge;
- detect `.minecraft`;
- copy worlds/settings safely.

### MVP 3 — Export Center

- existing ZIP export surfaced cleanly;
- `.mrpack`;
- CurseForge export;
- world-only export;
- initial `.pxlt` Thin/Offline.

### MVP 4 — polish

- task/download center;
- network diagnostics;
- error UX;
- accessibility pass;
- performance profiling.

### After stable release

- Crash Doctor;
- snapshot/rollback;
- deduplicated store;
- optional AI explanation;
- optional server/team features.

---

## 39. Licensing / attribution

PineconeMC states that launcher code is available under **GPL-3.0-only**.

Therefore PXL Cone, as a distributed derivative of that launcher code, must comply with the GPL requirements.

Also:

- make it clear that PXL Cone is an independent fork;
- do not imply endorsement by Prism/Pinecone/Ely.by;
- review and replace upstream branding/assets as required;
- review API/client IDs in `CMakeLists.txt`;
- register/use appropriate own client/API credentials where required;
- preserve required copyright/license notices;
- publish corresponding source for distributed GPL builds.

PineconeMC's README also notes separate licensing for logo/assets. Prefer creating new PXL Cone branding instead of reusing the Pinecone logo.

This document is a technical project plan, not legal advice.

---

## 40. API keys / client IDs

Before public distribution, inspect:

```text
CMakeLists.txt
buildconfig/
```

Pay special attention to:

- Microsoft authentication client ID;
- Ely.by client ID;
- CurseForge API/client configuration;
- updater URLs;
- news URLs;
- website/support links.

Do not accidentally ship Pinecone/Prism private or project-specific credentials as if they belonged to PXL Cone.

---

## 41. Testing checklist

Every release should test at minimum:

### Operating systems

- Windows 11;
- Fedora KDE;
- another common Linux environment/Flatpak if supported.

Later:

- macOS.

### Accounts

- Microsoft;
- Ely.by;
- offline behavior supported by upstream.

### Versions

- current Minecraft release;
- one older popular release;
- Fabric;
- Forge;
- NeoForge;
- Quilt where supported.

### Packs

- Modrinth install/import/export;
- CurseForge install/import/export;
- plain instance ZIP;
- PXL Thin transfer;
- PXL Offline transfer.

### User data

- worlds;
- custom icons;
- screenshots;
- options;
- resource packs;
- shaders;
- configs.

### Failure tests

- no internet;
- provider unavailable;
- removed mod version;
- wrong checksum;
- corrupt archive;
- archive path traversal attempt;
- full disk;
- destination already exists.

---

## 42. Definition of success

PXL Cone is successful when a normal user can:

1. install the launcher;
2. log in;
3. import an old launcher;
4. see all instances immediately;
5. click Play without understanding Java paths;
6. install/update mods without opening a browser;
7. move their worlds and packs to a second PC safely;
8. send a `.pxlconep` to a friend for one-click installation;
9. export a normal offline ZIP that remains useful outside the Prism/Pinecone/PXL ecosystem;
10. still access every advanced feature an experienced Prism/Pinecone user expects;
11. do all of this without turning the launcher into a heavyweight Electron application.

---

## 43. First Codex task

Use this as the first implementation prompt:

```text
We are creating PXL Cone Launcher, an independent GPL-3.0 open-source fork of
ElyPrismLauncher/Launcher (PineconeMC).

Goal for this task:
1. Inspect the repository architecture and current build documentation.
2. Build the repository unchanged.
3. Identify the exact files responsible for:
   - branding/application IDs,
   - MainWindow,
   - instance view,
   - themes,
   - account UI,
   - Ely.by authentication,
   - import/export,
   - updater/package metadata.
4. Create docs/ARCHITECTURE-PXL.md summarizing those findings.
5. Do NOT redesign UI yet.
6. Do NOT modify Ely.by/MS authentication behavior.
7. Do NOT remove the Cat Button.
8. Keep changes minimal and build/test before finishing.

The long-term product goal is a Modrinth-like user-friendly UI while preserving
PineconeMC performance and functionality, plus a cross-platform transfer/export
system.
```

This forces Codex to understand the codebase before it starts randomly rewriting it.

---

## 44. Second Codex task

After the clean build succeeds:

```text
Create the first PXL Cone UI prototype without replacing core launcher logic.

Requirements:
- Add a modern library/home widget using Qt 6 Widgets/C++.
- Reuse the existing instance model.
- Display instance icon, name, Minecraft version/loader where available, and
  running/selected state.
- Preserve existing launch behavior and all existing instance actions.
- Do not change authentication, Minecraft launch logic, loaders or updater code.
- Do not remove the Cat Button.
- Keep the old view available behind a development toggle until the new view
  reaches feature parity.
- Add keyboard navigation/accessibility labels.
- Avoid synchronous network/filesystem work on the UI thread.
- Build and run tests.
```

---


## 45. Target platforms

PXL Cone is a **single C++/Qt codebase** with platform-specific builds.

Primary supported platforms for the first public releases:

### Windows

Required release artifacts:

```text
PXL-Cone-<version>-Windows-x64-Setup.exe
PXL-Cone-<version>-Windows-x64-Portable.zip
```

Goals:

- normal installer;
- portable build;
- Start Menu shortcuts;
- file associations for `.pxlconep` and `.pxlt`;
- uninstall support;
- no requirement for WSL, Python, Node.js or a separate launcher runtime.

### Linux

Linux is a first-class platform, not an afterthought.

Required package targets:

```text
Fedora / RPM-based:
pxl-cone-<version>-1.x86_64.rpm

Ubuntu / Debian-based:
pxl-cone_<version>_amd64.deb

Arch Linux:
pxl-cone-<version>-1-x86_64.pkg.tar.zst

Generic Linux:
PXL-Cone-<version>-x86_64.AppImage

Flatpak:
PXL-Cone-<version>.flatpak
```

Optional additional artifact:

```text
PXL-Cone-<version>-Linux-x86_64.tar.zst
```

for users who want a simple portable archive.

### macOS

Not required for the first release.

The architecture should avoid unnecessary Windows/Linux-only assumptions so macOS can be added later.

Potential future artifacts:

```text
PXL-Cone-<version>-macOS.dmg
PXL-Cone-<version>-macOS.zip
```

Do not delay Windows/Linux releases just to support macOS.

---

## 46. Linux packaging strategy

The initial PXL Cone distribution strategy does **not** require maintaining custom package repositories.

The project should provide directly downloadable packages.

### Fedora / RPM systems

Ship a normal `.rpm`.

Typical direct installation:

```bash
sudo dnf install ./pxl-cone-<version>-1.x86_64.rpm
```

The package should install:

- application binary;
- required application data;
- icon;
- `.desktop` entry;
- MIME/file associations where appropriate;
- license information.

A COPR or custom DNF repository may be added later, but is **not a launch requirement**.

### Ubuntu / Debian systems

Ship a normal `.deb`.

Typical direct installation:

```bash
sudo apt install ./pxl-cone_<version>_amd64.deb
```

A PPA or APT repository may be added later, but is **not a launch requirement**.

### Arch Linux

Ship a real Arch package:

```text
pxl-cone-<version>-1-x86_64.pkg.tar.zst
```

Arch packages are created from a `PKGBUILD` with `makepkg`.

Direct installation:

```bash
sudo pacman -U pxl-cone-<version>-1-x86_64.pkg.tar.zst
```

Keep the `PKGBUILD` in the repository so users can inspect/rebuild the package.

AUR publication can happen later, but it is not required for the first release.

### Flatpak

Maintain a Flatpak manifest and produce a single-file `.flatpak` bundle for direct download.

A Flatpak bundle can be installed directly:

```bash
flatpak install PXL-Cone-<version>.flatpak
```

Important:

- the Flatpak sandbox must be tested with Java launching;
- Minecraft instance directories must be accessible in a controlled way;
- imports from existing launchers need an understandable file/folder permission flow;
- `.pxlconep` / `.pxlt` opening must work correctly;
- do not request broad filesystem access without a real need.

### Flathub

**Flathub is desirable, but optional for the first release.**

Once packaging is stable, publish PXL Cone on Flathub so users can install it through software stores such as KDE Discover and GNOME Software.

The directly downloadable `.flatpak` bundle should still exist if practical.

### AppImage

Provide AppImage as the generic fallback.

Goals:

- download;
- mark executable if required;
- launch;
- no distro-specific package manager required.

The AppImage should not become the only Linux distribution method because native packages and Flatpak integrate better with desktops.

---

## 47. Release layout

A GitHub Release / project download page should look approximately like this:

```text
PXL Cone 1.0.0

Windows
  PXL-Cone-1.0.0-Windows-x64-Setup.exe
  PXL-Cone-1.0.0-Windows-x64-Portable.zip

Linux
  PXL-Cone-1.0.0-x86_64.AppImage
  pxl-cone-1.0.0-1.x86_64.rpm
  pxl-cone_1.0.0_amd64.deb
  pxl-cone-1.0.0-1-x86_64.pkg.tar.zst
  PXL-Cone-1.0.0.flatpak

Source
  Source code archive / Git tag

Verification
  SHA256SUMS
  SHA256SUMS.sig        # later if release signing is implemented
```

The website should detect the visitor's OS only as a convenience.

Never hide the full download list.

Example:

```text
Download for Windows

Other downloads:
Linux (RPM / DEB / Arch / Flatpak / AppImage)
Portable
Source code
```

---

## 48. Build and CI strategy

Use one codebase and build per platform.

Recommended CI:

```text
push / pull request
        |
        +--> Windows build + tests
        |
        +--> Linux build + tests
        |
        +--> package smoke tests
```

Tagged release:

```text
git tag v1.0.0
        |
        +--> Windows installer
        +--> Windows portable ZIP
        +--> RPM
        +--> DEB
        +--> Arch pkg.tar.zst
        +--> AppImage
        +--> Flatpak bundle
        +--> checksums
```

Important rule:

> A feature is not finished if it silently breaks another Tier-1 platform.

Tier 1 for v1:

- Windows 11 x64;
- Fedora Linux x86_64;
- Arch-family Linux x86_64;
- Ubuntu/Debian-family Linux x86_64.

Do not require every developer to manually package every format for every commit. CI should automate as much as practical.

---

## 49. What PXL Cone is at the end

PXL Cone should **not** be marketed as merely:

> PineconeMC with a new theme.

The product is:

> **A fast native Minecraft launcher based on PineconeMC that keeps Prism/Pinecone power but makes normal everyday use dramatically easier.**

Core identity:

### 1. PineconeMC foundation

Keep:

- Microsoft accounts;
- Ely.by integration inherited from PineconeMC;
- multiple isolated instances;
- Fabric / Forge / NeoForge / Quilt support where upstream supports them;
- Modrinth / CurseForge integration;
- Java/runtime management;
- advanced configuration;
- low-overhead native Qt/C++ architecture;
- Cat Button / cats.

### 2. Modern user-friendly UI

A cleaner interface inspired by the usability of modern launchers:

- Library;
- Discover;
- Downloads/Tasks;
- Accounts;
- Settings;
- instance detail pages;
- search;
- modern cards;
- clear errors;
- simple defaults;
- advanced controls still available.

### 3. PXL Cone Pack

`.pxlconep`

One file sent through a messenger.

Two modes:

- Smart Pack;
- Offline Pack.

Double-click -> preview -> install.

### 4. PXL Transfer

`.pxlt`

For:

- PC-to-PC migration;
- Windows <-> Linux migration;
- multiple instances;
- worlds;
- configs;
- custom local data;
- Thin and Offline modes.

### 5. Universal Offline ZIP

A **normal Minecraft-content ZIP**, not a Prism/PXL-specific instance archive.

Designed so the contents remain useful in other launchers and for manual installation.

### 6. Migration Center

Import from:

- PineconeMC;
- Prism;
- Modrinth App;
- CurseForge;
- `.minecraft`;
- compatible MultiMC-style instances.

### 7. Safe updates and backups

Before risky pack changes:

- optional world backup;
- snapshot;
- changed-file/mod preview;
- rollback path.

### 8. Better diagnostics

Eventually:

- network diagnostics;
- known crash rules;
- Crash Doctor;
- optional AI explanation only with explicit user action.

### 9. Cross-platform from day one

One project:

```text
Windows
Linux
  ├── RPM
  ├── DEB
  ├── Arch pkg.tar.zst
  ├── Flatpak
  └── AppImage

macOS later
```

---

## 50. Why somebody would download PXL Cone

The launcher needs concrete reasons to switch.

### Reason A — “I like Prism/Pinecone functionality, but not the workflow/UI.”

PXL Cone offers the same class of advanced multi-instance launcher while making the common path simpler.

### Reason B — “I want to send my pack to a friend without writing instructions.”

Create `.pxlconep`.

Send one file.

Friend opens it and installs.

### Reason C — “I am moving from Windows to Linux.”

Export a `.pxlt`.

Import on the other OS.

Avoid manually discovering every instance/world/config folder.

### Reason D — “I want a pack that is not trapped in one launcher.”

Use Universal Offline ZIP, `.mrpack`, CurseForge export, or another supported standard.

### Reason E — “I want a lightweight desktop launcher.”

Keep native C++/Qt rather than rebuilding the entire app around a Chromium/Electron runtime.

### Reason F — “I want advanced features without being forced to understand all of them.”

Simple defaults first, Advanced mode when needed.

### Reason G — open source

The user can:

- inspect code;
- build it;
- report issues;
- contribute;
- fork it.

This is especially important for a launcher handling accounts, game files and downloads.

---

## 51. What NOT to claim in marketing

Never advertise unmeasured numbers.

Bad:

> Uses 70% less RAM.

unless it was actually benchmarked reproducibly.

Bad:

> Fastest Minecraft launcher.

unless there is strong evidence and a defined benchmark.

Bad:

> Works with every launcher.

No format can guarantee that.

Better:

> Export a conventional Offline ZIP, Modrinth pack or CurseForge-compatible pack.

Do not market Ely.by as a way to bypass buying Minecraft.

Describe it factually as an account/authentication system supported by PineconeMC/PXL Cone where applicable.

Do not present PXL Cone as an official Minecraft, Mojang, Microsoft, Prism, PineconeMC, Modrinth or Ely.by product.

---

## 52. Marketing positioning

Primary message:

> **Prism/Pinecone power without Prism/Pinecone friction.**

Alternative user-facing messages:

> **Send your Minecraft setup as one file.**

> **Move your instances from Windows to Linux without rebuilding everything manually.**

> **A modern native Minecraft launcher that stays powerful.**

> **One launcher. Your packs, worlds and settings. Easy to move.**

The website should demonstrate features instead of listing fifty technical terms above the fold.

Suggested landing page order:

1. short product statement;
2. screenshot/video;
3. Download;
4. `.pxlconep` sharing demo;
5. migration demo;
6. modern Library UI;
7. mod/content management;
8. performance/native architecture;
9. open-source/GitHub;
10. download formats.

---

## 53. TikTok / short-form promotion

Do not start by making every video look like a corporate advertisement.

TikTok's own business guidance emphasizes:

- hook quickly;
- show the product early;
- demonstrate a relatable problem and solution;
- make content feel native to the platform;
- use clear captions;
- test different creative rather than repeating one ad forever.

### Best content themes for PXL Cone

#### 1. Problem -> fix

Hook:

> “Почему чтобы перенести Minecraft с Windows на Fedora мне надо копировать полдиска?”

Then:

- show old manual folders;
- click Export;
- create `.pxlt`;
- import on Linux;
- show working instance.

#### 2. One-file sharing

Hook:

> “Я сделал формат для Minecraft-сборок, который можно просто кинуть другу в Telegram.”

Then show:

```text
CreatePack.pxlconep
```

send -> open -> Install.

#### 3. Before / after UI

Show:

- classic Pinecone/Prism workflow;
- PXL Cone Library;
- same advanced functionality still available.

Do not attack upstream developers. The message is usability preference, not “upstream is garbage”.

#### 4. Build in public

Short clips:

- “Сегодня добавил Linux package builds”;
- “Вот почему мой архив не копирует 800 MB модов, если их можно скачать по точной версии”;
- “Как я делаю перенос Windows -> Linux”;
- “Сломал сборку на Arch, вот почему”.

This can build interest before release.

#### 5. Real performance tests

After the app exists, record honest tests:

- launch time;
- idle RAM;
- install time;
- archive sizes.

Always show enough methodology that the numbers are meaningful.

---

## 54. Social account strategy

### Recommended initial structure

#### Pixel Service — primary project hub

Use it for:

- PXL Cone;
- PixelNet;
- Pixel City;
- future PXL software;
- release announcements;
- demos;
- technical/build-in-public clips.

This avoids maintaining several almost-empty accounts.

The profile should make the umbrella concept obvious:

> PXL / Pixel Service — apps, tools and open-source projects.

Use consistent visual branding, but give every project a recognizable sub-brand.

### Personal account — secondary/creator channel

Use when the post works better as:

> “Я сделал…”

rather than:

> “PXL Cone announces…”

Personal content can feel more natural for:

- development stories;
- mistakes;
- experiments;
- opinions about UX;
- before/after demos;
- project progress.

Then point interested viewers to:

- Pixel Service;
- project website;
- GitHub.

### Dedicated PXL Cone account

Do **not** create one immediately unless there is enough content to keep it alive.

Create a dedicated account later if PXL Cone develops:

- meaningful independent audience;
- enough release/tutorial/support content;
- consistent views/search interest;
- community submissions;
- frequent updates.

The goal is to avoid splitting a small audience three ways before the product has traction.

---

## 55. PXLNET cross-promotion inside PXL Cone

Cross-promotion is acceptable if it stays unobtrusive.

Suggested implementation:

### First run

One optional card shown once:

> From the PXL ecosystem: PXLNET

Buttons:

- Learn more;
- Not now;
- Don't show project recommendations.

### About / Other PXL projects

Permanent small section:

```text
Other PXL projects
- PXLNET
- Pixel City
```

### Do not

- show a full-screen ad on every launch;
- delay Play;
- autoplay promotional media;
- constantly reopen dismissed promos;
- hide the fact that the recommendation is from the same developer;
- make core launcher features depend on clicking an ad.

Because PXL Cone is open source, promotional UI can always be removed by a fork anyway.

The better strategy is to make users **want** to check other PXL projects.

---

## 56. Project priority

PXL Cone is intentionally **Project #3**, not the current main project.

Priority:

```text
1. PixelNet
2. Pixel City
3. PXL Cone
```

Until PixelNet's major update/application work is finished and Pixel City reaches its intended release, PXL Cone should mainly remain:

- specification;
- research;
- design ideas;
- low-cost planning.

Avoid spending large Codex budgets on implementation prematurely.

This prevents a new exciting project from blocking already active products.

---

## 57. PXL Cone v1 release target

A good v1 does not need every long-term feature.

Required:

- Windows build;
- Linux builds;
- Microsoft/Ely.by functionality inherited from PineconeMC remains working;
- cats remain;
- modern Library/Home UI;
- modern instance details flow;
- no major feature regression from PineconeMC;
- Modrinth/CurseForge flows remain usable;
- `.pxlconep` Smart Pack;
- `.pxlconep` Offline Pack;
- basic `.pxlt` transfer;
- Universal Offline ZIP;
- `.mrpack` / existing standard exports;
- basic migration from Pinecone/Prism;
- RPM;
- DEB;
- Arch `.pkg.tar.zst`;
- Flatpak bundle;
- AppImage;
- Windows installer + portable archive;
- basic documentation;
- GitHub releases;
- automatic CI builds.

Can wait until later:

- macOS;
- AI Crash Doctor;
- cloud sync;
- PXL account system;
- team/server SaaS;
- deduplicated content store;
- every launcher importer;
- official distro repositories;
- custom COPR/PPA/repository infrastructure.

---

## 58. Version 3 changes

Version 3 adds:

- Windows and Linux defined as first-class launch platforms;
- one-codebase / multi-build strategy;
- direct `.rpm` for Fedora/RPM-family systems;
- direct `.deb` for Ubuntu/Debian-family systems;
- direct Arch `.pkg.tar.zst` package;
- Flatpak single-file bundle;
- AppImage;
- Windows installer and portable ZIP;
- macOS explicitly deferred;
- no requirement for COPR, PPA, AUR or a custom package repository for launch;
- CI/release artifact matrix;
- complete product summary;
- switching reasons / differentiation;
- marketing positioning;
- TikTok/short-form content plan;
- Pixel Service umbrella-account strategy;
- personal account as secondary creator channel;
- optional PXLNET cross-promotion rules;
- project priority recorded as PixelNet -> Pixel City -> PXL Cone;
- realistic v1 release scope.

References for packaging details:

- Flatpak single-file bundles:
  https://docs.flatpak.org/en/latest/single-file-bundles.html
- Flatpak first build/share:
  https://docs.flatpak.org/en/latest/first-build.html
- Arch package creation:
  https://wiki.archlinux.org/title/Creating_packages
- TikTok creative guidance:
  https://ads.tiktok.com/business/en/guides/what-is-ad-creative-guide

---

## Appendix A — Version 2 changes

This spec revision adds and clarifies:

- `.pxlconep` as the dedicated **PXL Cone Pack** sharing format;
- Smart and Offline variants of `.pxlconep`;
- `.pxlt` remains the multi-instance backup/migration format;
- proper OS file associations for both PXL formats;
- a dedicated **Universal Offline ZIP**;
- explicit distinction between launcher-specific instance ZIPs and a normal Minecraft-content ZIP;
- friend-oriented Share/Export UX;
- manual/third-party launcher compatibility as a first-class goal;
- warning that a custom PXL format cannot automatically be understood by every launcher;
- conventional Minecraft folder layout for Universal Offline ZIP;
- worlds excluded by default from general pack sharing unless explicitly selected.

---

## Sources / references

- PineconeMC repository: https://github.com/ElyPrismLauncher/Launcher
- PineconeMC organization: https://github.com/ElyPrismLauncher
- PineconeMC releases: https://github.com/ElyPrismLauncher/Launcher/releases
- Halky Launcher: https://github.com/oleksandr1811/HalkyLauncher
- Modrinth App: https://modrinth.com/app
- XMCL: https://github.com/Voxelum/x-minecraft-launcher
- Refract: https://github.com/RefractMC/Refract_MC

---

## Status

**Idea:** worth prototyping.

The strongest differentiator is not merely “PineconeMC but prettier”.

It is:

> **PineconeMC power without PineconeMC/Prism UX friction, plus safe cross-PC migration and smart portable packs.**

That is enough to justify a real prototype.
