import { bundledLibraryPayload } from './library-snapshot.js';

const DEFAULT_LIBRARY_SOURCE = 'https://tray.cati.me/sections.json';
const LIBRARY_CACHE_KEY = 'catime:tray-library:v4';
const MAX_CACHED_MANIFEST_BYTES = 2 * 1024 * 1024;

export function loadImmediateLibraryData(source = configuredLibrarySource()) {
    const cached = readCachedPayload(source);
    const bundled = source === DEFAULT_LIBRARY_SOURCE ? bundledLibraryPayload : null;
    const payload = newerPayload(cached, bundled);
    if (!payload) return null;
    try {
        return normalizeLibrary(payload);
    } catch {
        return payload !== bundled && bundled ? normalizeLibrary(bundled) : null;
    }
}

export async function loadLibraryData(source = configuredLibrarySource()) {
    const response = await fetch(source, {
        cache: 'default',
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

function normalizeLibrary(payload) {
    const collections = Object.entries(payload.sections && typeof payload.sections === 'object' ? payload.sections : {})
        .filter(([, data]) => data && typeof data === 'object')
        .map(([key, data]) => normalizeCollection(key, data))
        .filter(collection => collection.count > 0);

    return {
        collections,
        authors: groupByAuthor(collections),
        revision: String(payload.generated || payload.version || ''),
    };
}

function normalizeCollection(key, data) {
    return {
        key,
        title: data.title || key,
        author: String(data.author || data.creator || data.contributor || data.title || key).trim(),
        authorBio: data.authorBio || data.bio || '',
        authorAvatar: data.authorAvatar || data.avatar || '',
        authorUrl: data.authorUrl || data.creatorUrl || '',
        authorTag: data.authorTag || data.tag || '',
        authorLinks: normalizeAuthorLinks(data.authorLinks),
        rating: Number(data.rating) || 0,
        reviewCount: Number(data.reviewCount) || 0,
        description: data.description || '',
        files: Array.isArray(data.files) ? data.files.map(String) : [],
        fileVersions: Array.isArray(data.fileVersions) ? data.fileVersions.map(String) : [],
        count: Array.isArray(data.files) ? data.files.length : Number(data.count) || 0,
        cdnBase: data.cdnBase || `./gifs/${key}/`,
        repository: data.repository || '',
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
        bio: items.find(item => item.authorBio)?.authorBio || '',
        avatar: items.find(item => item.authorAvatar)?.authorAvatar || '',
        url: items.find(item => item.authorUrl)?.authorUrl || '',
        tag: items.find(item => item.authorTag)?.authorTag || '',
        links: mergeAuthorLinks(items),
        rating: items.find(item => item.rating)?.rating || 0,
        reviewCount: items.reduce((sum, item) => sum + item.reviewCount, 0),
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
    return collection.files[index - 1]
        || `${String(index).padStart(4, '0')}_${collection.key}.gif`;
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

function newerPayload(first, second) {
    if (!first) return second;
    if (!second) return first;
    const firstTime = Date.parse(first.generated || '') || 0;
    const secondTime = Date.parse(second.generated || '') || 0;
    return firstTime >= secondTime ? first : second;
}
