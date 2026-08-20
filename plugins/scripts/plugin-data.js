const DEFAULT_CATALOG_SOURCE = 'https://vladelaina.github.io/Catime-Plugins/api/v1/catalog.json';
const CACHE_KEY = 'catime:plugin-catalog:v1';
const MAX_CATALOG_BYTES = 1024 * 1024;
const MAX_PLUGIN_BYTES = 1024 * 1024;

export function configuredCatalogSource() {
    return import.meta.env?.VITE_PLUGIN_CATALOG_URL || DEFAULT_CATALOG_SOURCE;
}

export function loadCachedCatalog(source = configuredCatalogSource()) {
    if (typeof localStorage === 'undefined') return null;
    try {
        const cached = JSON.parse(localStorage.getItem(CACHE_KEY) || 'null');
        if (cached?.source !== source) return null;
        return normalizeCatalog(cached.payload);
    } catch {
        return null;
    }
}

export async function loadPluginCatalog(source = configuredCatalogSource(), fetchImpl = fetch) {
    const response = await fetchImpl(source, {
        cache: 'default',
        mode: 'cors',
        credentials: 'omit',
        referrerPolicy: 'strict-origin-when-cross-origin',
    });
    if (!response.ok) throw new Error(`Catalog request returned HTTP ${response.status}`);
    const length = Number.parseInt(response.headers.get('content-length') || '0', 10);
    if (length > MAX_CATALOG_BYTES) throw new Error('Catalog response is too large');

    const body = await response.text();
    if (new TextEncoder().encode(body).byteLength > MAX_CATALOG_BYTES) {
        throw new Error('Catalog response is too large');
    }
    const payload = JSON.parse(body);
    const catalog = normalizeCatalog(payload);
    cacheCatalog(source, payload);
    return catalog;
}

export function normalizeCatalog(payload) {
    if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
        throw new TypeError('Catalog must be an object');
    }
    if (payload.schemaVersion !== 1) throw new TypeError('Unsupported catalog schema');
    if (!Array.isArray(payload.plugins)) throw new TypeError('Catalog plugins must be an array');
    if (payload.count !== payload.plugins.length) throw new TypeError('Catalog count does not match plugins');

    const plugins = payload.plugins.map((plugin, index) => normalizePlugin(plugin, index));
    const ids = new Set(plugins.map(plugin => plugin.id));
    if (ids.size !== plugins.length) throw new TypeError('Catalog contains duplicate plugin ids');

    const source = payload.source;
    if (!source || typeof source !== 'object') throw new TypeError('Catalog source is missing');
    const commit = typeof source.commit === 'string' && /^[a-f0-9]{40}$/i.test(source.commit)
        ? source.commit.toLowerCase()
        : '';
    if (!commit) throw new TypeError('Catalog source commit is invalid');

    return {
        schemaVersion: 1,
        generatedAt: validDate(payload.generatedAt),
        source: {
            repository: publicHttpsUrl(source.repository, 'repository'),
            commit,
        },
        count: plugins.length,
        plugins,
    };
}

export async function fetchVerifiedPlugin(plugin, fetchImpl = fetch) {
    const response = await fetchImpl(plugin.downloadUrl, {
        cache: 'no-store',
        mode: 'cors',
        credentials: 'omit',
        referrerPolicy: 'no-referrer',
    });
    if (!response.ok) throw new Error(`Plugin request returned HTTP ${response.status}`);

    const length = Number.parseInt(response.headers.get('content-length') || '0', 10);
    if (length > MAX_PLUGIN_BYTES) throw new Error('Plugin file is too large');
    const bytes = await response.arrayBuffer();
    if (bytes.byteLength !== plugin.size || bytes.byteLength > MAX_PLUGIN_BYTES) {
        throw new Error('Plugin file size does not match the catalog');
    }
    const digest = await sha256Hex(bytes);
    if (digest !== plugin.sha256) throw new Error('Plugin SHA-256 verification failed');
    return new Blob([bytes], { type: 'application/octet-stream' });
}

export async function sha256Hex(bytes) {
    if (!globalThis.crypto?.subtle) throw new Error('SHA-256 verification is unavailable');
    const digest = await globalThis.crypto.subtle.digest('SHA-256', bytes);
    return [...new Uint8Array(digest)].map(byte => byte.toString(16).padStart(2, '0')).join('');
}

function normalizePlugin(plugin, index) {
    const label = `plugins[${index}]`;
    if (!plugin || typeof plugin !== 'object' || Array.isArray(plugin)) throw new TypeError(`${label} is invalid`);
    if (!/^[a-z0-9]+(?:_[a-z0-9]+)*$/.test(plugin.id || '')) throw new TypeError(`${label}.id is invalid`);
    if (!/^[A-Za-z0-9][A-Za-z0-9._-]*\.(?:bat|py)$/i.test(plugin.filename || '')) {
        throw new TypeError(`${label}.filename is invalid`);
    }
    if (!/^[a-f0-9]{64}$/i.test(plugin.sha256 || '')) throw new TypeError(`${label}.sha256 is invalid`);
    if (!Number.isInteger(plugin.size) || plugin.size <= 0 || plugin.size > MAX_PLUGIN_BYTES) {
        throw new TypeError(`${label}.size is invalid`);
    }
    const previewUrl = publicHttpsUrl(plugin.previewUrl, `${label}.previewUrl`);
    const posterUrl = plugin.posterUrl === undefined
        ? previewUrl
        : publicHttpsUrl(plugin.posterUrl, `${label}.posterUrl`);
    return {
        id: plugin.id,
        filename: plugin.filename,
        posterUrl,
        previewUrl,
        downloadUrl: publicHttpsUrl(plugin.downloadUrl, `${label}.downloadUrl`),
        size: plugin.size,
        sha256: plugin.sha256.toLowerCase(),
    };
}

function publicHttpsUrl(value, label) {
    try {
        const url = new URL(value);
        if (url.protocol !== 'https:' || url.username || url.password) throw new Error();
        return url.toString();
    } catch {
        throw new TypeError(`${label} must be a public HTTPS URL`);
    }
}

function validDate(value) {
    if (typeof value !== 'string' || Number.isNaN(Date.parse(value))) throw new TypeError('generatedAt is invalid');
    return value;
}

function cacheCatalog(source, payload) {
    if (typeof localStorage === 'undefined') return;
    try {
        const value = JSON.stringify({ source, payload });
        if (value.length <= MAX_CATALOG_BYTES) localStorage.setItem(CACHE_KEY, value);
    } catch {
        // Storage may be unavailable or full.
    }
}
