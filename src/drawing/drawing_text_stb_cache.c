/**
 * @file drawing_text_stb_cache.c
 * @brief Glyph and cached-font bookkeeping helpers.
 */

#include "drawing_text_stb_internal.h"

DWORD GetGlyphMetricsCacheSlot(wchar_t c, wchar_t nextC) {
    DWORD hash = (DWORD)c * 2654435761u;
    hash ^= ((DWORD)nextC * 2246822519u) + (hash << 6) + (hash >> 2);
    return hash & (GLYPH_METRICS_CACHE_SIZE - 1);
}

DWORD FloatBitsForGlyphCache(float value) {
    DWORD bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

DWORD PointerBitsForGlyphCache(const void* ptr) {
    uintptr_t value = (uintptr_t)ptr;
    DWORD hash = (DWORD)value;
#if defined(_WIN64)
    hash ^= (DWORD)(((uint64_t)value) >> 32);
#endif
    return hash;
}

DWORD GetGlyphBitmapCacheSlot(const stbtt_fontinfo* fontInfo,
                                     int glyphIndex,
                                     DWORD scaleXBits,
                                     DWORD scaleYBits,
                                     int width,
                                     int height,
                                     int xoff,
                                     int yoff) {
    DWORD hash = PointerBitsForGlyphCache(fontInfo);
    hash ^= (DWORD)glyphIndex * 2654435761u;
    hash ^= scaleXBits * 2246822519u;
    hash ^= scaleYBits * 3266489917u;
    hash ^= (DWORD)width * 668265263u;
    hash ^= (DWORD)height * 374761393u;
    hash ^= (DWORD)xoff * 1274126177u;
    hash ^= (DWORD)yoff * 974142619u;
    return hash & (GLYPH_BITMAP_CACHE_SIZE - 1);
}

BOOL GlyphBitmapCacheEntryMatches(const GlyphBitmapCacheEntry* entry,
                                         const stbtt_fontinfo* fontInfo,
                                         DWORD generation,
                                         int glyphIndex,
                                         DWORD scaleXBits,
                                         DWORD scaleYBits,
                                         int width,
                                         int height,
                                         int xoff,
                                         int yoff,
                                         size_t pixelCount) {
    return entry && entry->valid &&
           entry->fontInfo == fontInfo &&
           entry->generation == generation &&
           entry->glyphIndex == glyphIndex &&
           entry->scaleXBits == scaleXBits &&
           entry->scaleYBits == scaleYBits &&
           entry->width == width &&
           entry->height == height &&
           entry->xoff == xoff &&
           entry->yoff == yoff &&
           entry->pixelCount == pixelCount &&
           entry->pixels != NULL;
}

unsigned char* CopyCachedGlyphBitmapLocked(const stbtt_fontinfo* fontInfo,
                                                  DWORD generation,
                                                  int glyphIndex,
                                                  DWORD scaleXBits,
                                                  DWORD scaleYBits,
                                                  int width,
                                                  int height,
                                                  int xoff,
                                                  int yoff,
                                                  size_t pixelCount) {
    if (pixelCount == 0 || pixelCount > GLYPH_BITMAP_CACHE_MAX_BYTES) {
        return NULL;
    }

    DWORD slot = GetGlyphBitmapCacheSlot(fontInfo, glyphIndex,
                                         scaleXBits, scaleYBits,
                                         width, height, xoff, yoff);
    GlyphBitmapCacheEntry* entry = &g_glyphBitmapCache[slot];
    if (!GlyphBitmapCacheEntryMatches(entry, fontInfo, generation, glyphIndex,
                                      scaleXBits, scaleYBits,
                                      width, height, xoff, yoff, pixelCount)) {
        return NULL;
    }

    unsigned char* copy = (unsigned char*)malloc(pixelCount);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, entry->pixels, pixelCount);
    entry->lastUse = ++g_glyphBitmapCacheUseCounter;
    return copy;
}

