import { animationFilename, animationUrl, loadImmediateLibraryData, loadLibraryData } from './library-data.js';
import { colorForIndex, escapeAttribute, escapeHtml } from './dom-utils.js';

const INITIAL_VISIBLE_ANIMATIONS = 18;
const LOAD_MORE_SIZE = 24;
const FEATURED_ANIMATIONS = 5;
const preloadedUrls = new Set();

const state = {
    collections: [],
    authors: [],
    expandedAuthor: null,
    visibleByCollection: new Map(),
    revision: '',
    userInteracted: false,
};

const elements = {};

document.addEventListener('DOMContentLoaded', () => {
    Object.assign(elements, {
        board: document.getElementById('trayBoard'),
        empty: document.getElementById('boardEmpty'),
        trayIcon: document.getElementById('trayIconImage'),
        trayClock: document.getElementById('trayClock'),
        trayDate: document.getElementById('trayDate'),
    });

    updateTrayClock();
    setInterval(updateTrayClock, 30000);
    const immediateLibrary = loadImmediateLibraryData();
    if (immediateLibrary?.collections.length) applyLibrary(immediateLibrary);
    loadLibrary(Boolean(immediateLibrary?.collections.length));
});

async function loadLibrary(hasImmediateLibrary) {
    try {
        const library = await loadLibraryData();
        // Keep an already-visible board stable. The fresh payload has been
        // cached for the next navigation. A newer catalog can update this view
        // only while the user has not started interacting with it.
        const hasNewRevision = isNewerRevision(library.revision, state.revision);
        if (!hasImmediateLibrary || (hasNewRevision && !state.userInteracted)) applyLibrary(library);
    } catch (error) {
        console.error('Unable to load tray animation library.', error);
        if (state.collections.length) return;
        elements.board.innerHTML = '';
        elements.empty.hidden = false;
        elements.empty.querySelector('h3').textContent = '动画资源加载失败';
        elements.empty.querySelector('p').textContent = '请稍后刷新页面重试。';
    }
}

function isNewerRevision(next, current) {
    if (!next || next === current) return false;
    const nextTime = Date.parse(next);
    const currentTime = Date.parse(current);
    if (Number.isFinite(nextTime) && Number.isFinite(currentTime)) return nextTime > currentTime;
    return true;
}

function applyLibrary(library) {
    state.collections = library.collections;
    state.authors = library.authors;
    state.revision = library.revision;
    preloadFirstRow(library.authors[0]);
    renderBoard();

    if (state.collections[0]) setTrayPreview(animationUrl(state.collections[0], 1));
}

function renderBoard() {
    elements.board.replaceChildren(...state.authors.map((author, index) => createArtistRow(author, index)));
    elements.empty.hidden = state.authors.length > 0;
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
                <span class="artist-status">${escapeHtml(author.tag || '动画作者')}</span>
                ${createAuthorLinks(author)}
            </span>
            <span class="artist-metrics">${createArtistMetrics(author)}</span>
        </span>
        ${canExpand ? `<span class="artist-expand-label">${isExpanded ? '收起' : '展开'} <i class="fas fa-chevron-down"></i></span>` : ''}
    `;

    toggle.addEventListener('mouseenter', () => previewFirstWork(author));
    toggle.addEventListener('focus', () => previewFirstWork(author));
    const toggleExpanded = () => {
        state.userInteracted = true;
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
    const links = author.links.length > 0
        ? author.links
        : author.url ? [{ label: author.tag || '作者主页', url: author.url }] : [];
    return links.map(link => `
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
    const links = author.links.length > 0
        ? author.links
        : author.url ? [{ label: author.tag || 'Homepage', url: author.url }] : [];
    const pixiv = links.find(link => {
        if (link.label.toLowerCase() === 'pixiv') return true;
        try {
            const hostname = new URL(link.url).hostname.toLowerCase();
            return hostname === 'pixiv.net' || hostname.endsWith('.pixiv.net');
        } catch {
            return false;
        }
    });
    return pixiv?.url || links[0]?.url || '';
}

function setArtistRowExpanded(row, expanded, author) {
    row.classList.toggle('expanded', expanded);

    const toggle = row.querySelector('.artist-identity');
    const label = row.querySelector('.artist-expand-label');
    toggle?.setAttribute('aria-expanded', String(expanded));
    if (label) label.innerHTML = `${expanded ? '收起' : '展开'} <i class="fas fa-chevron-down"></i>`;

    const details = row.querySelector(':scope > .artist-details');
    if (expanded && !details && author) row.appendChild(createArtistDetails(author));
    if (!expanded) details?.remove();
}

