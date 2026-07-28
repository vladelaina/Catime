import assert from 'node:assert/strict';
import test from 'node:test';
import { animationFilename, normalizeLibrary } from './library-data.js';

test('uses the repository key, explicit files, avatar, and README links', () => {
    const library = normalizeLibrary({
        generated: '2026-07-29T00:00:00.000Z',
        sections: {
            eirna: {
                author: 'ignored legacy override',
                authorAvatar: 'https://tray.example/avatars/eirna/a.webp',
                authorLinks: [{ label: 'Bilibili', url: 'https://space.bilibili.com/1195508399' }],
                cdnBase: 'https://tray.example/assets/eirna/',
                files: ['1.gif', '2.png'],
                fileVersions: ['abc', 'def'],
            },
        },
    });

    assert.equal(library.authors[0].name, 'eirna');
    assert.equal(library.authors[0].avatar, 'https://tray.example/avatars/eirna/a.webp');
    assert.deepEqual(library.authors[0].links, [
        { label: 'Bilibili', url: 'https://space.bilibili.com/1195508399' },
    ]);
    assert.equal(library.collections[0].count, 2);
    assert.equal(animationFilename(library.collections[0], 2), '2.png');
});

test('does not revive collections through legacy count or filename conventions', () => {
    const library = normalizeLibrary({
        sections: {
            legacy: {
                creator: 'legacy author',
                avatar: '/legacy.webp',
                count: 24,
            },
        },
    });

    assert.deepEqual(library.collections, []);
    assert.throws(
        () => animationFilename({ key: 'eirna', files: [] }, 1),
        /does not exist/,
    );
});
