import {
    animationDownloadFilename,
    animationPosterUrl,
    animationPreviewUrl,
    animationUrl,
    loadImmediateLibraryData,
    loadLibraryData,
} from './library-data.js';
import { createPreviewLoader, resolveMotionPolicy } from './adaptive-image-loading.js';
import { colorForIndex, escapeAttribute, escapeHtml } from './dom-utils.js';
import { createSecureRandomOrder } from './secure-random-order.js';

const INITIAL_VISIBLE_ANIMATIONS = 18;
const LOAD_MORE_SIZE = 24;
const FEATURED_ANIMATIONS = 5;
const orderAuthors = createSecureRandomOrder(author => author.name);
const networkConnection = navigator.connection || navigator.mozConnection || navigator.webkitConnection;
const motionPolicy = resolveMotionPolicy({
    reducedMotion: window.matchMedia?.('(prefers-reduced-motion: reduce)').matches,
    saveData: networkConnection?.saveData,
    effectiveType: networkConnection?.effectiveType,
});
const previewLoader = createPreviewLoader({
    concurrency: motionPolicy.concurrency,
    loadPreview: loadPreviewImage,
});
const language = getCurrentLanguage();
const copy = {
    zh: {
        pageTitle: 'Catime - 托盘动画库',
        metaDescription: '浏览、预览并下载适用于 Catime 的托盘动画资源。',
        noticeTitle: '作品与作者',
        noticeBody: '本页面展示的托盘动画均在与作者沟通确认后收录。Catime 仅负责整理与展示，作品版权及相关权益归原作者所有。<br>需要转载、修改或用于其他用途时，请先联系原作者并遵循作者说明。<br>每一帧、每一处像素，都凝聚着设计师悉心打磨的心血。若您喜爱这些作品，请务必点击作者头像前往其主页，关注并支持他们。',
        loading: '正在整理动画看板…',
        emptyTitle: '没有找到对应资源',
        emptyDescription: '请稍后再来看看。',
        loadErrorTitle: '动画资源加载失败',
        loadErrorDescription: '请稍后刷新页面重试。',
        artistTag: '动画作者',
        expand: '展开',
        collapse: '收起',
        animations: count => `${count.toLocaleString('zh-CN')} 个托盘动画`,
        loadMore: count => `加载更多（剩余 ${count.toLocaleString('zh-CN')} 个）`,
        openProfile: name => `打开 ${name} 的作者主页`,
        previewLabel: 'Windows 托盘动画效果预览',
        previewAlt: '托盘图标效果',
    },
    en: {
        pageTitle: 'Catime - Tray Animation Library',
        metaDescription: 'Browse, preview, and download tray animations for Catime.',
        noticeTitle: 'Works & Artists',
        noticeBody: 'The tray animations shown on this page are included after confirmation with their creators. Catime only organizes and showcases them; all copyrights and related rights remain with the original creators.<br>For redistribution, modification, or any other use, please contact the original creator first and follow their terms.<br>Every frame and every pixel reflects the care of its designer. If you enjoy these works, please click the creator\'s avatar to visit their profile, follow them, and show your support.',
        loading: 'Preparing the animation gallery…',
        emptyTitle: 'No resources found',
        emptyDescription: 'Please check back later.',
        loadErrorTitle: 'Unable to load animations',
        loadErrorDescription: 'Please refresh the page and try again later.',
        artistTag: 'Animation Artist',
        expand: 'Expand',
        collapse: 'Collapse',
        animations: count => `${count.toLocaleString('en-US')} tray animations`,
        loadMore: count => `Load more (${count.toLocaleString('en-US')} remaining)`,
        openProfile: name => `Open ${name}'s profile`,
        previewLabel: 'Windows tray animation preview',
        previewAlt: 'Tray icon preview',
    },
}[language];

const state = {
    collections: [],
    authors: [],
    expandedAuthor: null,
    visibleByCollection: new Map(),
};

const elements = {};
let automaticMotionStarted = false;
let previewObserver = null;
let renderGeneration = 0;
let trayPreviewSelection = null;

function getCurrentLanguage() {
    const saved = localStorage.getItem('catime-language');
    if (saved === 'en' || saved === 'zh') return saved;
    const browserLanguage = (navigator.languages && navigator.languages[0]) || navigator.language || 'zh-CN';
    return /^en\b/i.test(browserLanguage) ? 'en' : 'zh';
}

