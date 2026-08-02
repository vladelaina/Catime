const RANDOM_RANK_WORDS = 4;

export function createSecureRandomOrder(keyForItem = item => item, fillRandom = fillWithWebCrypto) {
    const ranks = new Map();
    const usedRanks = new Set();

    return items => {
        const ordered = [...items];
        ordered.forEach(item => {
            const key = String(keyForItem(item));
            if (!ranks.has(key)) ranks.set(key, createUniqueRank(usedRanks, fillRandom));
        });
        return ordered.sort((left, right) => {
            const leftRank = ranks.get(String(keyForItem(left)));
            const rightRank = ranks.get(String(keyForItem(right)));
            return leftRank < rightRank ? -1 : leftRank > rightRank ? 1 : 0;
        });
    };
}

function createUniqueRank(usedRanks, fillRandom) {
    const words = new Uint32Array(RANDOM_RANK_WORDS);
    let rank;
    do {
        fillRandom(words);
        rank = [...words].map(word => word.toString(16).padStart(8, '0')).join('');
    } while (usedRanks.has(rank));
    usedRanks.add(rank);
    return rank;
}

function fillWithWebCrypto(words) {
    if (!globalThis.crypto?.getRandomValues) {
        throw new Error('Secure random ordering requires Web Crypto');
    }
    globalThis.crypto.getRandomValues(words);
}
