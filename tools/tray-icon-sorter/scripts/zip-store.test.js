import test from 'node:test';
import assert from 'node:assert/strict';

import { createZipBlob, crc32 } from './zip-store.js';

const decoder = new TextDecoder();

test('CRC-32 matches the standard reference value', () => {
    const bytes = new TextEncoder().encode('123456789');
    assert.equal(crc32(bytes), 0xcbf43926);
});

test('ZIP exporter creates valid stored entries and a central directory', async () => {
    const progress = [];
    const files = [
        { name: '1.png', blob: new Blob(['first']), lastModified: Date.UTC(2026, 0, 1) },
        { name: '2.webp', blob: new Blob(['second']), lastModified: Date.UTC(2026, 0, 2) },
    ];
    const zip = await createZipBlob(files, {
        onProgress: update => progress.push(update.percent),
    });
    const bytes = new Uint8Array(await zip.arrayBuffer());
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

    let offset = 0;
    for (const file of files) {
        assert.equal(view.getUint32(offset, true), 0x04034b50);
        assert.equal(view.getUint16(offset + 8, true), 0);

        const compressedSize = view.getUint32(offset + 18, true);
        const filenameLength = view.getUint16(offset + 26, true);
        const filenameStart = offset + 30;
        const dataStart = filenameStart + filenameLength;
        const filename = decoder.decode(bytes.subarray(filenameStart, dataStart));
        const contents = decoder.decode(bytes.subarray(dataStart, dataStart + compressedSize));

        assert.equal(filename, file.name);
        assert.equal(contents, await file.blob.text());
        offset = dataStart + compressedSize;
    }

    const endOffset = bytes.length - 22;
    assert.equal(view.getUint32(endOffset, true), 0x06054b50);
    assert.equal(view.getUint16(endOffset + 10, true), files.length);
    assert.equal(view.getUint32(endOffset + 16, true), offset);
    assert.equal(view.getUint32(offset, true), 0x02014b50);
    assert.equal(progress.at(-1), 100);
    assert.equal(zip.type, 'application/zip');
});
