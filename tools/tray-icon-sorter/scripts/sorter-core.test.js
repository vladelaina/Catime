import test from 'node:test';
import assert from 'node:assert/strict';

import {
    createNumberedFilename,
    createSequentialEntries,
    formatBytes,
    getFileExtension,
    isImageFile,
    moveItem,
    totalFileSize,
} from './sorter-core.js';

test('image files keep their original extension while receiving sequential names', () => {
    const png = { name: 'cat.frame.PNG', type: 'image/png', size: 10, lastModified: 1 };
    const webp = { name: 'another-image.webp', type: 'image/webp', size: 20, lastModified: 2 };

    assert.equal(getFileExtension(png), 'png');
    assert.equal(createNumberedFilename(png, 1), '1.png');
    assert.deepEqual(createSequentialEntries([{ file: png }, { file: webp }]).map(entry => entry.name), [
        '1.png',
        '2.webp',
    ]);
});

test('image detection supports MIME types and common image extensions', () => {
    assert.equal(isImageFile({ name: 'clipboard', type: 'image/png' }), true);
    assert.equal(isImageFile({ name: 'legacy.ico', type: '' }), true);
    assert.equal(isImageFile({ name: 'notes.txt', type: 'text/plain' }), false);
});

test('moving an item returns the expected order without mutating the input', () => {
    const original = ['one', 'two', 'three', 'four'];
    const moved = moveItem(original, 0, 2);

    assert.deepEqual(moved, ['two', 'three', 'one', 'four']);
    assert.deepEqual(original, ['one', 'two', 'three', 'four']);
});

test('file totals and byte labels are stable', () => {
    const items = [{ file: { size: 1024 } }, { file: { size: 1536 } }];

    assert.equal(totalFileSize(items), 2560);
    assert.equal(formatBytes(0), '0 B');
    assert.equal(formatBytes(1536), '1.50 KB');
});

test('numbering rejects positions below one', () => {
    assert.throws(() => createNumberedFilename({ name: 'frame.png' }, 0), RangeError);
});
