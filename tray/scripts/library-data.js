const DEFAULT_LIBRARY_SOURCE = 'https://tray.cati.me/sections.json';
const LIBRARY_CACHE_KEY = 'catime:tray-library:v6';
const MAX_CACHED_MANIFEST_BYTES = 2 * 1024 * 1024;

export function loadImmediateLibraryData(source = configuredLibrarySource()) {
    const cached = readCachedPayload(source);
    if (!cached) return null;
    try {
        return normalizeLibrary(cached);
    } catch {
        return null;
    }
}

export async function loadLibraryData(source = configuredLibrarySource()) {
    const response = await fetch(source, {
        cache: 'no-cache',
        mode: 'cors',
        credentials: 'omit',
        referrerPolicy: 'strict-origin-when-cross-origin',
        headers: { Accept: 'application/json' },
    });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);

    const payload = await response.json();
    cachePayload(source, payload);
    return normalizeLibrary(payload);
}

export function normalizeLibrary(payload) {
    const collections = Object.entries(payload.sections && typeof payload.sections === 'object' ? payload.sections : {})
        .filter(([, data]) => data && typeof data === 'object')
        .map(([key, data]) => normalizeCollection(key, data))
        .filter(collection => collection.count > 0);

    return {
        collections,
        authors: groupByAuthor(collections),
    };
}

function normalizeCollection(key, data) {
    const files = Array.isArray(data.files) ? data.files.map(String) : [];
    return {
        key,
        title: key,
        author: key,
        authorAvatar: typeof data.authorAvatar === 'string' ? data.authorAvatar : '',
        authorLinks: normalizeAuthorLinks(data.authorLinks),
        files,
        fileVersions: Array.isArray(data.fileVersions) ? data.fileVersions.map(String) : [],
        count: files.length,
        cdnBase: typeof data.cdnBase === 'string' ? data.cdnBase : '',
        repository: typeof data.repository === 'string' ? data.repository : '',
    };
}

function groupByAuthor(collections) {
    const groups = new Map();

    collections.forEach(collection => {
        if (!groups.has(collection.author)) groups.set(collection.author, []);
        groups.get(collection.author).push(collection);
    });

    return [...groups.entries()].map(([name, items]) => ({
        name,
        items,
        total: items.reduce((sum, item) => sum + item.count, 0),
        avatar: items.find(item => item.authorAvatar)?.authorAvatar || '',
        links: mergeAuthorLinks(items),
    }));
}

function normalizeAuthorLinks(links) {
    if (!Array.isArray(links)) return [];
    return links.flatMap(link => {
        if (!link || typeof link.label !== 'string' || typeof link.url !== 'string') return [];
        try {
            const url = new URL(link.url);
            if (url.protocol !== 'https:' && url.protocol !== 'http:') return [];
            return [{ label: link.label.trim(), url: url.toString() }];
        } catch {
            return [];
        }
    }).filter(link => link.label);
}

function mergeAuthorLinks(items) {
    const links = items.flatMap(item => item.authorLinks);
    const seen = new Set();
    return links.filter(link => {
        const key = `${link.label.toLowerCase()}\0${link.url}`;
        if (seen.has(key)) return false;
        seen.add(key);
        return true;
    });
}

export function animationFilename(collection, index) {
    const filename = collection.files[index - 1];
    if (!filename) throw new RangeError(`Animation ${index} does not exist in ${collection.key}`);
    return filename;
}

export function animationDownloadFilename(collection, index) {
    const sourceFilename = animationFilename(collection, index).split('/').pop() || '';
    const extension = sourceFilename.match(/\.[a-z0-9]+$/i)?.[0] || '.gif';
    const author = String(collection.author || collection.title || collection.key || 'animation')
        .replace(/[<>:"/\\|?*\u0000-\u001f]+/g, '-')
        .replace(/[. ]+$/g, '')
        .trim() || 'animation';

    return `${author}-${index}${extension}`;
}

export function animationUrl(collection, index) {
    const filename = animationFilename(collection, index)
        .split('/')
        .map(encodeURIComponent)
        .join('/');
    const url = `${collection.cdnBase}${filename}`;
    const version = collection.fileVersions[index - 1];
    return version ? `${url}?v=${encodeURIComponent(version)}` : url;
}

function configuredLibrarySource() {
    return import.meta.env?.VITE_TRAY_HUB_URL || DEFAULT_LIBRARY_SOURCE;
}

function readCachedPayload(source) {
    if (typeof localStorage === 'undefined') return null;
    try {
        const cached = JSON.parse(localStorage.getItem(LIBRARY_CACHE_KEY) || 'null');
        return cached?.source === source && cached.payload?.sections ? cached.payload : null;
    } catch {
        return null;
    }
}

function cachePayload(source, payload) {
    if (typeof localStorage === 'undefined' || !payload?.sections) return;
    try {
        const value = JSON.stringify({ source, payload });
        if (value.length <= MAX_CACHED_MANIFEST_BYTES) localStorage.setItem(LIBRARY_CACHE_KEY, value);
    } catch {
        // Storage may be unavailable in private mode; the bundled snapshot
        // still keeps the first render synchronous.
    }
}
