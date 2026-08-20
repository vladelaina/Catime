import { createSecureRandomOrder } from '../../tray/scripts/secure-random-order.js';
import {
    createPreviewLoader,
    loadPreviewImage,
    resolveMotionPolicy,
} from '../../tray/scripts/adaptive-image-loading.js';
import {
    fetchVerifiedPlugin,
    loadCachedCatalog,
    loadPluginCatalog,
} from './plugin-data.js';

const language = getLanguage();
const copy = {
    zh: {
        title: 'Catime - 插件库',
        meta: '浏览并下载经过校验的 Catime 插件脚本。',
        loading: '正在加载插件目录…',
        emptyTitle: '暂时没有插件',
        emptyDescription: '请稍后再来看看。',
        errorTitle: '暂时无法读取插件目录',
        errorDescription: '请稍后刷新页面重试。',
        retry: '重新加载',
        download: '下载插件',
        downloading: '正在下载并校验插件…',
        downloaded: '插件已通过校验并开始下载。',
        failed: '插件下载失败，文件未保存。',
    },
    en: {
        title: 'Catime - Plugin Library',
        meta: 'Browse and download verified plugin scripts for Catime.',
        loading: 'Loading plugin catalog…',
        emptyTitle: 'No plugins yet',
        emptyDescription: 'Please check back later.',
        errorTitle: 'Plugin catalog is unavailable',
        errorDescription: 'Refresh the page and try again later.',
        retry: 'Reload',
        download: 'Download plugin',
        downloading: 'Downloading and verifying plugin…',
        downloaded: 'Plugin passed verification and is downloading.',
        failed: 'Plugin could not be downloaded. No file was saved.',
    },
}[language];

const orderPlugins = createSecureRandomOrder(plugin => plugin.id);
const networkConnection = navigator.connection || navigator.mozConnection || navigator.webkitConnection;
const previewPolicy = resolveMotionPolicy({
    reducedMotion: window.matchMedia?.('(prefers-reduced-motion: reduce)').matches,
    saveData: networkConnection?.saveData,
    effectiveType: networkConnection?.effectiveType,
});
const previewLoader = createPreviewLoader({
    concurrency: previewPolicy.concurrency,
    loadPreview: loadPreviewImage,
});
const state = { catalog: null };
const elements = {};
let toastTimer = null;
let previewObserver = null;

document.addEventListener('DOMContentLoaded', () => {
    Object.assign(elements, {
        grid: document.getElementById('pluginGrid'),
        loading: document.getElementById('pluginLoading'),
        empty: document.getElementById('pluginEmpty'),
        error: document.getElementById('pluginError'),
        retry: document.getElementById('pluginRetry'),
        toast: document.getElementById('pluginToast'),
    });

    localizePage();
    elements.retry.addEventListener('click', refreshCatalog);

    const cached = loadCachedCatalog();
    if (cached) applyCatalog(cached);
    refreshCatalog();
});

function getLanguage() {
    const locale = window.CatimeLocale?.getLanguage?.();
    if (locale === 'zh' || locale === 'en') return locale;
    try {
        const saved = localStorage.getItem('catime-language');
        if (saved === 'zh' || saved === 'en') return saved;
    } catch {
    }
    const browserLanguage = navigator.languages?.[0] || navigator.language || 'en';
    return /^zh\b/i.test(browserLanguage) ? 'zh' : 'en';
}

function localizePage() {
    document.documentElement.lang = language === 'zh' ? 'zh-CN' : 'en';
    document.title = copy.title;
    document.querySelector('meta[name="description"]')?.setAttribute('content', copy.meta);
    setText('pluginLoadingText', copy.loading);
    setText('pluginEmptyTitle', copy.emptyTitle);
    setText('pluginEmptyDescription', copy.emptyDescription);
    setText('pluginErrorTitle', copy.errorTitle);
    setText('pluginErrorDescription', copy.errorDescription);
    setText('pluginRetryText', copy.retry);
}

async function refreshCatalog() {
    elements.retry.disabled = true;
    if (!state.catalog) setLoading(true);
    try {
        applyCatalog(await loadPluginCatalog());
    } catch (error) {
        console.error('Unable to load plugin catalog.', error);
        if (!state.catalog) showFatalError();
    } finally {
        elements.retry.disabled = false;
    }
}

function applyCatalog(catalog) {
    if (state.catalog?.source.commit === catalog.source.commit) {
        state.catalog = catalog;
        elements.error.hidden = true;
        setLoading(false);
        return;
    }
    state.catalog = catalog;
    elements.error.hidden = true;
    setLoading(false);
    renderCatalog();
}

function renderCatalog() {
    if (!state.catalog) return;
    const plugins = orderPlugins(state.catalog.plugins);
    previewObserver?.disconnect();
    previewObserver = 'IntersectionObserver' in window
        ? new IntersectionObserver(loadPreview, { rootMargin: '240px 0px' })
        : null;
    const tiles = plugins.map(plugin => createPluginTile(plugin));
    elements.grid.replaceChildren(...tiles);
    tiles.slice(0, 6).forEach(tile => requestPoster(tile.querySelector('img'), true));
    elements.grid.setAttribute('aria-busy', 'false');
    elements.grid.hidden = plugins.length === 0;
    elements.empty.hidden = plugins.length !== 0;
}