function localizePage() {
    document.documentElement.lang = language === 'en' ? 'en' : 'zh-CN';
    document.title = copy.pageTitle;
    document.querySelector('meta[name="description"]')?.setAttribute('content', copy.metaDescription);

    const title = document.getElementById('creatorNoteTitleText');
    const body = document.getElementById('creatorNoteBody');
    if (title) title.textContent = copy.noticeTitle;
    if (body) body.innerHTML = copy.noticeBody;

    document.querySelector('.board-loading p').textContent = copy.loading;
    document.querySelector('#boardEmpty h3').textContent = copy.emptyTitle;
    document.querySelector('#boardEmpty p').textContent = copy.emptyDescription;
    document.querySelector('.tray-simulator')?.setAttribute('aria-label', copy.previewLabel);
    document.getElementById('trayIconImage')?.setAttribute('alt', copy.previewAlt);

    if (window.CatimeUI?.translateNavLinks) {
        const translations = language === 'en'
            ? { linkTranslations: { '首页': 'Home', '指南': 'Guide', '关于': 'About', '插件': 'Plugins' }, spanTranslations: { '支持项目': 'Support', '下载': 'Download' } }
            : { linkTranslations: { Home: '首页', Guide: '指南', About: '关于', Plugins: '插件' }, spanTranslations: { Support: '支持项目', Download: '下载' } };
        window.CatimeUI.translateNavLinks(translations);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    Object.assign(elements, {
        board: document.getElementById('trayBoard'),
        empty: document.getElementById('boardEmpty'),
        trayIcon: document.getElementById('trayIconImage'),
        trayClock: document.getElementById('trayClock'),
        trayDate: document.getElementById('trayDate'),
    });

    localizePage();
    window.initAOSOnce?.();
    updateTrayClock();
    setInterval(updateTrayClock, 30000);
    const immediateLibrary = loadImmediateLibraryData();
    if (immediateLibrary?.collections.length) applyLibrary(immediateLibrary);
    loadLibrary();
});

async function loadLibrary() {
    try {
        const library = await loadLibraryData();
        applyLibrary(library);
    } catch (error) {
        console.error('Unable to load tray animation library.', error);
        if (state.collections.length) return;
        elements.board.innerHTML = '';
        elements.empty.hidden = false;
        elements.empty.querySelector('h3').textContent = copy.loadErrorTitle;
        elements.empty.querySelector('p').textContent = copy.loadErrorDescription;
    }
}

function applyLibrary(library) {
    state.collections = library.collections;
    state.authors = orderAuthors(library.authors);
    const generation = renderBoard();

    const firstCollection = state.authors[0]?.items[0] || state.collections[0];
    if (firstCollection) setTrayPreview(firstCollection, 1);
    scheduleProgressivePreviews(generation);
}

function renderBoard() {
    previewObserver?.disconnect();
    previewObserver = null;
    automaticMotionStarted = false;
    renderGeneration += 1;
    elements.board.replaceChildren(...state.authors.map((author, index) => createArtistRow(author, index)));
    elements.empty.hidden = state.authors.length > 0;
    return renderGeneration;
}

function createArtistRow(author, index) {
    const canExpand = author.total > FEATURED_ANIMATIONS;
    const isExpanded = canExpand && state.expandedAuthor === author.name;
    const row = document.createElement('article');
    row.className = `artist-showcase${isExpanded ? ' expanded' : ''}${canExpand ? '' : ' artist-showcase-static'}`;
    row.style.setProperty('--author-color', colorForIndex(index));

    const toggle = document.createElement('div');
    toggle.className = `artist-identity${canExpand ? '' : ' artist-identity-static'}`;
    if (canExpand) {
        toggle.tabIndex = 0;
        toggle.setAttribute('role', 'button');
        toggle.setAttribute('aria-expanded', String(isExpanded));
    }
    toggle.innerHTML = `
        ${createArtistAvatar(author, index === 0)}
        <span class="artist-heading">
            <span class="artist-name-line">
                ${createArtistName(author)}
                <span class="artist-status">${escapeHtml(copy.artistTag)}</span>
                ${createAuthorLinks(author)}
            </span>
            <span class="artist-metrics">${createArtistMetrics(author)}</span>
        </span>
        ${canExpand ? `<span class="artist-expand-label">${isExpanded ? copy.collapse : copy.expand} <i class="fas fa-chevron-down"></i></span>` : ''}
    `;

    toggle.addEventListener('mouseenter', () => previewFirstWork(author));
    toggle.addEventListener('focus', () => previewFirstWork(author));
    const toggleExpanded = () => {
        const shouldExpand = !row.classList.contains('expanded');
        const expandedRow = elements.board.querySelector('.artist-showcase.expanded');

        if (expandedRow && expandedRow !== row) setArtistRowExpanded(expandedRow, false);
        setArtistRowExpanded(row, shouldExpand, author);
        state.expandedAuthor = shouldExpand ? author.name : null;

        if (shouldExpand) requestAnimationFrame(() => row.scrollIntoView({ behavior: 'smooth', block: 'nearest' }));
    };
    if (canExpand) {
        toggle.addEventListener('click', event => {
            if (event.target.closest('a')) return;
            toggleExpanded();
        });
        toggle.addEventListener('keydown', event => {
            if (event.target !== toggle || (event.key !== 'Enter' && event.key !== ' ')) return;
            event.preventDefault();
            toggleExpanded();
        });
    }

    row.append(toggle, createFeaturedGallery(author, index === 0));
    if (isExpanded) row.appendChild(createArtistDetails(author));
    return row;
}

