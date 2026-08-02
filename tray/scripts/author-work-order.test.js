import assert from 'node:assert/strict';
import test from 'node:test';
import { createAuthorWorkOrder } from './author-work-order.js';

test('randomizes all works for an author while keeping the order stable', () => {
    const values = [4, 1, 3, 2];
    const orderAuthorWorks = createAuthorWorkOrder(words => words.fill(values.shift()));
    const author = {
        items: [
            { key: 'first', count: 2, files: ['one.webp', 'two.webp'] },
            { key: 'second', count: 2, files: ['three.webp', 'four.webp'] },
        ],
    };

    const firstOrder = orderAuthorWorks(author);
    const refreshedOrder = orderAuthorWorks({
        items: author.items.map(collection => ({ ...collection })),
    });
    const labels = works => works.map(({ collection, index }) => collection.files[index - 1]);

    assert.deepEqual(labels(firstOrder), ['two.webp', 'four.webp', 'three.webp', 'one.webp']);
    assert.deepEqual(labels(refreshedOrder), labels(firstOrder));
    assert.deepEqual(author.items[0].files, ['one.webp', 'two.webp']);
});
