const AUTHOR_COLORS = ['#111318', '#ff3b30', '#6d5dfc', '#0f8b8d', '#cf6a27', '#3d6b45'];

export function colorForIndex(index) {
    return AUTHOR_COLORS[index % AUTHOR_COLORS.length];
}

export function escapeHtml(value) {
    return String(value).replace(/[&<>'"]/g, character => ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        "'": '&#39;',
        '"': '&quot;',
    }[character]));
}

export const escapeAttribute = escapeHtml;
