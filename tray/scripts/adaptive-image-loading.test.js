import assert from 'node:assert/strict';
import test from 'node:test';
import { createPreviewLoader, resolveMotionPolicy } from './adaptive-image-loading.js';

test('adapts animated previews to motion preferences and network limits', () => {
    assert.deepEqual(resolveMotionPolicy({ reducedMotion: true }), {
        automatic: false, manual: false, concurrency: 1,
    });
    assert.deepEqual(resolveMotionPolicy({ saveData: true, effectiveType: '4g' }), {
        automatic: false, manual: true, concurrency: 1,
    });
    assert.deepEqual(resolveMotionPolicy({ effectiveType: '2g' }), {
        automatic: false, manual: true, concurrency: 1,
    });
    assert.deepEqual(resolveMotionPolicy({ effectiveType: '3g' }), {
        automatic: true, manual: true, concurrency: 1,
    });
    assert.deepEqual(resolveMotionPolicy({ effectiveType: '4g' }), {
        automatic: true, manual: true, concurrency: 2,
    });
});

test('deduplicates previews, respects concurrency, and promotes priority work', async () => {
    const started = [];
    const completions = new Map();
    const loader = createPreviewLoader({
        concurrency: 1,
        loadPreview: url => new Promise(resolve => {
            started.push(url);
            completions.set(url, resolve);
        }),
    });

    const first = loader.request('first.webp');
    const duplicate = loader.request('first.webp');
    const second = loader.request('second.webp');
    const priority = loader.request('priority.webp', { priority: true });
    assert.equal(first, duplicate);
    assert.deepEqual(started, ['first.webp']);

    completions.get('first.webp')(true);
    assert.equal(await first, true);
    await Promise.resolve();
    assert.deepEqual(started, ['first.webp', 'priority.webp']);

    completions.get('priority.webp')(true);
    assert.equal(await priority, true);
    await Promise.resolve();
    assert.deepEqual(started, ['first.webp', 'priority.webp', 'second.webp']);

    completions.get('second.webp')(true);
    assert.equal(await second, true);
});
