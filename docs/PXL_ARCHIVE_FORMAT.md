# PXL archives, version 1

PXL Cone exports a single Minecraft client instance as `.pxlconep`, or one or more instances as `.pxlt`. Both are ZIP-compatible archives with a UTF-8 `manifest.json` at the root. Imported archives create new instances; they do not overwrite an existing instance or import account credentials and custom launch commands.

## Storage modes

For `.pxlconep`, `mode` is `smart` or `offline`. For `.pxlt`, it is `thin` or `offline`.

Smart and thin exports reference exact known provider files when existing resource metadata supplies an exact project/version, trusted HTTPS download URL and matching hash. Local, modified or unresolved files are bundled. If a provider does not permit a direct download, the original file is bundled rather than silently omitted.

Offline exports bundle the selected instance content. This does not bundle the launcher, Java, shared Minecraft assets or every loader installation artifact. A receiving computer may still need network access to provision Minecraft, its loader and Java.

## Manifest

```json
{
  "formatVersion": 1,
  "format": "pxlconep",
  "mode": "smart",
  "instances": [{
    "name": "Example pack",
    "group": "Favorites",
    "components": {
      "formatVersion": 1,
      "components": [{"uid": "net.minecraft", "version": "1.21.1"}]
    },
    "settings": {},
    "metadata": {},
    "files": [{
      "path": "minecraft/config/example.json",
      "sha256": "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
      "size": 2,
      "source": "local",
      "projectId": "",
      "versionId": "",
      "url": "",
      "side": "unknown",
      "bundled": true
    }]
  }]
}
```

The example content bytes are `{}`. The bundled entry is `instances/0/minecraft/config/example.json`.

`format` must be `pxlconep` or `pxlt`. Packs contain exactly one instance. Transfers contain up to 1,000 instances. Instance array positions determine payload directories: `instances/0/`, `instances/1/`, and so on. These are archive-local indices, not destination instance IDs.

`components` retains the existing Prism/Pinecone component profile, including exact Minecraft and loader versions. `settings` contains only explicitly portable instance preferences, such as memory/window settings, play history and managed-pack identity. Machine-specific Java paths, account secrets and pre/post/wrapper launch commands are excluded. Unknown setting keys never become executable launcher settings.

`metadata` is optional Cone workspace data, preserved as `pxl-workspace.json` in the imported instance. Merely importing metadata does not start a server or execute actions. Nonempty top-level `requires` is rejected because this implementation does not provide additional required capabilities.

## Content records

Every content file has its exact uncompressed `size` and lowercase hexadecimal SHA-256 checksum. `path` is relative to the restored instance, using `minecraft/`, `patches/`, `jarmods/`, or `icon.png`. Original `minecraft` and `.minecraft` source layouts are normalized to `minecraft` during restoration. A custom icon is registered under a fresh launcher icon key on a successful import.

`side` is `client`, `server`, `both`, or `unknown`; it records provider metadata and does not imply that all content was independently compatibility-tested. This version restores client instances and does not create server processes.

For bundled files, the archive must contain `instances/<index>/<path>`. For references, `bundled` is false, `source` is `modrinth` or `curseforge`, and `projectId`, `versionId` and `url` must identify the exact recorded content. Downloads use the launcher's network jobs, are size-limited and SHA-256 verified. Unavailable or mismatched content fails the import; there is no automatic version substitution.

Worlds and screenshots are separate export choices. General pack sharing excludes both by default. Launcher-directory imports copy available worlds and screenshots, report their presence in the preview, and leave the originals unchanged. Logs, crash reports and known account/cache files are excluded.

## Validation and failure behavior

Unknown format versions, unsafe paths, link entries, duplicate/case-colliding paths, file/directory collisions, missing payloads, undeclared files and checksum mismatches are rejected. Paths cannot contain traversal, drive prefixes, backslashes, Windows device names or reserved characters. Names are validated for Windows/Linux portability.

Limits are a 16 MiB manifest, 32 GiB per content file, 256 GiB total declared content, and 500,000 archive entries/content records. Content is streamed rather than loaded into memory. These are rejection limits, not expected normal pack sizes.

Extraction and downloads occur in the existing launcher's staging area. Cancellation waits for file operations to stop before staging cleanup. Only successfully restored instances are published. A multi-instance transfer reports success or failure for each selected instance. Exports use a temporary archive, verify bundled content, and atomically replace the chosen output only after success.

Existing `.mrpack`, CurseForge and launcher ZIP workflows remain available. This format is not a guarantee that other launchers understand PXL workspace metadata.
