(() => {
const { detectPlatform, loadDownloads } = globalThis.PxlDownloads;

const copy = {
  "ru": {
    "title": "PXL Cone — Minecraft. По-вашему.",
    "skip": "К содержимому",
    "navigation": "Навигация",
    "features": "Внутри лаунчера",
    "downloadNav": "Скачать",
    "hero1": "Minecraft.",
    "hero2": "По-вашему.",
    "heroBody": "Сборки, моды и любимые миры в одном лаунчере. С вашим оформлением и без лишних окон.",
    "otherPlatforms": "Другая система или Portable",
    "heroCaption": "Главная и библиотека PXL Cone",
    "heroDetail": "Свои обложки. Три вида библиотеки. Быстрый запуск.",
    "contentEyebrow": "Дополнения",
    "contentTitle": "Моды — рядом с игрой.",
    "contentBody": "Ищите моды, ресурспаки и шейдеры для выбранной сборки. Перед установкой — понятный список дополнений и зависимостей.",
    "contentNote": "Доступность зависит от версии Minecraft и загрузчика.",
    "shareEyebrow": "Игра с друзьями",
    "shareTitle": "Собрали для себя. Отправили другу.",
    "shareBody": "Не нужно заново собирать список модов. Выберите сборку, сохраните архив и передайте его другу — или перенесите своё окружение на другой компьютер.",
    "shareNote": "Миры и скриншоты включаются только по вашему выбору.",
    "downloadEyebrow": "PXL Cone Launcher",
    "downloadTitle": "Начните со своей сборки.",
    "alphaNote": "Первая публичная альфа ещё готовится. Здесь появятся загрузки для Windows и Linux. Перед тестированием сохраните копии важных миров.",
    "alphaAvailable": "Публичная тестовая версия. В ней ещё могут быть ошибки — перед тестированием сохраните копии важных миров.",
    "startNote": "При первом запуске лаунчер поможет выбрать язык, добавить аккаунт и настроить Java.",
    "platformGroup": "Формат загрузки",
    "retry": "Попробовать ещё раз",
    "source": "Исходный код",
    "feedback": "Сообщить об ошибке",
    "attribution": "На основе PineconeMC / Prism Launcher. Бесплатный проект с открытым кодом. Не связан с Mojang или Microsoft.",
    "fontLicense": "Лицензия шрифта",
    "iconLicense": "Иконки",
    "homeAlt": "Главная PXL Cone: быстрый запуск и библиотека сборок",
    "contentAlt": "Каталог дополнений PXL Cone",
    "shareAlt": "Выбор сборок для отправки другу",
    "loading": "Проверяем загрузки…",
    "loadingStatus": "Проверяем доступные версии…",
    "choose": "Выберите систему",
    "chooseStatus": "Лаунчер для компьютера. Выберите Windows или Linux в разделе загрузок.",
    "preparing": "Альфа готовится",
    "unavailable": "Подходящий файл ещё не опубликован. Он появится здесь вместе с публичной альфой.",
    "failed": "Загрузки недоступны",
    "failedStatus": "Не удалось проверить файлы. Проверьте подключение и попробуйте ещё раз.",
    "windows-setup": "Скачать для Windows",
    "windows-portable": "Скачать Portable",
    "linux": "Скачать для Linux",
    "note-windows-setup": "Windows 64-bit · установщик",
    "note-windows-portable": "Windows 64-bit · ZIP без установки",
    "note-linux": "Linux 64-bit · AppImage",
    "releaseStatus": "Версия {version} · прямая загрузка файла"
  },
  "en": {
    "title": "PXL Cone — Minecraft. Your way.",
    "skip": "Skip to content",
    "navigation": "Navigation",
    "features": "Inside the launcher",
    "downloadNav": "Download",
    "hero1": "Minecraft.",
    "hero2": "Your way.",
    "heroBody": "Your instances, mods and favourite worlds in one launcher. Make it yours, without a maze of windows.",
    "otherPlatforms": "Other systems and Portable",
    "heroCaption": "PXL Cone Home and Library",
    "heroDetail": "Custom covers. Three library layouts. Quick launch.",
    "contentEyebrow": "Content",
    "contentTitle": "Mods, right next to your game.",
    "contentBody": "Find mods, resource packs and shaders for your instance. Review the content and its dependencies before you install.",
    "contentNote": "Availability depends on your Minecraft version and loader.",
    "shareEyebrow": "Playing together",
    "shareTitle": "Built for you. Shared with a friend.",
    "shareBody": "No need to rebuild a mod list. Choose an instance, save an archive and send it to a friend — or take your setup to another computer.",
    "shareNote": "Worlds and screenshots are included only when you choose.",
    "downloadEyebrow": "PXL Cone Launcher",
    "downloadTitle": "Start with your own instance.",
    "alphaNote": "The first public alpha is still in development. Downloads for Windows and Linux will appear here. Back up important worlds before testing.",
    "alphaAvailable": "A public test build. It may still have bugs, so back up important worlds before testing.",
    "startNote": "First-run setup helps you choose a language, add an account and configure Java.",
    "platformGroup": "Download format",
    "retry": "Try again",
    "source": "Source code",
    "feedback": "Report a problem",
    "attribution": "Built on PineconeMC / Prism Launcher. Free and open source. Not affiliated with Mojang or Microsoft.",
    "fontLicense": "Font licence",
    "iconLicense": "Icons",
    "homeAlt": "PXL Cone Home with quick launch and instance library",
    "contentAlt": "PXL Cone content browser",
    "shareAlt": "Choose instances to share with a friend",
    "loading": "Checking downloads…",
    "loadingStatus": "Checking available versions…",
    "choose": "Choose your system",
    "chooseStatus": "A launcher for your computer. Choose Windows or Linux in the downloads section.",
    "preparing": "Alpha coming soon",
    "unavailable": "A matching file has not been published yet. It will appear here with the public alpha.",
    "failed": "Downloads unavailable",
    "failedStatus": "Could not check download files. Check your connection and try again.",
    "windows-setup": "Download for Windows",
    "windows-portable": "Download Portable",
    "linux": "Download for Linux",
    "note-windows-setup": "Windows 64-bit · installer",
    "note-windows-portable": "Windows 64-bit · ZIP, no installation",
    "note-linux": "Linux 64-bit · AppImage",
    "releaseStatus": "Version {version} · direct file download"
  }
};

let language = navigator.language?.toLowerCase().startsWith('ru') ? 'ru' : 'en';
try { const saved = localStorage.getItem('pxl-site-language'); if (copy[saved]) language = saved; } catch { /* Storage is optional. */ }
let selected = detectPlatform({ userAgent: navigator.userAgent, platform: navigator.userAgentData?.platform || navigator.platform, mobile: navigator.userAgentData?.mobile });
let downloads = {};
let status = 'loading';
let request;

function renderDownloads() {
  const text = copy[language];
  const asset = selected && downloads[selected];
  let label = text.loading;
  let message = text.loadingStatus;
  if (status === 'error') { label = text.failed; message = text.failedStatus; }
  else if (!selected) { label = text.choose; message = text.chooseStatus; }
  else if (status === 'ready') {
    label = asset ? text[selected] : text.preparing;
    message = asset ? text.releaseStatus.replace('{version}', asset.version) : text.unavailable;
  }
  for (const link of document.querySelectorAll('.download-link')) {
    link.querySelector('.download-label').textContent = label;
    const enabled = status === 'ready' && !!asset;
    link.setAttribute('aria-disabled', String(!enabled));
    link.tabIndex = enabled ? 0 : -1;
    if (enabled) link.href = asset.url;
    else link.removeAttribute('href');
  }
  for (const element of document.querySelectorAll('.download-status')) element.textContent = message;
  for (const button of document.querySelectorAll('[data-platform]')) button.setAttribute('aria-pressed', String(button.dataset.platform === selected));
  document.querySelector('.format-note').textContent = selected ? text[`note-${selected}`] : '';
  document.querySelector('#retry').hidden = status !== 'error';
  document.querySelector('[data-i18n="alphaNote"]').textContent =
    status === 'ready' && Object.keys(downloads).length ? text.alphaAvailable : text.alphaNote;
}

function renderLanguage() {
  const text = copy[language];
  document.documentElement.lang = language;
  document.title = text.title;
  for (const element of document.querySelectorAll('[data-i18n]')) element.textContent = text[element.dataset.i18n];
  for (const element of document.querySelectorAll('[data-i18n-alt]')) element.alt = text[element.dataset.i18nAlt];
  for (const element of document.querySelectorAll('[data-i18n-aria]')) element.setAttribute('aria-label', text[element.dataset.i18nAria]);
  const toggle = document.querySelector('#language');
  toggle.textContent = language === 'ru' ? 'EN' : 'RU';
  toggle.setAttribute('aria-label', language === 'ru' ? 'Switch to English' : 'Переключить на русский');
  renderDownloads();
}

async function refreshDownloads() {
  request?.abort();
  const controller = new AbortController();
  request = controller;
  status = 'loading';
  renderDownloads();
  const timeout = setTimeout(() => controller.abort(), 12000);
  try {
    const result = await loadDownloads(fetch, controller.signal);
    if (request !== controller) return;
    downloads = result;
    status = 'ready';
  } catch {
    if (request !== controller) return;
    downloads = {};
    status = 'error';
  } finally {
    clearTimeout(timeout);
    if (request === controller) renderDownloads();
  }
}

document.querySelector('#language').addEventListener('click', () => {
  language = language === 'ru' ? 'en' : 'ru';
  try { localStorage.setItem('pxl-site-language', language); } catch { /* Private browsing may deny storage. */ }
  renderLanguage();
});
for (const button of document.querySelectorAll('[data-platform]')) button.addEventListener('click', () => {
  selected = button.dataset.platform;
  renderDownloads();
});
document.querySelector('#retry').addEventListener('click', refreshDownloads);
renderLanguage();
refreshDownloads();
})();
