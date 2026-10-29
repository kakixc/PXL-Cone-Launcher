(() => {
const repository = 'kakixc/PXL-Cone-Launcher';
const releasesEndpoint = `https://api.github.com/repos/${repository}/releases?per_page=30`;

function detectPlatform({ userAgent = '', platform = '', mobile = false } = {}) {
  const identity = `${platform} ${userAgent}`;
  if (mobile || /Android|iPhone|iPad|iPod/i.test(identity)) return null;
  if (/Windows|Win32|Win64/i.test(identity)) return 'windows-setup';
  if (/Linux/i.test(identity) && !/CrOS/i.test(identity)) return 'linux';
  return null;
}

function classifyAsset(name) {
  if (typeof name !== 'string' || !/^PXL[-_. ]Cone[-_. ]/i.test(name)) return null;
  if (/arm64|aarch64|debug/i.test(name)) return null;
  if (/\.AppImage$/i.test(name)) return 'linux';
  if (/(setup|installer).*\.exe$/i.test(name)) return 'windows-setup';
  if (/windows.*portable.*\.zip$/i.test(name)) return 'windows-portable';
  return null;
}

function isDirectReleaseAsset(url) {
  try {
    const parsed = new URL(url);
    return parsed.protocol === 'https:' && parsed.hostname === 'github.com'
      && !parsed.username && !parsed.password
      && parsed.pathname.toLowerCase().startsWith(`/${repository.toLowerCase()}/releases/download/`);
  } catch { return false; }
}

function selectDownloads(releases) {
  if (!Array.isArray(releases)) throw new Error('Invalid release response');
  const published = releases.filter(release => release && !release.draft && release.published_at && Array.isArray(release.assets))
    .sort((a, b) => Date.parse(b.published_at) - Date.parse(a.published_at));
  for (const release of published) {
    const downloads = {};
    for (const asset of release.assets) {
      const format = classifyAsset(asset.name);
      if (!format || asset.state !== 'uploaded' || !isDirectReleaseAsset(asset.browser_download_url)) continue;
      // Prefer an explicitly x64-labelled file when a release has several builds.
      const candidate = { url: asset.browser_download_url, name: asset.name, version: String(release.tag_name || ''), size: asset.size };
      if (!downloads[format] || /x64|x86_64|amd64/i.test(asset.name)) downloads[format] = candidate;
    }
    if (Object.keys(downloads).length) return downloads;
  }
  return {};
}

async function loadDownloads(fetcher = fetch, signal) {
  const response = await fetcher(releasesEndpoint, { headers: { Accept: 'application/vnd.github+json' }, signal });
  if (!response.ok) throw new Error(`Release request failed: ${response.status}`);
  return selectDownloads(await response.json());
}

const api = { repository, releasesEndpoint, detectPlatform, classifyAsset, isDirectReleaseAsset, selectDownloads, loadDownloads };
if (typeof module !== 'undefined' && module.exports) module.exports = api;
else globalThis.PxlDownloads = api;
})();