function createPluginTile(plugin) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'plugin-card';
    button.dataset.pluginId = plugin.id;
    button.setAttribute('aria-label', copy.download);
    button.addEventListener('click', () => downloadPlugin(plugin, button));

    const image = document.createElement('img');
    image.dataset.posterUrl = plugin.posterUrl;
    image.dataset.previewUrl = plugin.previewUrl;
    image.alt = '';
    image.loading = 'lazy';
    image.decoding = 'async';
    image.fetchPriority = 'low';
    image.referrerPolicy = 'no-referrer';
    image.addEventListener('load', () => {
        button.classList.add('is-loaded');
        if (image.dataset.posterLoaded !== 'true') {
            image.dataset.posterLoaded = 'true';
            const motionPending = image.dataset.motionPending === 'true';
            if (previewPolicy.automatic || motionPending) requestMotion(image, motionPending);
        }
    });
    image.addEventListener('error', () => {
        if (image.dataset.showingMotion === 'true') {
            image.dataset.showingMotion = 'false';
            image.src = image.dataset.posterUrl;
            return;
        }
        button.classList.add('is-missing');
    });
    if (previewPolicy.manual) {
        button.addEventListener('mouseenter', () => requestMotion(image, true));
        button.addEventListener('focus', () => requestMotion(image, true));
    }

    const placeholder = document.createElement('span');
    placeholder.className = 'plugin-preview-placeholder';
    placeholder.setAttribute('aria-hidden', 'true');
    placeholder.innerHTML = '<i class="fas fa-puzzle-piece"></i>';
    button.append(image, placeholder);
    if (previewObserver) previewObserver.observe(image);
    else requestPoster(image);
    return button;
}

function loadPreview(entries, observer) {
    for (const entry of entries) {
        if (!entry.isIntersecting) continue;
        requestPoster(entry.target, true);
        observer.unobserve(entry.target);
    }
}

function requestPoster(image, priority = false) {
    const url = image?.dataset.posterUrl;
    if (!url || image.dataset.requested === 'true') return;
    image.dataset.requested = 'true';
    image.dataset.previewPriority = String(priority);
    image.loading = priority ? 'eager' : 'lazy';
    image.fetchPriority = priority ? 'high' : 'low';
    image.src = url;
}

function requestMotion(image, priority = false) {
    const url = image?.dataset.previewUrl;
    if (!url || url === image.dataset.posterUrl || image.dataset.motionRequested === 'true') return;
    if (image.dataset.posterLoaded !== 'true') {
        image.dataset.motionPending = 'true';
        return;
    }
    image.dataset.motionPending = 'false';
    image.dataset.motionRequested = 'true';
    const highPriority = priority || image.dataset.previewPriority === 'true';
    previewLoader.request(url, { priority: highPriority }).then(loaded => {
        if (!loaded) {
            image.dataset.motionRequested = 'false';
            return;
        }
        if (!image.isConnected || image.dataset.previewUrl !== url) return;
        image.dataset.showingMotion = 'true';
        image.src = url;
    });
}

async function downloadPlugin(plugin, button) {
    if (button.disabled) return;
    button.disabled = true;
    button.classList.add('is-downloading');
    button.setAttribute('aria-busy', 'true');
    showToast(copy.downloading, false);

    try {
        const blob = await fetchVerifiedPlugin(plugin);
        triggerDownload(blob, plugin.filename);
        showToast(copy.downloaded);
    } catch (error) {
        console.error(`Unable to download ${plugin.id}.`, error);
        showToast(copy.failed);
    } finally {
        button.disabled = false;
        button.classList.remove('is-downloading');
        button.removeAttribute('aria-busy');
    }
}

export function triggerDownload(blob, filename) {
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = filename;
    anchor.hidden = true;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function showToast(message, autoHide = true) {
    clearTimeout(toastTimer);
    elements.toast.textContent = message;
    elements.toast.classList.toggle('is-pending', !autoHide);
    elements.toast.hidden = false;
    if (autoHide) toastTimer = setTimeout(() => { elements.toast.hidden = true; }, 4200);
}

function showFatalError() {
    setLoading(false);
    elements.grid.hidden = true;
    elements.empty.hidden = true;
    elements.error.hidden = false;
}

function setLoading(loading) {
    elements.grid.hidden = false;
    elements.grid.setAttribute('aria-busy', String(loading));
    if (loading) {
        elements.grid.replaceChildren(elements.loading);
        elements.loading.hidden = false;
    } else {
        elements.loading.hidden = true;
    }
}

function setText(id, value) {
    const element = document.getElementById(id);
    if (element) element.textContent = value;
}

window.CatimePlugins = {
    get catalog() { return state.catalog; },
    refreshCatalog,
    triggerDownload,
};
