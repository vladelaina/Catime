/**
 * @file drawing_text_stb.c
 * @brief Implementation of text rendering using stb_truetype
 */

#include "drawing/drawing_text_stb.h"
#include "drawing/drawing_effect.h"
#include "menu_preview.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../libs/stb/stb_truetype.h"

/* Global font state */
static unsigned char* g_fontBuffer = NULL;
static stbtt_fontinfo g_fontInfo;
static char g_currentFontPath[MAX_PATH] = {0};
static FILETIME g_currentFontLastWriteTime = {0};
static ULONGLONG g_currentFontFileSize = 0;
static DWORD g_currentFontLastValidateTick = 0;
static BOOL g_currentFontFileInfoValid = FALSE;
static BOOL g_fontLoaded = FALSE;

/* Fallback font state (Segoe UI Emoji) */
static unsigned char* g_fallbackFontBuffer = NULL;
static stbtt_fontinfo g_fallbackFontInfo;
static BOOL g_fallbackFontLoaded = FALSE;

/* Memory mapping handles */
static HANDLE g_hFontFile = INVALID_HANDLE_VALUE;
static HANDLE g_hFontMapping = NULL;

/* Fallback memory mapping handles */
static HANDLE g_hFallbackFontFile = INVALID_HANDLE_VALUE;
static HANDLE g_hFallbackFontMapping = NULL;
static volatile LONG g_fontStateGeneration = 1;

/* Font cache for <font:> tags */
static CachedFont g_fontCache[MAX_CACHED_FONTS] = {0};
static int g_fontCacheLRU[MAX_CACHED_FONTS] = {0};  /* LRU counter for eviction */
static int g_fontCacheAccessCounter = 0;

#define FONT_TAG_GLYPH_METRICS_CACHE_SIZE 256

typedef struct {
    BOOL valid;
    wchar_t c;
    int index;
    int advanceUnits;
} FontTagGlyphMetricsCacheEntry;

static FontTagGlyphMetricsCacheEntry
    g_fontTagGlyphMetricsCache[MAX_CACHED_FONTS][FONT_TAG_GLYPH_METRICS_CACHE_SIZE] = {0};

#define MAX_FAILED_FONT_CACHE 256
#define FONT_FAILURE_RETRY_MS 5000
#define MAX_MAPPED_FONT_BYTES (64ull * 1024ull * 1024ull)
#define MAIN_FONT_FILE_RECHECK_MS 1000u

typedef struct {
    wchar_t fontName[MAX_PATH];
    DWORD retryAfterFailureTick;
} FailedFontCacheEntry;

static FailedFontCacheEntry g_failedFontCache[MAX_FAILED_FONT_CACHE] = {0};

#define GLYPH_METRICS_CACHE_SIZE 512

typedef struct {
    BOOL valid;
    wchar_t c;
    wchar_t nextC;
    int index;
    BOOL isFallback;
    int advanceUnits;
    int kernUnits;
} GlyphMetricsCacheEntry;

static GlyphMetricsCacheEntry g_glyphMetricsCache[GLYPH_METRICS_CACHE_SIZE] = {0};

#define GLYPH_BITMAP_CACHE_SIZE 32
#define GLYPH_BITMAP_CACHE_MAX_BYTES (256u * 1024u)

typedef struct {
    BOOL valid;
    const stbtt_fontinfo* fontInfo;
    DWORD generation;
    int glyphIndex;
    DWORD scaleXBits;
    DWORD scaleYBits;
    int width;
    int height;
    int xoff;
    int yoff;
    size_t pixelCount;
    unsigned char* pixels;
    DWORD lastUse;
} GlyphBitmapCacheEntry;

static GlyphBitmapCacheEntry g_glyphBitmapCache[GLYPH_BITMAP_CACHE_SIZE] = {0};
static DWORD g_glyphBitmapCacheUseCounter = 0;

static INIT_ONCE g_fontStateLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_fontStateCS;

/* Forward declarations for cleanup paths that already hold g_fontStateCS */
static void ClearFontCacheSTBLocked(void);
static void ClearGlyphBitmapCacheLocked(void);
static void CleanupFontSTBLocked(void);

static void AdvanceFontStateGeneration(void) {
    LONG generation = InterlockedIncrement(&g_fontStateGeneration);
    if (generation == 0) {
        InterlockedIncrement(&g_fontStateGeneration);
    }
}

static BOOL CALLBACK InitFontStateLock(PINIT_ONCE initOnce, PVOID parameter, PVOID* context) {
    (void)initOnce;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_fontStateCS);
    return TRUE;
}

static void ClearGlyphMetricsCacheLocked(void) {
    ZeroMemory(g_glyphMetricsCache, sizeof(g_glyphMetricsCache));
}

static void ClearGlyphBitmapCacheLocked(void) {
    for (int i = 0; i < GLYPH_BITMAP_CACHE_SIZE; ++i) {
        free(g_glyphBitmapCache[i].pixels);
        g_glyphBitmapCache[i].pixels = NULL;
        g_glyphBitmapCache[i].valid = FALSE;
    }
    g_glyphBitmapCacheUseCounter = 0;
}

static DWORD GetGlyphMetricsCacheSlot(wchar_t c, wchar_t nextC) {
    DWORD hash = (DWORD)c * 2654435761u;
    hash ^= ((DWORD)nextC * 2246822519u) + (hash << 6) + (hash >> 2);
    return hash & (GLYPH_METRICS_CACHE_SIZE - 1);
}

static DWORD FloatBitsForGlyphCache(float value) {
    DWORD bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static DWORD PointerBitsForGlyphCache(const void* ptr) {
    uintptr_t value = (uintptr_t)ptr;
    DWORD hash = (DWORD)value;
#if defined(_WIN64)
    hash ^= (DWORD)(((uint64_t)value) >> 32);
#endif
    return hash;
}

static DWORD GetGlyphBitmapCacheSlot(const stbtt_fontinfo* fontInfo,
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

static BOOL GlyphBitmapCacheEntryMatches(const GlyphBitmapCacheEntry* entry,
                                         const stbtt_fontinfo* fontInfo,
                                         DWORD generation,
                                         int glyphIndex,
                                         DWORD scaleXBits,
#include "drawing_text_stb_part01.inc"
#include "drawing_text_stb_part02.inc"
#include "drawing_text_stb_part03.inc"
#include "drawing_text_stb_part04.inc"
#include "drawing_text_stb_part05.inc"
#include "drawing_text_stb_part06.inc"
#include "drawing_text_stb_part07.inc"
#include "drawing_text_stb_part08.inc"
