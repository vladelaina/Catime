/**
 * @file drawing_text_stb_metrics.c
 * @brief Glyph metrics and visibility decisions.
 */

#include "drawing_text_stb_internal.h"

void GetCharMetricsSTB(wchar_t c, wchar_t nextC, float scale, float fallbackScale, GlyphMetrics* out) {
    if (!out) return;

    out->index = 0;
    out->isFallback = FALSE;
    out->advance = 0;
    out->kern = 0;

    if (c == L'\n' || c == L'\r') return;

    wchar_t cacheNextC = (nextC == L'\n' || nextC == L'\r') ? 0 : nextC;
    if (c == L'\t') {
        cacheNextC = 0;
    }

    DWORD cacheSlot = GetGlyphMetricsCacheSlot(c, cacheNextC);
    GlyphMetricsCacheEntry* cached = &g_glyphMetricsCache[cacheSlot];
    if (cached->valid && cached->c == c && cached->nextC == cacheNextC) {
        ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
        return;
    }

    if (c == L'\t') {
        // Tab = 4 spaces
        int spaceIdx = stbtt_FindGlyphIndex(&g_fontInfo, ' ');
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&g_fontInfo, spaceIdx, &adv, &lsb);
        cached->valid = TRUE;
        cached->c = c;
        cached->nextC = cacheNextC;
        cached->index = spaceIdx;
        cached->isFallback = FALSE;
        cached->advanceUnits = adv * 4;
        cached->kernUnits = 0;
        ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
        return;
    }

    out->index = stbtt_FindGlyphIndex(&g_fontInfo, (int)c);

    if (out->index == 0 && g_fallbackFontLoaded && c != L' ') {
        int fallbackIndex = stbtt_FindGlyphIndex(&g_fallbackFontInfo, (int)c);
        if (fallbackIndex != 0) {
            out->index = fallbackIndex;
            out->isFallback = TRUE;
        }
    }

    int adv = 0;
    int lsb = 0;
    int kern = 0;
    if (out->isFallback) {
        stbtt_GetGlyphHMetrics(&g_fallbackFontInfo, out->index, &adv, &lsb);
    } else {
        stbtt_GetGlyphHMetrics(&g_fontInfo, out->index, &adv, &lsb);

        // Kerning
        if (cacheNextC) {
            int nextIdx = stbtt_FindGlyphIndex(&g_fontInfo, (int)cacheNextC);
            if (nextIdx != 0) {
                kern = stbtt_GetGlyphKernAdvance(&g_fontInfo, out->index, nextIdx);
            }
        }
    }

    cached->valid = TRUE;
    cached->c = c;
    cached->nextC = cacheNextC;
    cached->index = out->index;
    cached->isFallback = out->isFallback;
    cached->advanceUnits = adv;
    cached->kernUnits = kern;
    ApplyCachedGlyphMetrics(cached, scale, fallbackScale, out);
}

BOOL GetCachedFontCharMetricsSTB(const stbtt_fontinfo* fontInfo,
                                 wchar_t c,
                                 float scale,
                                 GlyphMetrics* out) {
    if (!fontInfo || !out) return FALSE;

    out->index = 0;
    out->isFallback = FALSE;
    out->advance = 0;
    out->kern = 0;

    int fontSlot = -1;
    for (int i = 0; i < MAX_CACHED_FONTS; ++i) {
        if (g_fontCache[i].isLoaded && fontInfo == &g_fontCache[i].fontInfo) {
            fontSlot = i;
            break;
        }
    }
    if (fontSlot < 0) {
        return FALSE;
    }

    DWORD cacheSlot = GetFontTagGlyphMetricsCacheSlot(c);
    FontTagGlyphMetricsCacheEntry* cached =
        &g_fontTagGlyphMetricsCache[fontSlot][cacheSlot];
    if (cached->valid && cached->c == c) {
        out->index = cached->index;
        out->advance = ScaleTextMetricClamped(cached->advanceUnits, scale);
        return TRUE;
    }

    int glyphIndex = stbtt_FindGlyphIndex(fontInfo, (int)c);
    int advanceUnits = 0;
    if (glyphIndex != 0) {
        int lsb = 0;
        stbtt_GetGlyphHMetrics(fontInfo, glyphIndex, &advanceUnits, &lsb);
    }

    cached->valid = TRUE;
    cached->c = c;
    cached->index = glyphIndex;
    cached->advanceUnits = advanceUnits;

    out->index = glyphIndex;
    out->advance = ScaleTextMetricClamped(advanceUnits, scale);
    return TRUE;
}

BOOL IsGlyphBitmapVisibleSTB(const stbtt_fontinfo* fontInfo,
                             int glyphIndex,
                             float scaleX,
                             float scaleY,
                             int originX,
                             int originY,
                             int destWidth,
                             int destHeight,
                             EffectType effect,
                             int extraMargin) {
    if (!fontInfo || glyphIndex == 0 || destWidth <= 0 || destHeight <= 0) {
        return FALSE;
    }

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(fontInfo, glyphIndex, scaleX, scaleY, &x0, &y0, &x1, &y1);
    if (x1 <= x0 || y1 <= y0) {
        return FALSE;
    }

    int margin = extraMargin;
    if (effect != EFFECT_TYPE_NONE) {
        margin += 24;
    }

    int left = AddTextIntClamped(AddTextIntClamped(originX, x0), -margin);
    int top = AddTextIntClamped(AddTextIntClamped(originY, y0), -margin);
    int right = AddTextIntClamped(AddTextIntClamped(originX, x1), margin);
    int bottom = AddTextIntClamped(AddTextIntClamped(originY, y1), margin);

    return right > 0 && bottom > 0 && left < destWidth && top < destHeight;
}
