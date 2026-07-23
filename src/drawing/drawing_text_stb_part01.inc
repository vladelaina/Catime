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

static unsigned char* CopyCachedGlyphBitmapLocked(const stbtt_fontinfo* fontInfo,
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

static void StoreGlyphBitmapCacheLocked(const stbtt_fontinfo* fontInfo,
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

static DWORD GetFontTagGlyphMetricsCacheSlot(wchar_t c) {
    return ((DWORD)c * 2654435761u) & (FONT_TAG_GLYPH_METRICS_CACHE_SIZE - 1);
}

static void ClearFontTagGlyphMetricsCacheSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return;
    ZeroMemory(g_fontTagGlyphMetricsCache[slot],
               sizeof(g_fontTagGlyphMetricsCache[slot]));
}

static void CompactFontCacheLRULocked(void) {
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

static int TouchFontCacheSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return 0;

    if (g_fontCacheAccessCounter >= INT_MAX - MAX_CACHED_FONTS) {
        CompactFontCacheLRULocked();
    }

    g_fontCacheLRU[slot] = ++g_fontCacheAccessCounter;
    return g_fontCacheLRU[slot];
}

static int ScaleTextMetricClamped(int metric, float scale) {
    double scaled = (double)metric * (double)scale;
    if (!isfinite(scaled)) {
        return scaled < 0.0 ? INT_MIN : INT_MAX;
    }
    if (scaled > (double)INT_MAX) return INT_MAX;
    if (scaled < (double)INT_MIN) return INT_MIN;
    return (int)scaled;
}

static void ApplyCachedGlyphMetrics(const GlyphMetricsCacheEntry* entry,
                                    float scale,
                                    float fallbackScale,
                                    GlyphMetrics* out) {
    out->index = entry->index;
    out->isFallback = entry->isFallback;
    out->advance = ScaleTextMetricClamped(entry->advanceUnits,
                                          entry->isFallback ? fallbackScale : scale);
    out->kern = ScaleTextMetricClamped(entry->kernUnits, scale);
}

BOOL BeginFontUseSTB(void) {
    if (!InitOnceExecuteOnce(&g_fontStateLockOnce, InitFontStateLock, NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_fontStateCS);
    return TRUE;
}

void EndFontUseSTB(void) {
    LeaveCriticalSection(&g_fontStateCS);
}

DWORD GetFontStateGenerationSTB(void) {
    return (DWORD)InterlockedCompareExchange(&g_fontStateGeneration, 0, 0);
}

static void ReleaseMappedFont(unsigned char* buffer, HANDLE hFile, HANDLE hMapping) {
    if (buffer) {
        UnmapViewOfFile(buffer);
    }
    if (hMapping) {
        CloseHandle(hMapping);
    }
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }
}

static BOOL InitFontInfoFromBuffer(stbtt_fontinfo* fontInfo,
                                   const unsigned char* buffer,
                                   const char* pathForLog) {
    if (!fontInfo || !buffer) return FALSE;

    int fontOffset = stbtt_GetFontOffsetForIndex(buffer, 0);
    if (fontOffset < 0) {
        LOG_WARNING("Font has no usable face at index 0: %s",
                    pathForLog ? pathForLog : "(unknown)");
        return FALSE;
    }

    if (!stbtt_InitFont(fontInfo, buffer, fontOffset)) {
        LOG_WARNING("Failed to init STB font: %s",
                    pathForLog ? pathForLog : "(unknown)");
        return FALSE;
    }

    return TRUE;
}

static BOOL InitFontInfoFromBufferW(stbtt_fontinfo* fontInfo,
                                    const unsigned char* buffer,
                                    const wchar_t* pathForLog) {
    if (!fontInfo || !buffer) return FALSE;

    int fontOffset = stbtt_GetFontOffsetForIndex(buffer, 0);
    if (fontOffset < 0) {
        LOG_WARNING("Font has no usable face at index 0: %ls",
                    pathForLog ? pathForLog : L"(unknown)");
        return FALSE;
    }

    if (!stbtt_InitFont(fontInfo, buffer, fontOffset)) {
        LOG_WARNING("Failed to init STB font: %ls",
                    pathForLog ? pathForLog : L"(unknown)");
        return FALSE;
    }

    return TRUE;
}

static BOOL CalculateBitmapPixelCount(int width, int height, size_t* outPixelCount) {
    if (!outPixelCount || width <= 0 || height <= 0) return FALSE;

    size_t sw = (size_t)width;
    size_t sh = (size_t)height;
    if (sw > (size_t)-1 / sh) return FALSE;

    *outPixelCount = sw * sh;
    return TRUE;
}

typedef struct {
    int srcLeft;
    int srcTop;
    int srcRight;
    int srcBottom;
    int destLeft;
    int destTop;
} TextBitmapClip;

static BOOL ClipTextBitmapToDestination(int x, int y,
                                        int bitmapWidth, int bitmapHeight,
                                        int destWidth, int destHeight,
                                        TextBitmapClip* clip) {
    if (!clip || bitmapWidth <= 0 || bitmapHeight <= 0 ||
        destWidth <= 0 || destHeight <= 0) {
