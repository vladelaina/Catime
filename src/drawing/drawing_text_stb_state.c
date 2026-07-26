/**
 * @file drawing_text_stb_state.c
 * @brief Shared STB font state, locking, and low-level cache cleanup.
 */

#include "drawing_text_stb_internal.h"

unsigned char* g_fontBuffer = NULL;
stbtt_fontinfo g_fontInfo;
char g_currentFontPath[MAX_PATH] = {0};
FILETIME g_currentFontLastWriteTime = {0};
ULONGLONG g_currentFontFileSize = 0;
DWORD g_currentFontLastValidateTick = 0;
BOOL g_currentFontFileInfoValid = FALSE;
BOOL g_fontLoaded = FALSE;

unsigned char* g_fallbackFontBuffer = NULL;
stbtt_fontinfo g_fallbackFontInfo;
BOOL g_fallbackFontLoaded = FALSE;

HANDLE g_hFontFile = INVALID_HANDLE_VALUE;
HANDLE g_hFontMapping = NULL;
HANDLE g_hFallbackFontFile = INVALID_HANDLE_VALUE;
HANDLE g_hFallbackFontMapping = NULL;
volatile LONG g_fontStateGeneration = 1;

CachedFont g_fontCache[MAX_CACHED_FONTS] = {0};
int g_fontCacheLRU[MAX_CACHED_FONTS] = {0};
int g_fontCacheAccessCounter = 0;
FontTagGlyphMetricsCacheEntry
    g_fontTagGlyphMetricsCache[MAX_CACHED_FONTS][FONT_TAG_GLYPH_METRICS_CACHE_SIZE] = {0};
FailedFontCacheEntry g_failedFontCache[MAX_FAILED_FONT_CACHE] = {0};
GlyphMetricsCacheEntry g_glyphMetricsCache[GLYPH_METRICS_CACHE_SIZE] = {0};
GlyphBitmapCacheEntry g_glyphBitmapCache[GLYPH_BITMAP_CACHE_SIZE] = {0};
DWORD g_glyphBitmapCacheUseCounter = 0;

INIT_ONCE g_fontStateLockOnce = INIT_ONCE_STATIC_INIT;
CRITICAL_SECTION g_fontStateCS;

void AdvanceFontStateGeneration(void) {
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

void ClearGlyphMetricsCacheLocked(void) {
    ZeroMemory(g_glyphMetricsCache, sizeof(g_glyphMetricsCache));
}

void ClearGlyphBitmapCacheLocked(void) {
    for (int i = 0; i < GLYPH_BITMAP_CACHE_SIZE; ++i) {
        free(g_glyphBitmapCache[i].pixels);
        g_glyphBitmapCache[i].pixels = NULL;
        g_glyphBitmapCache[i].valid = FALSE;
    }
    g_glyphBitmapCacheUseCounter = 0;
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