function createAuthorLinks(author) {
    return author.links.map(link => `
        <a class="artist-gallery-tag" href="${escapeAttribute(link.url)}" target="_blank" rel="noopener noreferrer">${escapeHtml(link.label)}</a>
    `).join('');
}

function createArtistName(author) {
    const url = preferredAuthorUrl(author);
    const name = `<strong>${escapeHtml(author.name)}</strong>`;
    return url
        ? `<a class="artist-name-link" href="${escapeAttribute(url)}" target="_blank" rel="noopener noreferrer">${name}</a>`
        : name;
}

function preferredAuthorUrl(author) {
    const pixiv = author.links.find(link => {
        if (link.label.toLowerCase() === 'pixiv') return true;
        try {
            const hostname = new URL(link.url).hostname.toLowerCase();
            return hostname === 'pixiv.net' || hostname.endsWith('.pixiv.net');
        } catch {
            return false;
        }
    });
    return pixiv?.url || author.links[0]?.url || '';
}

function setArtistRowExpanded(row, expanded, author) {
    row.classList.toggle('expanded', expanded);

    const toggle = row.querySelector('.artist-identity');
    const label = row.querySelector('.artist-expand-label');
    toggle?.setAttribute('aria-expanded', String(expanded));
    if (label) label.innerHTML = `${expanded ? copy.collapse : copy.expand} <i class="fas fa-chevron-down"></i>`;

    const details = row.querySelector(':scope > .artist-details');
    if (expanded && !details && author) {
        const newDetails = createArtistDetails(author);
        row.appendChild(newDetails);
        observeAnimationImages(newDetails);
    }
    if (!expanded) details?.remove();
}

function createArtistAvatar(author, highPriority = false) {
    const imagePriority = highPriority
        ? ' loading="eager" decoding="async" fetchpriority="high"'
        : ' loading="lazy" decoding="async"';
    const profileUrl = preferredAuthorUrl(author);
    const tag = profileUrl ? 'a' : 'span';
    const linkAttributes = profileUrl
        ? ` href="${escapeAttribute(profileUrl)}" target="_blank" rel="noopener noreferrer" aria-label="${escapeAttribute(copy.openProfile(author.name))}"`
        : '';
    if (author.avatar) {
        return `<${tag} class="artist-avatar artist-profile-link"${linkAttributes}><img src="${escapeAttribute(author.avatar)}" alt="${escapeAttribute(author.name)}"${imagePriority}></${tag}>`;
    }
    const preview = author.items[0] ? animationPosterUrl(author.items[0], 1) : '';
    if (preview) {
        return `<${tag} class="artist-avatar artist-profile-link"${linkAttributes}><img src="${escapeAttribute(preview)}" alt="${escapeAttribute(author.name)}"${imagePriority}></${tag}>`;
    }
    return `<${tag} class="artist-avatar artist-avatar-fallback artist-profile-link"${linkAttributes}>${escapeHtml(author.name.slice(0, 2))}</${tag}>`;
}

function createArtistMetrics(author) {
    return `<span class="artist-rating"><i class="fas fa-star"></i></span><span>${copy.animations(author.total)}</span>`;
}

function createFeaturedGallery(author, highPriority = false) {
    const gallery = document.createElement('div');
    gallery.className = 'artist-featured-gallery';
    gallery.append(...collectFeaturedWorks(author.items, FEATURED_ANIMATIONS).map(({ collection, index }, featuredIndex) => {
        const item = createAnimationItem(collection, index, {
            highPriority: highPriority && featuredIndex === 0,
        });
        item.classList.add('featured-animation');
        return item;
    }));
    return gallery;
}

