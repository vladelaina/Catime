const ZIP32_MAX_VALUE = 0xffffffff;
const ZIP32_MAX_FILES = 0xffff;
const UTF8_FLAG = 0x0800;
const STORE_METHOD = 0;
const textEncoder = new TextEncoder();
const CRC_TABLE = createCrcTable();

export async function createZipBlob(entries, options = {}) {
    if (!Array.isArray(entries) || entries.length === 0) {
        throw new Error('At least one file is required to create a ZIP archive.');
    }
    if (entries.length > ZIP32_MAX_FILES) {
        throw new Error('This ZIP archive contains too many files.');
    }

    const localParts = [];
    const centralParts = [];
    const metadata = [];
    let localOffset = 0;

    for (let index = 0; index < entries.length; index += 1) {
        const entry = entries[index];
        const blob = normalizeBlob(entry.blob);
        const filename = normalizeFilename(entry.name);
        const filenameBytes = textEncoder.encode(filename);

        if (filenameBytes.length > ZIP32_MAX_FILES) {
            throw new Error(`ZIP filename is too long: ${filename}`);
        }
        if (blob.size > ZIP32_MAX_VALUE) {
            throw new Error(`File is too large for a standard ZIP archive: ${filename}`);
        }

        const bytes = new Uint8Array(await blob.arrayBuffer());
        const checksum = crc32(bytes);
        const timestamp = toDosTimestamp(entry.lastModified);
        const header = createLocalHeader({
            checksum,
            size: blob.size,
            filenameLength: filenameBytes.length,
            ...timestamp,
        });

        if (localOffset + header.length + filenameBytes.length + blob.size > ZIP32_MAX_VALUE) {
            throw new Error('The ZIP archive is too large for the browser exporter.');
        }

        localParts.push(header, filenameBytes, blob);
        metadata.push({
            checksum,
            size: blob.size,
            filenameBytes,
            localOffset,
            ...timestamp,
        });
        localOffset += header.length + filenameBytes.length + blob.size;

        options.onProgress?.({
            completed: index + 1,
            total: entries.length,
            filename,
            percent: Math.round(((index + 1) / entries.length) * 88),
        });
    }

    metadata.forEach(entry => {
        centralParts.push(createCentralHeader(entry), entry.filenameBytes);
    });

    const centralSize = centralParts.reduce((total, part) => total + part.length, 0);
    const archiveSize = localOffset + centralSize + 22;
    if (archiveSize > ZIP32_MAX_VALUE) {
        throw new Error('The ZIP archive is too large for the browser exporter.');
    }

    const endRecord = createEndRecord(entries.length, centralSize, localOffset);
    options.onProgress?.({
        completed: entries.length,
        total: entries.length,
        filename: '',
        percent: 100,
    });

    return new Blob([...localParts, ...centralParts, endRecord], {
        type: 'application/zip',
    });
}

export function crc32(bytes) {
    let checksum = 0xffffffff;

    for (const byte of bytes) {
        checksum = CRC_TABLE[(checksum ^ byte) & 0xff] ^ (checksum >>> 8);
    }

    return (checksum ^ 0xffffffff) >>> 0;
}

function createCrcTable() {
    return Uint32Array.from({ length: 256 }, (_, index) => {
        let value = index;
        for (let bit = 0; bit < 8; bit += 1) {
            value = (value >>> 1) ^ ((value & 1) ? 0xedb88320 : 0);
        }
        return value >>> 0;
    });
}

function normalizeBlob(value) {
    if (value instanceof Blob) return value;
    return new Blob([value]);
}

function normalizeFilename(value) {
    const filename = String(value || '').replace(/\\/g, '/').split('/').pop();
    if (!filename) throw new Error('Every ZIP entry needs a filename.');
    return filename;
}

function toDosTimestamp(lastModified) {
    const date = new Date(Number(lastModified) || Date.now());
    const year = Math.max(1980, Math.min(date.getFullYear(), 2107));

    return {
        dosDate: ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate(),
        dosTime: (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2),
    };
}

function createLocalHeader(entry) {
    const bytes = new Uint8Array(30);
    const view = new DataView(bytes.buffer);

    view.setUint32(0, 0x04034b50, true);
    view.setUint16(4, 20, true);
    view.setUint16(6, UTF8_FLAG, true);
    view.setUint16(8, STORE_METHOD, true);
    view.setUint16(10, entry.dosTime, true);
    view.setUint16(12, entry.dosDate, true);
    view.setUint32(14, entry.checksum, true);
    view.setUint32(18, entry.size, true);
    view.setUint32(22, entry.size, true);
    view.setUint16(26, entry.filenameLength, true);
    view.setUint16(28, 0, true);

    return bytes;
}

function createCentralHeader(entry) {
    const bytes = new Uint8Array(46);
    const view = new DataView(bytes.buffer);

    view.setUint32(0, 0x02014b50, true);
    view.setUint16(4, 20, true);
    view.setUint16(6, 20, true);
    view.setUint16(8, UTF8_FLAG, true);
    view.setUint16(10, STORE_METHOD, true);
    view.setUint16(12, entry.dosTime, true);
    view.setUint16(14, entry.dosDate, true);
    view.setUint32(16, entry.checksum, true);
    view.setUint32(20, entry.size, true);
    view.setUint32(24, entry.size, true);
    view.setUint16(28, entry.filenameBytes.length, true);
    view.setUint16(30, 0, true);
    view.setUint16(32, 0, true);
    view.setUint16(34, 0, true);
    view.setUint16(36, 0, true);
    view.setUint32(38, 0, true);
    view.setUint32(42, entry.localOffset, true);

    return bytes;
}

function createEndRecord(entryCount, centralSize, centralOffset) {
    const bytes = new Uint8Array(22);
    const view = new DataView(bytes.buffer);

    view.setUint32(0, 0x06054b50, true);
    view.setUint16(4, 0, true);
    view.setUint16(6, 0, true);
    view.setUint16(8, entryCount, true);
    view.setUint16(10, entryCount, true);
    view.setUint32(12, centralSize, true);
    view.setUint32(16, centralOffset, true);
    view.setUint16(20, 0, true);

    return bytes;
}
