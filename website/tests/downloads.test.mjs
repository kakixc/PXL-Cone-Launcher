import test from 'node:test';
import assert from 'node:assert/strict';
import api from '../downloads.js';
const { detectPlatform, classifyAsset, selectDownloads, loadDownloads, isDirectReleaseAsset } = api;

const asset = (name, overrides = {}) => ({ name, state: 'uploaded', browser_download_url: `https://github.com/kakixc/PXL-Cone-Launcher/releases/download/v0.1.0-alpha/${name}`, ...overrides });
const release = (assets, overrides = {}) => ({ draft: false, prerelease: true, published_at: '2026-10-29T10:00:00Z', tag_name: 'v0.1.0-alpha', assets, ...overrides });

test('Windows and Linux visitors get the matching desktop format', () => {
  assert.equal(detectPlatform({ userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)' }), 'windows-setup');
  assert.equal(detectPlatform({ userAgent: 'Mozilla/5.0 (X11; Linux x86_64)' }), 'linux');
  assert.equal(detectPlatform({ platform: 'Windows' }), 'windows-setup');
  assert.equal(detectPlatform({ platform: 'Linux' }), 'linux');
});

test('mobile, macOS and unknown systems require a manual desktop choice', () => {
  for (const userAgent of ['Linux; Android 15', 'iPhone', 'iPad', 'MacIntel', 'CrOS x86_64', 'X11; FreeBSD', '']) assert.equal(detectPlatform({ userAgent }), null);
  assert.equal(detectPlatform({ platform: 'Linux', mobile: true }), null);
});

test('only the agreed player formats are offered', () => {
  assert.equal(classifyAsset('PXL-Cone-Setup.exe'), 'windows-setup');
  assert.equal(classifyAsset('PXL-Cone-Windows-MSVC-Portable-0.1.zip'), 'windows-portable');
  assert.equal(classifyAsset('PXL-Cone-Linux-x86_64.AppImage'), 'linux');
  for (const name of ['PineconeMC-Setup.exe', 'PXL-Cone-Windows-ARM64-Setup.exe', 'PXL-Cone-Linux-aarch64.AppImage', 'PXL-Cone-Setup-Debug.exe', 'PXL-Cone-source.zip', 'PXL-Cone.dmg']) assert.equal(classifyAsset(name), null);
});

test('the public alpha prerelease resolves directly to uploaded files', () => {
  const result = selectDownloads([release([asset('PXL-Cone-Setup.exe'), asset('PXL-Cone-Linux-x86_64.AppImage')])]);
  assert.match(result['windows-setup'].url, /\/releases\/download\/.*\.exe$/);
  assert.match(result.linux.url, /\.AppImage$/);
  assert.equal(result['windows-setup'].version, 'v0.1.0-alpha');
});

test('drafts, incomplete assets and off-repository URLs are never downloads', () => {
  const result = selectDownloads([
    release([asset('PXL-Cone-Setup.exe')], { draft: true }),
    release([asset('PXL-Cone-Setup.exe', { state: 'new' }), asset('PXL-Cone-Linux.AppImage', { browser_download_url: 'https://example.com/file.AppImage' })]),
  ]);
  assert.deepEqual(result, {});
  assert.equal(isDirectReleaseAsset('javascript:alert(1)'), false);
  assert.equal(isDirectReleaseAsset('https://github.com/kakixc/PXL-Cone-Launcher/releases/latest'), false);
  assert.equal(isDirectReleaseAsset('https://github.com/other/project/releases/download/v1/PXL-Cone-Setup.exe'), false);
});

test('the newest published build wins without mixing older platform packages', () => {
  const older = release([asset('PXL-Cone-Setup.exe'), asset('PXL-Cone-Linux.AppImage')], { published_at: '2026-10-28T10:00:00Z', tag_name: 'older' });
  const newer = release([asset('PXL-Cone-Setup.exe')], { published_at: '2026-10-29T10:00:00Z', tag_name: 'newer' });
  const result = selectDownloads([older, newer]);
  assert.equal(result['windows-setup'].version, 'newer');
  assert.equal(result.linux, undefined);
});

test('HTTP failures and malformed responses remain errors, not fake links', async () => {
  await assert.rejects(loadDownloads(async () => ({ ok: false, status: 403 })), /403/);
  await assert.rejects(loadDownloads(async () => ({ ok: true, json: async () => ({}) })), /Invalid/);
  assert.deepEqual(await loadDownloads(async () => ({ ok: true, json: async () => [] })), {});
});
