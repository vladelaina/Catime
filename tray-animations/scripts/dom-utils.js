const AUTHOR_COLORS = ['#f77daa', '#7aa2f7', '#bb9af7', '#2ac3de', '#ff9e64', '#9ece6a'];

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
