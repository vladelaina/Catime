 #include "markdown_renderer_internal.h"
 #include <string.h>
void InitBaseFontState(HDC hdc, HFONT* originalFont, LOGFONT* baseFont, int* baseFontHeight) {
    if (originalFont) {
        *originalFont = NULL;
    }
    if (baseFont) {
        memset(baseFont, 0, sizeof(*baseFont));
    }
    if (baseFontHeight) {
        *baseFontHeight = 16;
    }
    if (!hdc || !baseFont || !baseFontHeight) return;

    TEXTMETRIC tm;
    if (GetTextMetrics(hdc, &tm) && tm.tmHeight > 0) {
        *baseFontHeight = tm.tmHeight;
    }

    HFONT hCurrentFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    if (originalFont) {
        *originalFont = hCurrentFont;
    }

    if (hCurrentFont && GetObject(hCurrentFont, sizeof(*baseFont), baseFont) == sizeof(*baseFont)) {
        if (baseFont->lfHeight != 0) {
            *baseFontHeight = baseFont->lfHeight;
        }
        return;
    }

    HFONT hDefaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (hDefaultFont && GetObject(hDefaultFont, sizeof(*baseFont), baseFont) == sizeof(*baseFont)) {
        if (baseFont->lfHeight != 0) {
            *baseFontHeight = baseFont->lfHeight;
        }
        return;
    }

    baseFont->lfHeight = *baseFontHeight;
    baseFont->lfWeight = FW_NORMAL;
    wcscpy_s(baseFont->lfFaceName, LF_FACESIZE, L"Segoe UI");
}

HFONT GetCachedMarkdownFont(MarkdownFontCache* cache, const LOGFONT* baseFont,
                                   int height, int weight, BOOL italic, BOOL monospace) {
    if (!cache || !baseFont) return NULL;

    BYTE italicByte = (BYTE)(italic ? 1 : 0);

    for (int i = 0; i < cache->count; i++) {
        const MarkdownFontCacheEntry* entry = &cache->entries[i];
        if (entry->height == height &&
            entry->weight == weight &&
            entry->italic == italicByte &&
            entry->monospace == monospace) {
            return entry->font;
        }
    }

    if (cache->count >= MARKDOWN_FONT_CACHE_CAPACITY) {
        return NULL;
    }

    LOGFONT lf;
    memcpy(&lf, baseFont, sizeof(LOGFONT));
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfItalic = italicByte;

    if (monospace) {
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Consolas");
    }

    HFONT font = CreateFontIndirect(&lf);
    if (!font) {
        return NULL;
    }

    MarkdownFontCacheEntry* entry = &cache->entries[cache->count++];
    entry->height = height;
    entry->weight = weight;
    entry->italic = italicByte;
    entry->monospace = monospace;
    entry->font = font;
    return font;
}

void ReleaseMarkdownFontCache(MarkdownFontCache* cache) {
    if (!cache) return;

    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].font) {
            DeleteObject(cache->entries[i].font);
            cache->entries[i].font = NULL;
        }
    }
    cache->count = 0;
}
