# PXL Cone Servers and `.pxlconep` Format

> Status: Draft specification  
> Target: PXL Cone Launcher  
> Format version: 1 (draft)

This document describes two connected parts of PXL Cone:

1. local Minecraft server creation and management;
2. the `.pxlconep` pack format used to share and recreate client/server setups.

The goal is to make a client instance, a server, and a shareable pack work as parts of one system.

---

## 1. Main idea

PXL Cone should be able to turn a Minecraft instance into a ready-to-run local server with minimal manual work.

A user should be able to:

- create a server from an existing instance;
- create a server from a `.pxlconep` file;
- create a server from a plain mod list;
- create an empty server from scratch;
- automatically exclude client-only mods from the server;
- keep required shared mods on both client and server;
- configure the server in a visual interface;
- start, stop and restart the server from PXL Cone;
- create a matching client pack for friends;
- share a server-compatible `.pxlconep` file;
- import a pack received from another person and create either a client, a server, or both.

PXL Cone is **not** a hosting provider. It manages local server files and processes. Exposing the server to the internet is outside the core responsibility of the launcher.

---

## 2. Servers section

The launcher should have a dedicated top-level section:

```text
Library | Servers | Discover | Downloads | Accounts | Settings
```

The `Servers` page should contain:

```text
My Servers

[ Create Server ]
[ Import Server ]

Server cards...
```

A server card may show:

- server name;
- Minecraft version;
- server type / loader;
- running state;
- local port;
- RAM allocation;
- player count when available;
- last launch time;
- linked client instance, if any.

---

## 3. Server creation sources

The create-server wizard should support at least these sources.

### 3.1 From current instance

The user selects an existing PXL Cone instance.

PXL Cone:

1. reads the Minecraft version and loader;
2. reads installed mods and content;
3. determines which mods belong on the server;
4. excludes known client-only mods;
5. adds required server/shared dependencies;
6. shows uncertain compatibility decisions to the user;
7. downloads the selected server implementation;
8. creates the server directory;
9. copies or downloads compatible content;
10. generates initial server settings.

Example:

```text
Create Survival
Minecraft 1.21.1
Fabric 0.16.14

Create              client + server
Farmer's Delight    client + server
Sodium              client only
Iris                client only
FerriteCore         client + server
```

### 3.2 From `.pxlconep`

If the pack contains server-compatible metadata, the user should be able to choose:

```text
Install Client
Create Server
Install Both
```

The launcher should only install content appropriate for the selected side.

### 3.3 From mod list

The user can paste or import a simple list:

```text
Create
Sodium
Farmer's Delight
Simple Voice Chat
JEI
```

PXL Cone asks for:

- Minecraft version;
- preferred loader;
- preferred source when several matches exist.

Then it resolves projects, versions and dependencies.

The launcher must not silently guess when several projects match the same name.

Example:

```text
"Create" matched multiple projects.
Choose the intended project before continuing.
```

Client-only projects may still be included in the generated client pack, but must not be copied to the server.

### 3.4 Empty server

The user chooses Minecraft version and server implementation, then configures everything manually.

---

## 4. Server type selection

The wizard should clearly separate modded and plugin-based server types.

### Modded

Examples:

- Fabric;
- NeoForge;
- Forge;
- Quilt, if supported.

### Plugin-based

Examples:

- Paper;
- Purpur;
- Vanilla.

The launcher must not imply that every server implementation supports both normal mods and normal plugins.

Hybrid server implementations, if ever supported, should be treated as a separate advanced category with explicit warnings.

---

## 5. Mod side handling

Every content item should have a side classification when known:

```text
client
server
both
unknown
```

### Rules

- `client`: install only on client;
- `server`: install only on server;
- `both`: install on both sides;
- `unknown`: never silently remove; ask the user or use a conservative fallback.

The launcher should use available metadata from Modrinth, CurseForge or other supported sources when possible.

A future local compatibility database may improve this classification, but source metadata should remain the preferred signal when trustworthy.

---

## 6. Uncertain compatibility

PXL Cone must not pretend to know compatibility when it does not.

Example:

```text
SomeMod
Server compatibility: Unknown

[ Keep ] [ Exclude ] [ View project ]
```

The user's decision may optionally be remembered for that exact project/version.

---

## 7. Server configuration UI

Common `server.properties` options should be editable through normal controls.

Example:

```text
Difficulty       [ Normal     v ]
Gamemode         [ Survival   v ]
PVP              [x]
Whitelist        [x]
Online mode      [x]
View distance    [12]
Simulation dist. [10]
Port             [25565]
MOTD             [My Server]
```