function createArtistAvatar(author, highPriority = false) {
    const imagePriority = highPriority
        ? ' loading="eager" decoding="async" fetchpriority="high"'
        : ' loading="lazy" decoding="async"';
    const profileUrl = preferredAuthorUrl(author);
    const tag = profileUrl ? 'a' : 'span';
    const linkAttributes = profileUrl
        ? ` href="${escapeAttribute(profileUrl)}" target="_blank" rel="noopener noreferrer" aria-label="Open ${escapeAttribute(author.name)} profile"`
        : '';
    if (author.avatar) {
        return `<${tag} class="artist-avatar artist-profile-link"${linkAttributes}><img src="${escapeAttribute(author.avatar)}" alt="${escapeAttribute(author.name)}"${imagePriority}></${tag}>`;
    }
    const preview = author.items[0] ? animationUrl(author.items[0], 1) : '';
    if (preview) {
        return `<${tag} class="artist-avatar artist-profile-link"${linkAttributes}><img src="${escapeAttribute(preview)}" alt="${escapeAttribute(author.name)}"${imagePriority}></${tag}>`;
    }
    return `<${tag} class="artist-avatar artist-avatar-fallback artist-profile-link"${linkAttributes}>${escapeHtml(author.name.slice(0, 2))}</${tag}>`;
}

function createArtistMetrics(author) {
    if (author.rating > 0) {
        return `<span class="artist-rating"><i class="fas fa-star"></i> ${author.rating.toFixed(1)}</span><span>${author.reviewCount.toLocaleString('zh-CN')} 条评价</span>`;
    }
    return `<span class="artist-rating"><i class="fas fa-star"></i></span><span>${author.total.toLocaleString('zh-CN')} 个托盘动画</span>`;
}

function createFeaturedGallery(author, highPriority = false) {
    const gallery = document.createElement('div');
    gallery.className = 'artist-featured-gallery';
    gallery.append(...collectFeaturedWorks(author.items, FEATURED_ANIMATIONS).map(({ collection, index }) => {
        const item = createAnimationItem(collection, index, { highPriority });
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
        loadMore.textContent = `加载更多（剩余 ${(collection.count - visibleCount).toLocaleString('zh-CN')} 个）`;
        loadMore.addEventListener('click', event => {
            event.stopPropagation();
            state.userInteracted = true;
            state.visibleByCollection.set(collection.key, Math.min(visibleCount + LOAD_MORE_SIZE, collection.count));
            section.replaceWith(createCollectionSection(collection));
        });
        section.appendChild(loadMore);
    }

    return section;
}

function createAnimationItem(collection, index, { highPriority = false } = {}) {
    const url = animationUrl(collection, index);
    const filename = animationFilename(collection, index);
    const item = document.createElement('a');
    item.className = 'animation-item';
    item.href = url;
    item.download = filename;
    const loading = highPriority ? 'eager' : 'lazy';
    const priority = highPriority ? ' fetchpriority="high"' : '';
    item.innerHTML = `<img src="${escapeAttribute(url)}" alt="${escapeAttribute(collection.title)} ${index}" loading="${loading}" decoding="async"${priority}>`;
    item.addEventListener('mouseenter', () => setTrayPreview(url));
    item.addEventListener('focus', () => setTrayPreview(url));
    item.addEventListener('click', event => downloadAnimation(event, url, filename));
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
    if (collection) setTrayPreview(animationUrl(collection, 1));
}

function setTrayPreview(url) {
    if (elements.trayIcon.getAttribute('src') !== url) elements.trayIcon.src = url;
}

function preloadFirstRow(author) {
    if (!author) return;
    collectFeaturedWorks(author.items, FEATURED_ANIMATIONS).forEach(({ collection, index }) => {
        const url = animationUrl(collection, index);
        if (preloadedUrls.has(url)) return;
        preloadedUrls.add(url);
        const preload = document.createElement('link');
        preload.rel = 'preload';
        preload.as = 'image';
        preload.href = url;
        preload.fetchPriority = 'high';
        document.head.appendChild(preload);
    });
}

function updateTrayClock() {
    const now = new Date();
    elements.trayClock.textContent = new Intl.DateTimeFormat('zh-CN', {
        hour: '2-digit', minute: '2-digit', hour12: false,
    }).format(now);
    elements.trayDate.textContent = `${now.getFullYear()}/${String(now.getMonth() + 1).padStart(2, '0')}/${String(now.getDate()).padStart(2, '0')}`;
}
