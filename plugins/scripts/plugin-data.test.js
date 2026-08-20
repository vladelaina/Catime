import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import test from 'node:test';
import {
    fetchVerifiedPlugin,
    normalizeCatalog,
} from './plugin-data.js';

function catalogFixture() {
    return {
        schemaVersion: 1,
        generatedAt: '2026-08-15T00:00:00.000Z',
        source: {
            repository: 'https://github.com/example/plugins',
            commit: '0123456789abcdef0123456789abcdef01234567',
        },
        count: 2,
        plugins: [
            pluginFixture(),
            {
                ...pluginFixture(),
                id: 'python_stats',
                filename: 'python_stats.py',
            },
        ],
    };
}

function pluginFixture() {
    return {
        id: 'hello_world',
        filename: 'hello_world.bat',
        posterUrl: 'https://example.com/poster.webp',
        previewUrl: 'https://example.com/preview.webp',
        downloadUrl: 'https://example.github.io/files/hello_world.bat?v=abc',
        size: 5,
        sha256: createHash('sha256').update('hello').digest('hex'),
    };
}

test('normalizes the versioned plugin catalog', () => {
    const catalog = normalizeCatalog(catalogFixture());
    assert.equal(catalog.count, 2);
    assert.equal(catalog.plugins[0].id, 'hello_world');
    assert.equal(catalog.plugins[0].posterUrl, 'https://example.com/poster.webp');
    assert.equal(catalog.plugins[1].filename, 'python_stats.py');
});

test('rejects mismatched counts and insecure download URLs', () => {
    const countMismatch = catalogFixture();
    countMismatch.count = 3;
    assert.throws(() => normalizeCatalog(countMismatch), /count/);

    const insecure = catalogFixture();
    insecure.plugins[0].downloadUrl = 'http://example.com/plugin.bat';
    assert.throws(() => normalizeCatalog(insecure), /HTTPS/);
});

test('downloads only bytes matching the catalog digest and size', async () => {
    const plugin = normalizeCatalog(catalogFixture()).plugins[0];
    const response = () => Promise.resolve(new Response('hello', {
        status: 200,
        headers: { 'Content-Length': '5' },
    }));
    const blob = await fetchVerifiedPlugin(plugin, response);
    assert.equal(await blob.text(), 'hello');

    const changedResponse = () => Promise.resolve(new Response('HELLO', { status: 200 }));
    await assert.rejects(() => fetchVerifiedPlugin(plugin, changedResponse), /SHA-256/);
});