function collectFeaturedWorks(collections, limit) {
    const works = [];
    let index = 1;
    while (works.length < limit && collections.some(collection => index <= collection.count)) {
        collections.forEach(collection => {
            if (works.length < limit && index <= collection.count) works.push({ collection, index });
        });
        index += 1;
    }
    return works;
}

function createArtistDetails(author) {
    const details = document.createElement('div');
    details.className = 'artist-details';

    const collections = document.createElement('div');
    collections.className = 'artist-collections';
    collections.append(...author.items.map(createCollectionSection));
    details.appendChild(collections);
    return details;
}

function createCollectionSection(collection) {
    const section = document.createElement('section');
    section.className = 'artist-collection';

    const visibleCount = state.visibleByCollection.get(collection.key)
        || Math.min(INITIAL_VISIBLE_ANIMATIONS, collection.count);

    const grid = document.createElement('div');
    grid.className = 'animation-grid';
    const fragment = document.createDocumentFragment();

    for (let index = 1; index <= visibleCount; index += 1) {
        fragment.appendChild(createAnimationItem(collection, index));
    }
    grid.appendChild(fragment);

    section.appendChild(grid);
    if (visibleCount < collection.count) {
        const loadMore = document.createElement('button');
        loadMore.type = 'button';
        loadMore.className = 'load-more';
        loadMore.textContent = copy.loadMore(collection.count - visibleCount);
        loadMore.addEventListener('click', event => {
            event.stopPropagation();
            state.visibleByCollection.set(collection.key, Math.min(visibleCount + LOAD_MORE_SIZE, collection.count));
            const replacement = createCollectionSection(collection);
            section.replaceWith(replacement);
            observeAnimationImages(replacement);
        });
        section.appendChild(loadMore);
    }

    return section;
}

function createAnimationItem(collection, index, { highPriority = false } = {}) {
    const originalUrl = animationUrl(collection, index);
    const posterUrl = animationPosterUrl(collection, index);
    const previewUrl = animationPreviewUrl(collection, index);
    const filename = animationDownloadFilename(collection, index);
    const item = document.createElement('a');
    item.className = 'animation-item';
    item.href = originalUrl;
    item.download = filename;

    const image = document.createElement('img');
    image.alt = `${collection.title} ${index}`;
    image.loading = highPriority ? 'eager' : 'lazy';
    image.decoding = 'async';
    image.fetchPriority = highPriority ? 'high' : 'low';
    image.dataset.posterUrl = posterUrl;
    image.dataset.previewUrl = previewUrl;
    if (highPriority) image.dataset.criticalPoster = 'true';
    image.addEventListener('load', () => image.classList.add('is-loaded'));
    image.src = posterUrl;
    if (image.complete) image.classList.add('is-loaded');
    item.appendChild(image);

    const activateMotion = () => {
        image.dataset.manualMotion = 'true';
        requestAnimationMotion(image, { manual: true, priority: true });
        setTrayPreview(collection, index, { manual: true });
    };
    const deactivateMotion = () => {
        image.dataset.manualMotion = 'false';
        if (!motionPolicy.automatic || image.dataset.inViewport !== 'true') restorePoster(image);
    };
    item.addEventListener('mouseenter', activateMotion);
    item.addEventListener('mouseleave', deactivateMotion);
    item.addEventListener('focus', activateMotion);
    item.addEventListener('blur', deactivateMotion);
    item.addEventListener('click', event => downloadAnimation(event, originalUrl, filename));
    return item;
}

async function downloadAnimation(event, url, filename) {
    event.preventDefault();

    try {
        const response = await fetch(url);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const objectUrl = URL.createObjectURL(await response.blob());
        const anchor = document.createElement('a');
        anchor.href = objectUrl;
        anchor.download = filename;
        anchor.click();
        setTimeout(() => URL.revokeObjectURL(objectUrl), 1000);
    } catch (error) {
        console.warn('Direct download failed; opening the animation source instead.', error);
        window.open(url, '_blank', 'noopener,noreferrer');
    }
}

function previewFirstWork(author) {
    const collection = author.items[0];
    if (collection) setTrayPreview(collection, 1, { manual: true });
}

function setTrayPreview(collection, index, { manual = false } = {}) {
    const selection = {
        token: Symbol('tray-preview'),
        posterUrl: animationPosterUrl(collection, index),
        previewUrl: animationPreviewUrl(collection, index),
    };
    trayPreviewSelection = selection;
    if (elements.trayIcon.getAttribute('src') !== selection.posterUrl) {
        elements.trayIcon.src = selection.posterUrl;
    }
    if (manual || automaticMotionStarted) loadTrayMotion(selection, { manual });
}

