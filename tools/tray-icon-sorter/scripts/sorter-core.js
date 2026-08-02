const IMAGE_EXTENSIONS = new Set([
    'avif',
    'bmp',
    'gif',
    'heic',
    'heif',
    'ico',
    'jfif',
    'jpeg',
    'jpg',
    'png',
    'svg',
    'tif',
    'tiff',
    'webp',
]);

const MIME_EXTENSIONS = {
    'image/avif': 'avif',
    'image/bmp': 'bmp',
    'image/gif': 'gif',
    'image/heic': 'heic',
    'image/heif': 'heif',
    'image/jpeg': 'jpg',
    'image/jpg': 'jpg',
    'image/png': 'png',
    'image/svg+xml': 'svg',
    'image/tiff': 'tiff',
    'image/vnd.microsoft.icon': 'ico',
    'image/webp': 'webp',
    'image/x-icon': 'ico',
};

export function getFileExtension(file) {
    const filename = String(file?.name || '');
    const match = filename.match(/\.([a-z\d]{1,12})$/i);
    if (match) return match[1].toLowerCase();

    const mimeType = String(file?.type || '').toLowerCase();
    return MIME_EXTENSIONS[mimeType] || 'img';
}

export function isImageFile(file) {
    const mimeType = String(file?.type || '').toLowerCase();
    if (mimeType.startsWith('image/')) return true;
    return IMAGE_EXTENSIONS.has(getFileExtension(file));
}

export function createNumberedFilename(file, position) {
    if (!Number.isInteger(position) || position < 1) {
        throw new RangeError('Image position must be an integer starting at 1.');
    }

    return `${position}.${getFileExtension(file)}`;
}

export function createSequentialEntries(items) {
    return items.map((item, index) => ({
        name: createNumberedFilename(item.file, index + 1),
        blob: item.file,
        lastModified: item.file.lastModified,
    }));
}

export function createArchiveFilename(date = new Date()) {
    const pad = value => String(value).padStart(2, '0');
    const day = [date.getFullYear(), pad(date.getMonth() + 1), pad(date.getDate())].join('');
    const time = [pad(date.getHours()), pad(date.getMinutes()), pad(date.getSeconds())].join('');

    return `catime-tray-icons-${day}-${time}.zip`;
}

export function moveItem(items, fromIndex, toIndex) {
    if (!Array.isArray(items)) return [];
    if (fromIndex < 0 || fromIndex >= items.length) return [...items];

    const destination = Math.max(0, Math.min(toIndex, items.length - 1));
    if (destination === fromIndex) return [...items];

    const result = [...items];
    const [item] = result.splice(fromIndex, 1);
    result.splice(destination, 0, item);
    return result;
}

export function formatBytes(bytes) {
    if (!Number.isFinite(bytes) || bytes <= 0) return '0 B';

    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    const unitIndex = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
    const value = bytes / (1024 ** unitIndex);
    const precision = value >= 100 || unitIndex === 0 ? 0 : value >= 10 ? 1 : 2;

    return `${value.toFixed(precision)} ${units[unitIndex]}`;
}

export function totalFileSize(items) {
    return items.reduce((total, item) => total + (Number(item?.file?.size) || 0), 0);
}