Advanced users should still be able to edit the raw configuration file.

Suggested server detail sections:

```text
Overview
Mods
Plugins
Players
World
Backups
Console
Logs
Settings
```

---

## 8. Server process controls

Basic controls:

```text
Start
Stop
Restart
Kill (advanced / fallback only)
```

The launcher should show:

- process state;
- current RAM usage when available;
- uptime;
- console output;
- exit code after shutdown;
- crash log link when applicable.

PXL Cone should prefer graceful shutdown before force-killing the Java process.

---

## 9. Sharing a server with friends

A server can generate a matching client pack.

Example flow:

```text
Server -> Share -> Create client pack
```

PXL Cone analyzes server content and generates:

### Required client content

Content that must exist on the client to join correctly.

### Recommended client content

Optional client-side additions such as performance or UI mods.

Example:

```text
Required
[x] Create
[x] Farmer's Delight
[x] Simple Voice Chat

Recommended
[x] Sodium
[x] Iris
[ ] Mod Menu
```

The result may be exported as `.pxlconep`.

---

# `.pxlconep` specification

## 10. Purpose

`.pxlconep` means **PXL Cone Pack**.

It is intended to describe a Minecraft setup in a way PXL Cone can recreate reliably.

The format should support at least:

- Minecraft version;
- loader and loader version;
- mods;
- resource packs;
- shader packs;
- optional content;
- client/server side metadata;
- exact source project/version identifiers;
- future server metadata;
- integrity hashes where useful.

The format must be versioned from the beginning.

---

## 11. Pack types

### `smart`

A small metadata-based pack.

The file contains exact project/version references. PXL Cone downloads required content during installation.

Advantages:

- small file size;
- easy sharing;
- no duplicated mod binaries;
- content can be verified by source IDs and hashes.

### `offline`

A self-contained pack that bundles selected files directly.

Useful when:

- the recipient has no internet connection;
- the source version is no longer downloadable;
- an exact local file must be preserved.

The exact container layout for offline packs is not finalized in this draft.

---

## 12. Draft JSON structure

A smart pack may look like this:

```json
{
  "formatVersion": 1,
  "type": "smart",
  "name": "Create Survival",

  "minecraft": {
    "version": "1.21.1",
    "loader": {
      "type": "fabric",
      "version": "0.16.14"
    }
  },

  "mods": [
    {
      "source": "modrinth",
      "projectId": "AANobbMI",
      "versionId": "EXACT_VERSION_ID",
      "side": "client",
      "optional": false
    },
    {
      "source": "curseforge",
      "projectId": "328085",
      "versionId": "EXACT_FILE_ID",
      "side": "both",
      "optional": false
    }
  ],

  "resourcePacks": [],
  "shaderPacks": []
}
```

`projectId` and `versionId` are preferred over display names or human-readable version strings.

---

## 13. Required top-level fields

### `formatVersion`

Integer format version.

```json
"formatVersion": 1
```

PXL Cone must reject unsupported future versions with a clear message instead of attempting a best-effort parse that may corrupt the setup.

### `type`

Current draft values:

```text
smart
offline
```

### `name`

Human-readable pack name.

### `minecraft`

Minecraft and loader metadata.

---

## 14. Minecraft object

Example:

```json
{
  "version": "1.21.1",
  "loader": {
    "type": "fabric",
    "version": "0.16.14"
  }
}
```

Possible loader values may include:

```text
vanilla
fabric
forge
neoforge
quilt
```

Loader values must be normalized and documented before the format is finalized.

---

## 15. Content object

Suggested fields:

```json
{
  "source": "modrinth",
  "projectId": "AANobbMI",
  "versionId": "EXACT_VERSION_ID",
  "side": "both",
  "optional": false,
  "sha256": "OPTIONAL_SHA256"
}
```

### `source`

Where PXL Cone should resolve the project.

Initial candidates:

```text
modrinth
curseforge
local
```

### `projectId`

Source-specific stable project identifier.

### `versionId`

Source-specific exact version/file identifier.

This should identify one exact downloadable version, not just a semantic version string.

### `side`

```text
client
server
both
unknown
```

### `optional`

If `true`, the installer may let the user enable or disable that item before installation.

### `sha256`

Optional integrity check for the expected downloaded file.

For offline packs, hashes should become more important because the pack may contain local bundled files.

---

## 16. Resource packs and shaders

Use separate arrays:

```json
"resourcePacks": [],
"shaderPacks": []
```

Items may use the same source/project/version structure where supported.

