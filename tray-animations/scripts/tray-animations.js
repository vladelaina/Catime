import { animationFilename, animationUrl, loadLibraryData } from './library-data.js';
import { colorForIndex, escapeAttribute, escapeHtml } from './dom-utils.js';

const INITIAL_VISIBLE_ANIMATIONS = 18;
const LOAD_MORE_SIZE = 24;
const FEATURED_ANIMATIONS = 5;

const state = {
    collections: [],
    authors: [],
    expandedAuthor: null,
    visibleByCollection: new Map(),
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
    loadLibrary();
});

async function loadLibrary() {
    try {
        const library = await loadLibraryData();
        state.collections = library.collections;
        state.authors = library.authors;
        renderBoard();

        if (state.collections[0]) {
            setTrayPreview(animationUrl(state.collections[0], 1));
        }
    } catch (error) {
        console.error('Unable to load tray animation library.', error);
        elements.board.innerHTML = '';
        elements.empty.hidden = false;
        elements.empty.querySelector('h3').textContent = '动画资源加载失败';
        elements.empty.querySelector('p').textContent = '请稍后刷新页面重试。';
    }
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

    const toggle = document.createElement(canExpand ? 'button' : 'div');
    if (canExpand) toggle.type = 'button';
    toggle.className = `artist-identity${canExpand ? '' : ' artist-identity-static'}`;
    if (canExpand) toggle.setAttribute('aria-expanded', String(isExpanded));
    toggle.innerHTML = `
        ${createArtistAvatar(author)}
        <span class="artist-heading">
            <span class="artist-name-line">
                <strong>${escapeHtml(author.name)}</strong>
                <span class="artist-status">${escapeHtml(author.tag || '动画作者')}</span>
                <span class="artist-gallery-tag">Bilibili</span>
            </span>
            <span class="artist-metrics">${createArtistMetrics(author)}</span>
        </span>
        ${canExpand ? `<span class="artist-expand-label">${isExpanded ? '收起' : '展开'} <i class="fas fa-chevron-down"></i></span>` : ''}
    `;

    toggle.addEventListener('mouseenter', () => previewFirstWork(author));
    toggle.addEventListener('focus', () => previewFirstWork(author));
    if (canExpand) toggle.addEventListener('click', () => {
        const shouldExpand = !row.classList.contains('expanded');
        const expandedRow = elements.board.querySelector('.artist-showcase.expanded');

        if (expandedRow && expandedRow !== row) setArtistRowExpanded(expandedRow, false);
        setArtistRowExpanded(row, shouldExpand, author);
        state.expandedAuthor = shouldExpand ? author.name : null;

        if (shouldExpand) requestAnimationFrame(() => row.scrollIntoView({ behavior: 'smooth', block: 'nearest' }));
    });

    row.append(toggle, createFeaturedGallery(author));
    if (isExpanded) row.appendChild(createArtistDetails(author));
    return row;
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

function createArtistAvatar(author) {
    if (author.avatar) {
        return `<span class="artist-avatar"><img src="${escapeAttribute(author.avatar)}" alt="${escapeAttribute(author.name)}"></span>`;
    }
    const preview = author.items[0] ? animationUrl(author.items[0], 1) : '';
    if (preview) {
        return `<span class="artist-avatar"><img src="${escapeAttribute(preview)}" alt="${escapeAttribute(author.name)}"></span>`;
    }
    return `<span class="artist-avatar artist-avatar-fallback">${escapeHtml(author.name.slice(0, 2))}</span>`;
}

function createArtistMetrics(author) {
    if (author.rating > 0) {
        return `<span class="artist-rating"><i class="fas fa-star"></i> ${author.rating.toFixed(1)}</span><span>${author.reviewCount.toLocaleString('zh-CN')} 条评价</span>`;
    }
    return `<span class="artist-rating"><i class="fas fa-star"></i></span><span>${author.total.toLocaleString('zh-CN')} 个托盘动画</span>`;
}

function createFeaturedGallery(author) {
    const gallery = document.createElement('div');
    gallery.className = 'artist-featured-gallery';
    gallery.append(...collectFeaturedWorks(author.items, FEATURED_ANIMATIONS).map(({ collection, index }) => {
        const item = createAnimationItem(collection, index);
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
            state.visibleByCollection.set(collection.key, Math.min(visibleCount + LOAD_MORE_SIZE, collection.count));
            renderBoard();
        });
        section.appendChild(loadMore);
    }

    return section;
}

function createAnimationItem(collection, index) {
    const url = animationUrl(collection, index);
    const filename = animationFilename(collection, index);
    const item = document.createElement('a');
    item.className = 'animation-item';
    item.href = url;
    item.download = filename;
    item.innerHTML = `<img src="${escapeAttribute(url)}" alt="${escapeAttribute(collection.title)} ${index}" loading="lazy" decoding="async">`;
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
    elements.trayIcon.src = url;
}

function updateTrayClock() {
    const now = new Date();
    elements.trayClock.textContent = new Intl.DateTimeFormat('zh-CN', {
        hour: '2-digit', minute: '2-digit', hour12: false,
    }).format(now);
    elements.trayDate.textContent = `${now.getFullYear()}/${String(now.getMonth() + 1).padStart(2, '0')}/${String(now.getDate()).padStart(2, '0')}`;
}