function loadTrayMotion(selection, { manual }) {
    if (manual ? !motionPolicy.manual : !motionPolicy.automatic) return;
    previewLoader.request(selection.previewUrl, { priority: true }).then(loaded => {
        if (!loaded || trayPreviewSelection?.token !== selection.token) return;
        if (elements.trayIcon.getAttribute('src') !== selection.previewUrl) {
            elements.trayIcon.src = selection.previewUrl;
        }
    });
}

function scheduleProgressivePreviews(generation) {
    if (!motionPolicy.automatic || !state.authors.length) return;
    const criticalPoster = elements.board.querySelector('img[data-critical-poster="true"]');
    const begin = () => scheduleAfterCriticalRender(() => startProgressivePreviews(generation));

    if (!criticalPoster || criticalPoster.complete) {
        begin();
        return;
    }
    criticalPoster.addEventListener('load', begin, { once: true });
    criticalPoster.addEventListener('error', begin, { once: true });
}

function scheduleAfterCriticalRender(callback) {
    if (typeof window.requestIdleCallback === 'function') {
        window.requestIdleCallback(callback, { timeout: 700 });
    } else {
        setTimeout(callback, 0);
    }
}

function startProgressivePreviews(generation) {
    if (generation !== renderGeneration || automaticMotionStarted) return;
    automaticMotionStarted = true;
    if (trayPreviewSelection) loadTrayMotion(trayPreviewSelection, { manual: false });
    if (!('IntersectionObserver' in window)) {
        const firstImage = elements.board.querySelector('img[data-preview-url]');
        if (firstImage) {
            firstImage.dataset.inViewport = 'true';
            requestAnimationMotion(firstImage);
        }
        return;
    }

    previewObserver = new IntersectionObserver(entries => {
        entries.forEach(entry => {
            const image = entry.target;
            image.dataset.inViewport = String(entry.isIntersecting);
            if (entry.isIntersecting) {
                requestAnimationMotion(image);
            } else if (image.dataset.manualMotion !== 'true') {
                restorePoster(image);
            }
        });
    }, {
        rootMargin: '180px 0px',
        threshold: 0.01,
    });
    observeAnimationImages(elements.board);
}

function observeAnimationImages(root) {
    if (!previewObserver || !root) return;
    if (root.matches?.('img[data-preview-url]')) previewObserver.observe(root);
    root.querySelectorAll?.('img[data-preview-url]').forEach(image => previewObserver.observe(image));
}

function requestAnimationMotion(image, { manual = false, priority = false } = {}) {
    if (manual ? !motionPolicy.manual : !motionPolicy.automatic) return;
    const previewUrl = image.dataset.previewUrl;
    if (!previewUrl) return;

    previewLoader.request(previewUrl, { priority }).then(loaded => {
        if (!loaded || !image.isConnected || image.dataset.previewUrl !== previewUrl) return;
        const shouldAnimate = image.dataset.inViewport === 'true' || image.dataset.manualMotion === 'true';
        if (shouldAnimate && image.getAttribute('src') !== previewUrl) image.src = previewUrl;
    });
}

function restorePoster(image) {
    const posterUrl = image.dataset.posterUrl;
    if (posterUrl && image.getAttribute('src') !== posterUrl) image.src = posterUrl;
}

function loadPreviewImage(url) {
    return new Promise(resolve => {
        const image = new Image();
        let settled = false;
        const finish = loaded => {
            if (settled) return;
            settled = true;
            resolve(loaded);
        };
        image.decoding = 'async';
        image.referrerPolicy = 'strict-origin-when-cross-origin';
        image.addEventListener('load', () => {
            if (typeof image.decode !== 'function') {
                finish(true);
                return;
            }
            image.decode().catch(() => {}).finally(() => finish(true));
        }, { once: true });
        image.addEventListener('error', () => finish(false), { once: true });
        image.src = url;
    });
}

function updateTrayClock() {
    const now = new Date();
    elements.trayClock.textContent = new Intl.DateTimeFormat('zh-CN', {
        hour: '2-digit', minute: '2-digit', hour12: false,
    }).format(now);
    elements.trayDate.textContent = `${now.getFullYear()}/${String(now.getMonth() + 1).padStart(2, '0')}/${String(now.getDate()).padStart(2, '0')}`;
}
