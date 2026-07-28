import assert from 'node:assert/strict';
import test from 'node:test';
import { createSecureRandomOrder } from './secure-random-order.js';

test('assigns stable random ranks while allowing every author position', () => {
    const values = [3, 1, 2];
    const orderAuthors = createSecureRandomOrder(
        author => author.name,
        words => words.fill(values.shift()),
    );
    const firstPayload = [{ name: 'eirna' }, { name: 'YM722' }];
    const secondPayload = [{ name: 'new-author' }, ...firstPayload];

    assert.deepEqual(orderAuthors(firstPayload).map(author => author.name), ['YM722', 'eirna']);
    assert.deepEqual(
        orderAuthors(secondPayload).map(author => author.name),
        ['YM722', 'new-author', 'eirna'],
    );
    assert.deepEqual(firstPayload.map(author => author.name), ['eirna', 'YM722']);
});

test('redraws a cryptographic rank if two authors receive the same value', () => {
    const values = [7, 7, 9];
    let draws = 0;
    const orderAuthors = createSecureRandomOrder(
        author => author,
        words => {
            draws += 1;
            words.fill(values.shift());
        },
    );

    assert.deepEqual(orderAuthors(['first', 'second']), ['first', 'second']);
    assert.equal(draws, 3);
});