void StoreGlyphBitmapCacheLocked(const stbtt_fontinfo* fontInfo,
                                        DWORD generation,
                                        int glyphIndex,
                                        DWORD scaleXBits,
                                        DWORD scaleYBits,
                                        int width,
                                        int height,
                                        int xoff,
                                        int yoff,
                                        const unsigned char* pixels,
                                        size_t pixelCount) {
    if (!pixels || pixelCount == 0 || pixelCount > GLYPH_BITMAP_CACHE_MAX_BYTES) {
        return;
    }

    DWORD slot = GetGlyphBitmapCacheSlot(fontInfo, glyphIndex,
                                         scaleXBits, scaleYBits,
                                         width, height, xoff, yoff);
    GlyphBitmapCacheEntry* entry = &g_glyphBitmapCache[slot];

    unsigned char* cachedPixels = (unsigned char*)malloc(pixelCount);
    if (!cachedPixels) {
        return;
    }
    memcpy(cachedPixels, pixels, pixelCount);

    free(entry->pixels);
    entry->pixels = cachedPixels;
    entry->valid = TRUE;
    entry->fontInfo = fontInfo;
    entry->generation = generation;
    entry->glyphIndex = glyphIndex;
    entry->scaleXBits = scaleXBits;
    entry->scaleYBits = scaleYBits;
    entry->width = width;
    entry->height = height;
    entry->xoff = xoff;
    entry->yoff = yoff;
    entry->pixelCount = pixelCount;
    entry->lastUse = ++g_glyphBitmapCacheUseCounter;
}

DWORD GetFontTagGlyphMetricsCacheSlot(wchar_t c) {
    return ((DWORD)c * 2654435761u) & (FONT_TAG_GLYPH_METRICS_CACHE_SIZE - 1);
}

void ClearFontTagGlyphMetricsCacheSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return;
    ZeroMemory(g_fontTagGlyphMetricsCache[slot],
               sizeof(g_fontTagGlyphMetricsCache[slot]));
}

void CompactFontCacheLRULocked(void) {
    BOOL used[MAX_CACHED_FONTS] = {0};
    int compactedCount = 0;

    for (int rank = 1; rank <= MAX_CACHED_FONTS; ++rank) {
        int nextSlot = -1;
        int nextLRU = INT_MAX;

        for (int i = 0; i < MAX_CACHED_FONTS; ++i) {
            if (!g_fontCache[i].isLoaded || used[i]) continue;
            if (nextSlot < 0 || g_fontCacheLRU[i] < nextLRU) {
                nextSlot = i;
                nextLRU = g_fontCacheLRU[i];
            }
        }

        if (nextSlot < 0) break;

        used[nextSlot] = TRUE;
        g_fontCacheLRU[nextSlot] = rank;
        compactedCount = rank;
    }

    g_fontCacheAccessCounter = compactedCount;
}

int TouchFontCacheSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return 0;

    if (g_fontCacheAccessCounter >= INT_MAX - MAX_CACHED_FONTS) {
        CompactFontCacheLRULocked();
    }

    g_fontCacheLRU[slot] = ++g_fontCacheAccessCounter;
    return g_fontCacheLRU[slot];
}

int ScaleTextMetricClamped(int metric, float scale) {
    double scaled = (double)metric * (double)scale;
    if (!isfinite(scaled)) {
        return scaled < 0.0 ? INT_MIN : INT_MAX;
    }
    if (scaled > (double)INT_MAX) return INT_MAX;
    if (scaled < (double)INT_MIN) return INT_MIN;
    return (int)scaled;
}

void ApplyCachedGlyphMetrics(const GlyphMetricsCacheEntry* entry,
                                    float scale,
                                    float fallbackScale,
                                    GlyphMetrics* out) {
    out->index = entry->index;
    out->isFallback = entry->isFallback;
    out->advance = ScaleTextMetricClamped(entry->advanceUnits,
                                          entry->isFallback ? fallbackScale : scale);
    out->kern = ScaleTextMetricClamped(entry->kernUnits, scale);
}