Side normally defaults to `client`, but the format should avoid assuming this in places where server-side resource handling exists.

---

## 17. Future server metadata

A future version may optionally include a server section.

Draft example:

```json
{
  "server": {
    "enabled": true,
    "type": "fabric",
    "minecraftVersion": "1.21.1",
    "recommendedRamMb": 4096,
    "properties": {
      "difficulty": "normal",
      "gamemode": "survival"
    }
  }
}
```

This section is **not finalized** for format version 1.

The pack should not automatically expose ports, configure routers or modify external networking settings.

---

## 18. Install behavior

When opening a `.pxlconep`, PXL Cone should show a summary before making changes.

Example:

```text
Create Survival
Minecraft 1.21.1
Fabric 0.16.14

43 mods
2 resource packs
1 shader pack

Client content: 38
Server content: 31
Optional: 4

[ Install Client ]
[ Create Server ]
[ Install Both ]
```

The user should be able to inspect the full content list before installation.

---

## 19. Dependency resolution

When a source API exposes dependencies, PXL Cone should resolve required dependencies automatically.

The installer should distinguish:

```text
Declared in pack
Added automatically as dependency
Optional dependency
Incompatible dependency
```

The resolved result should be shown before installation if additional content is being added.

---

## 20. Version compatibility

PXL Cone should verify that every selected content version is compatible with:

- selected Minecraft version;
- selected loader;
- selected side when metadata exists.

If an exact version is unavailable, the launcher must not silently substitute another version in a reproducible smart pack.

Instead:

```text
Required version is unavailable.

[ Choose replacement ]
[ Skip ]
[ Cancel installation ]
```

---

## 21. Security and safety

PXL Cone should treat imported packs as untrusted input.

The parser must not allow pack data to write arbitrary files outside the target instance/server directory.

Offline packs must protect against path traversal such as:

```text
../../some-file
```

The launcher should verify hashes when provided.

Executable files or scripts bundled by future offline formats should never be executed automatically just because a pack was opened.

---

## 22. Non-goals for v1

The first version does not need to solve everything.

Not required for the first implementation:

- automatic public internet exposure;
- port forwarding automation;
- built-in paid hosting;
- remote server administration;
- automatic DNS setup;
- hybrid mod/plugin server support;
- AI-based mod compatibility guessing;
- cloud synchronization.

These may be considered later as separate features.

---

## 23. Open questions

Before implementation, decide:

1. whether `side` belongs in every content item or may be inferred when omitted;
2. whether server metadata belongs in `.pxlconep` v1 or a later version;
3. how offline packs are physically packaged;
4. whether config files are referenced individually or stored as an archive section;
5. how optional mods are presented during installation;
6. how a pack references local-only content unavailable on Modrinth/CurseForge;
7. whether one pack can define multiple server profiles;
8. how world files should be handled, if at all;
9. how update behavior works after a pack is installed;
10. whether user compatibility overrides should remain local or be exportable.

---

## 24. Implementation principle

Do not build a second launcher engine for servers.

Reuse existing Pinecone/Prism concepts and download/content infrastructure wherever practical.

Before implementing this feature, an AI agent or developer should first analyze:

- current instance model;
- current import/export paths;
- Modrinth and CurseForge metadata handling;
- loader/version installation;
- Java/runtime launching;
- content dependency resolution.

Only then should the implementation points be selected.

##original message
[9/17/2026 9:40 AM] 𝐏𝐗𝐋: Вот это уже реально фича уровня “почему этого нет нормально в других лаунчерах”. Я бы вообще сделал серверы не маленькой кнопкой, а полноценной частью PXL Cone.
Отдельная вкладка Servers
Library | Servers | Discover | Downloads
В Servers:
My Servers Create Server Import Server Create from Instance Create from PXL Cone Pack Create from Mod List
И самый кайф именно в связи клиент ↔ сервер.
Допустим, у тебя есть сборка:
Create Sodium Iris Farmer's Delight JourneyMap FerriteCore
Ты нажимаешь:
Create server from this instance

Cone сам разбирает моды:
Client Server Create ✓ ✓ Farmer's Delight ✓ ✓ Sodium ✓ - Iris ✓ - JourneyMap ✓ ? / ✓ FerriteCore ✓ ✓
и сервер получает только то, что ему реально нужно.
Но важный момент: неизвестные моды лучше не удалять молча. Если Cone не уверен:

Unknown server compatibility: SomeMod
Keep / Exclude / Check manually
Иначе одна неправильная запись в базе и сборка внезапно не работает.
Создание сервера в один мастер
Я бы сделал прямо красивый wizard:
Create Server 1. Source ● Current instance ○ PXL Cone Pack ○ Mod list ○ Empty server 2. Minecraft 1.21.1 3. Server type Fabric NeoForge Forge Quilt Plugins: Paper Purpur Vanilla 4. Content 31 server mods 7 client-only mods excluded 4 dependencies added automatically 5. Settings RAM: 4 GB Port: 25565 Difficulty: Normal Online mode: On 6. Create
С плагинами надо аккуратно. Не делать вид, что любое ядро умеет одновременно любые моды и плагины. Если человек выбирает Fabric/NeoForge, это modded server. Если Paper/Purpur, это plugin server. Всякие hybrid-варианты можно потом добавлять отдельно с предупреждениями.
А Create from Mod List вообще имба
Пользователь просто вставляет:
Create Sodium Farmer's Delight Simple Voice Chat JEI
Cone спрашивает:
Minecraft version? 1.21.1 Preferred loader? NeoForge
Дальше:
Found: Create CurseForge / Modrinth Sodium Modrinth Farmer's Delight CurseForge / Modrinth Simple Voice Chat Modrinth JEI CurseForge Dependencies automatically added: 3 ⚠ Sodium is client-only. It will be added to the client pack but not the server.
И сам подбирает совместимые версии именно под выбранные Minecraft + loader.
Если есть два проекта с одинаковым/похожим названием, не угадывает:
Which "Create" did you mean? ● Create simibubi / CurseForge ○ Create Fabric ...
Вот это будет очень удобно.
Самое интересное: сервер и клиент должны быть связаны
Я бы не создавал ещё десяток форматов файлов.
Лучше расширить сам .pxlconep, чтобы он умел описывать одновременно клиент и сервер:
{ "formatVersion": 1, "name": "Create Survival", "minecraft": { "version": "1.21.1", "loader": "neoforge" }, "content": [ { "projectId": "create", "side": "both" }, { "projectId": "sodium", "side": "client" }, { "projectId": "ferrite-core", "side": "both" } ] }
И тогда один PXL Cone Pack знает обе стороны.
Из него можно нажать:
Install Client Create Server Install Both
Вот это охрененно.
А с уже готового сервера наоборот
Ты настроил сервер и нажимаешь
:
Share with friend
s
Cone смотрит серверные моды и собирает клиентский .pxlconep.
Причём может предложить:
Required client content ✓ Create ✓ Farmer's Delight ✓ Simple Voice Chat Recommended client mods ☑ Sodium ☑ Iris ☑ Mod Menu ☑ ImmediatelyFast
И другу ты отправляешь один файл.
Он открыва
ет:
This pack is designed for "My Create Server"
Install 43 mods?
Inst
all
И готово.
Вот это уже прям сильная экосистемная штука Cone.
А сама вкладка сервера потом может быть примерно:
My Create Server [ Start ] Status Offline Minecraft 1.21.1 Loader NeoForge RAM 4 GB Overview Mods Plugins Players World Backups Console Logs Settings
server.properties не обязательно заставлять человека открывать текстом. Сделать нормальные элементы:
Difficulty [ Normal ▼ ] Gamemode [ Survival ▼ ] PVP [✓] Whitelist [✓] Online mode [✓] View distance [12] Port [25565]
[9/17/2026 9:40 AM] 𝐏𝐗𝐋: А для тех, кому надо всё ру
ками:
Advanced → Edit server.prope
rties
Консоль прямо внутри Cone, backup мира перед обновлениями, лог ошибок, кнопки Start / Stop / Restart.
И важная граница: Cone не обязан быть хостингом.
Он
делает:
создание → установка → настройка → запуск → сборка клиента → экспорт/
перенос
А дальше пользователь сам решает:
port forwarding;
VPS;
Tailscale;
Radmin/Hamachi;
playit;
белый IP;
локальная сеть.
То есть ровно как ты сказал: «как вывести наружу, дальше разберёшься сам». Максимум можно дать справку.
И вот это я бы уже записал как одну из главных фич PXL Cone, наряду с .pxlconep и Migratio
n Center:
Turn any Minecraft instance into a ready-to-run server in one click, with automatic client/server mod se
paration.
Потому что это уже не просто «Prism, но красивее». Это реально отдельный сценарий, ради которого я бы сам мог поставить другой лаунчер.
[9/17/2026 9:42 AM] 𝐏𝐗𝐋: Нажимаешь на свой сборку 3 точки и запустить севрер с этой сборой
