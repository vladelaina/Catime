export async function loadLibraryData(source = 'sections.json') {
    const response = await fetch(source, { cache: 'no-store' });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);

    const payload = await response.json();
    const collections = Object.entries(payload.sections || {})
        .map(([key, data]) => normalizeCollection(key, data))
        .filter(collection => collection.count > 0);

    return {
        collections,
        authors: groupByAuthor(collections),
    };
}

function normalizeCollection(key, data) {
    return {
        key,
        title: data.title || key,
        author: String(data.author || data.creator || data.contributor || data.title || key).trim(),
        authorBio: data.authorBio || data.bio || '',
        authorAvatar: data.authorAvatar || data.avatar || '',
        authorUrl: data.authorUrl || data.creatorUrl || '',
        authorTag: data.authorTag || data.tag || '',
        rating: Number(data.rating) || 0,
        reviewCount: Number(data.reviewCount) || 0,
        description: data.description || '',
        count: Number(data.count) || 0,
        cdnBase: data.cdnBase || `./gifs/${key}/`,
        repository: data.repository || '',
    };
}

function groupByAuthor(collections) {
    const groups = new Map();

    collections.forEach(collection => {
        if (!groups.has(collection.author)) groups.set(collection.author, []);
        groups.get(collection.author).push(collection);
    });

    return [...groups.entries()].map(([name, items]) => ({
        name,
        items,
        total: items.reduce((sum, item) => sum + item.count, 0),
        bio: items.find(item => item.authorBio)?.authorBio || '',
        avatar: items.find(item => item.authorAvatar)?.authorAvatar || '',
        url: items.find(item => item.authorUrl)?.authorUrl || '',
        tag: items.find(item => item.authorTag)?.authorTag || '',
        rating: items.find(item => item.rating)?.rating || 0,
        reviewCount: items.reduce((sum, item) => sum + item.reviewCount, 0),
    }));
}

export function animationFilename(collection, index) {
    return `${String(index).padStart(4, '0')}_${collection.key}.gif`;
}

export function animationUrl(collection, index) {
    return `${collection.cdnBase}${animationFilename(collection, index)}`;
}
