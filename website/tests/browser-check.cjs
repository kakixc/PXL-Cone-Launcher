const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const http = require('node:http');

(async () => {
  const out = path.resolve('build-pxl-site-qa');
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch({ executablePath: process.env.PXL_CHROME_PATH || undefined, headless: true });
  const siteRoot = path.resolve('build-pxl-site');
  const types = { '.html': 'text/html', '.css': 'text/css', '.js': 'application/javascript', '.svg': 'image/svg+xml', '.png': 'image/png', '.ttf': 'font/ttf' };
  const server = http.createServer((req, res) => {
    const relative = decodeURIComponent(new URL(req.url, 'http://local').pathname).replace(/^\/PXL-Cone-Launcher\//, '').replace(/^\//, '') || 'index.html';
    const file = path.resolve(siteRoot, relative);
    if (!file.startsWith(siteRoot + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) { res.writeHead(404); res.end(); return; }
    res.setHeader('Content-Type', types[path.extname(file)] || 'application/octet-stream');
    res.end(fs.readFileSync(file));
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const baseUrl = `http://127.0.0.1:${server.address().port}/PXL-Cone-Launcher/`;
  const findings = [];
  const asset = name => ({ name, state: 'uploaded', browser_download_url: `https://github.com/kakixc/PXL-Cone-Launcher/releases/download/v0.1-test/${name}` });
  const releases = [{ draft: false, prerelease: true, tag_name: 'v0.1-test', published_at: '2026-10-29T10:00:00Z', assets: [asset('PXL-Cone-Setup-x64.exe'), asset('PXL-Cone-Windows-Portable-x64.zip'), asset('PXL-Cone-Linux-x86_64.AppImage')] }];

  for (const viewport of [{ width: 1440, height: 960 }, { width: 1024, height: 768 }, { width: 390, height: 844 }, { width: 320, height: 800 }]) {
    const context = await browser.newContext({ viewport, locale: 'ru-RU' });
    await context.route('https://api.github.com/**', route => route.fulfill({ json: [] }));
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(baseUrl, { waitUntil: 'networkidle' });
    await page.evaluate(() => document.fonts.ready);
    await page.locator('img').evaluateAll(images => Promise.all(images.map(image => { image.loading = 'eager'; return image.decode().catch(() => {}); })));
    await page.locator('.download-label').first().waitFor();
    const broken = await page.locator('img').evaluateAll(images => images.filter(image => !image.complete || !image.naturalWidth).map(image => image.src));
    assert.deepEqual(broken, []);
    assert.deepEqual(errors, []);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false,
      JSON.stringify({ viewport, overflow: await page.locator('body *').evaluateAll(elements => elements.filter(element => element.getBoundingClientRect().right > innerWidth + 1).map(element => ({ tag: element.tagName, class: element.className, width: element.getBoundingClientRect().width, right: element.getBoundingClientRect().right }))) }));
    assert.equal((await page.locator('body').innerText()).includes('undefined'), false);
    assert.equal(await page.locator('.download-link').first().getAttribute('href'), null);
    await page.screenshot({ path: path.join(out, `${viewport.width}-full.png`), fullPage: true });
    await page.screenshot({ path: path.join(out, `${viewport.width}-first.png`) });
    await page.getByRole('button', { name: 'Switch to English' }).click();
    assert.equal(await page.locator('html').getAttribute('lang'), 'en');
    assert.equal(await page.locator('h1').innerText(), 'Minecraft.\nYour way.');
    assert.equal((await page.locator('body').innerText()).includes('undefined'), false);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
    await page.screenshot({ path: path.join(out, `${viewport.width}-english-first.png`) });
    await page.emulateMedia({ reducedMotion: 'reduce' });
    assert.equal(await page.locator('.hero-product').evaluate(element => getComputedStyle(element).animationName), 'none');
    findings.push(`Layout ${viewport.width}: images, no horizontal overflow, honest empty state, RU/EN, reduced motion passed`);
    await context.close();
  }

  for (const platform of ['Windows', 'Linux']) {
    const context = await browser.newContext({ viewport: { width: 1440, height: 960 } });
    await context.addInitScript(platform => { Object.defineProperty(navigator, 'platform', { get: () => platform }); Object.defineProperty(navigator, 'userAgent', { get: () => platform }); Object.defineProperty(navigator, 'userAgentData', { get: () => ({ platform, mobile: false }) }); }, platform);
    await context.route('https://api.github.com/**', route => route.fulfill({ json: releases }));
    const page = await context.newPage();
    await page.goto(baseUrl, { waitUntil: 'networkidle' });
    assert.match(await page.locator('.download-link').first().getAttribute('href'), platform === 'Windows' ? /Setup-x64\.exe$/ : /x86_64\.AppImage$/);
    await page.locator('[data-platform="windows-portable"]').click();
    assert.match(await page.locator('.download-link').first().getAttribute('href'), /Portable-x64\.zip$/);
    findings.push(`${platform}: detected direct file and manual Portable passed`);
    await context.close();
  }

  const context = await browser.newContext({ locale: 'ru-RU' });
  await context.route('https://api.github.com/**', route => route.fulfill({ status: 403, json: {} }));
  const page = await context.newPage();
  await page.goto(baseUrl, { waitUntil: 'networkidle' });
  assert.equal(await page.locator('#retry').isVisible(), true);
  assert.equal(await page.locator('.download-link').first().getAttribute('href'), null);
  await context.unroute('https://api.github.com/**');
  await context.route('https://api.github.com/**', route => route.fulfill({ json: releases }));
  await page.locator('#retry').click();
  await page.locator('.download-link[href]').first().waitFor();
  findings.push('API error and retry recover a direct file link');

  await page.goto(pathToFileURL(path.resolve('website/index.html')).href, { waitUntil: 'networkidle' });
  await page.locator('img').evaluateAll(images => Promise.all(images.map(image => { image.loading = 'eager'; return image.decode().catch(() => {}); })));
  assert.deepEqual(await page.locator('img').evaluateAll(images => images.filter(image => !image.complete || !image.naturalWidth).map(image => image.src)), []);
  assert.equal(await page.locator('#language').isVisible(), true);
  await page.getByRole('button', { name: 'Switch to English' }).click();
  assert.equal(await page.locator('html').getAttribute('lang'), 'en');
  findings.push('Source index.html opened directly: all images and language switching work');
  await browser.close();
  server.close();
  fs.writeFileSync(path.join(out, 'results.txt'), findings.join('\n'));
  console.log(findings.join('\n'));
})().catch(error => { console.error(error); process.exit(1); });
