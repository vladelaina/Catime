import { createSecureRandomOrder } from './secure-random-order.js';

export function createAuthorWorkOrder(fillRandom) {
    const orderWorks = createSecureRandomOrder(workKey, fillRandom);

    return author => orderWorks(author.items.flatMap(collection => (
        Array.from({ length: collection.count }, (_, offset) => ({
            collection,
            index: offset + 1,
        }))
    )));
}

function workKey({ collection, index }) {
    return `${collection.key}\0${collection.files[index - 1] || index}`;
}
