/**
 * @file drawing_text_stb.c
 * @brief Implementation of text rendering using stb_truetype
 */

#include "drawing/drawing_text_stb.h"
#include "drawing/drawing_effect.h"
#include "menu_preview.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>  /* For SHGetFolderPathW */

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

static INIT_ONCE g_fontStateLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_fontStateCS;

/* Forward declarations for cleanup paths that already hold g_fontStateCS */
static void ClearFontCacheSTBLocked(void);
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

static DWORD GetGlyphMetricsCacheSlot(wchar_t c, wchar_t nextC) {
    DWORD hash = (DWORD)c * 2654435761u;
    hash ^= ((DWORD)nextC * 2246822519u) + (hash << 6) + (hash >> 2);
    return hash & (GLYPH_METRICS_CACHE_SIZE - 1);
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

static void ApplyCachedGlyphMetrics(const GlyphMetricsCacheEntry* entry,
                                    float scale,
                                    float fallbackScale,
                                    GlyphMetrics* out) {
    out->index = entry->index;
    out->isFallback = entry->isFallback;
    out->advance = (int)(entry->advanceUnits * (entry->isFallback ? fallbackScale : scale));
    out->kern = (int)(entry->kernUnits * scale);
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
        return FALSE;
    }

    long long left = (long long)x;
    long long top = (long long)y;
    long long right = left + (long long)bitmapWidth;
    long long bottom = top + (long long)bitmapHeight;

    if (right <= 0 || bottom <= 0 ||
        left >= (long long)destWidth || top >= (long long)destHeight) {
        return FALSE;
    }

    long long clipLeft = (left < 0) ? 0 : left;
    long long clipTop = (top < 0) ? 0 : top;
    long long clipRight = (right > (long long)destWidth) ? (long long)destWidth : right;
    long long clipBottom = (bottom > (long long)destHeight) ? (long long)destHeight : bottom;

    if (clipLeft >= clipRight || clipTop >= clipBottom) {
        return FALSE;
    }

    long long srcLeft = clipLeft - left;
    long long srcTop = clipTop - top;
    long long srcRight = clipRight - left;
    long long srcBottom = clipBottom - top;

    if (srcRight > (long long)bitmapWidth || srcBottom > (long long)bitmapHeight ||
        srcLeft > (long long)INT_MAX || srcTop > (long long)INT_MAX ||
        srcRight > (long long)INT_MAX || srcBottom > (long long)INT_MAX) {
        return FALSE;
    }

    clip->srcLeft = (int)srcLeft;
    clip->srcTop = (int)srcTop;
    clip->srcRight = (int)srcRight;
    clip->srcBottom = (int)srcBottom;
    clip->destLeft = (int)clipLeft;
    clip->destTop = (int)clipTop;
    return TRUE;
}

static int ClampTextInt64(long long value) {
    if (value > (long long)INT_MAX) return INT_MAX;
    if (value < (long long)INT_MIN) return INT_MIN;
    return (int)value;
}

static int AddTextIntClamped(int value, int delta) {
    return ClampTextInt64((long long)value + (long long)delta);
}

static int MulTextIntClamped(int value, int factor) {
    return ClampTextInt64((long long)value * (long long)factor);
}

static BOOL IsFontMappingSizeAllowed(HANDLE hFile, const wchar_t* pathForLog) {
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) ||
        fileSize.QuadPart <= 0 ||
        (ULONGLONG)fileSize.QuadPart > MAX_MAPPED_FONT_BYTES) {
        LOG_WARNING("Font file too large or unreadable for mapping: %ls (limit %llu bytes)",
                    pathForLog ? pathForLog : L"(unknown)",
                    (ULONGLONG)MAX_MAPPED_FONT_BYTES);
        return FALSE;
    }

    return TRUE;
}

static BOOL GetFontFileInfoFromHandle(HANDLE hFile,
                                      FILETIME* lastWriteTime,
                                      ULONGLONG* fileSizeOut) {
    if (lastWriteTime) {
        ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
    }
    if (fileSizeOut) {
        *fileSizeOut = 0;
    }
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(hFile, &info)) {
        return FALSE;
    }
    if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }

    ULONGLONG fileSize = ((ULONGLONG)info.nFileSizeHigh << 32) |
                         (ULONGLONG)info.nFileSizeLow;
    if (fileSize == 0 || fileSize > MAX_MAPPED_FONT_BYTES) {
        return FALSE;
    }

    if (lastWriteTime) {
        *lastWriteTime = info.ftLastWriteTime;
    }
    if (fileSizeOut) {
        *fileSizeOut = fileSize;
    }
    return TRUE;
}

static BOOL GetFontFileInfoFromPathW(const wchar_t* path,
                                     FILETIME* lastWriteTime,
                                     ULONGLONG* fileSizeOut) {
    if (lastWriteTime) {
        ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
    }
    if (fileSizeOut) {
        *fileSizeOut = 0;
    }
    if (!path || !*path) return FALSE;

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) {
        return FALSE;
    }
    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return FALSE;
    }

    ULONGLONG fileSize = ((ULONGLONG)attrs.nFileSizeHigh << 32) |
                         (ULONGLONG)attrs.nFileSizeLow;
    if (fileSize == 0 || fileSize > MAX_MAPPED_FONT_BYTES) {
        return FALSE;
    }

    if (lastWriteTime) {
        *lastWriteTime = attrs.ftLastWriteTime;
    }
    if (fileSizeOut) {
        *fileSizeOut = fileSize;
    }
    return TRUE;
}

static BOOL GetFontFileInfoFromPathUtf8(const char* path,
                                        FILETIME* lastWriteTime,
                                        ULONGLONG* fileSizeOut) {
    if (lastWriteTime) {
        ZeroMemory(lastWriteTime, sizeof(*lastWriteTime));
    }
    if (fileSizeOut) {
        *fileSizeOut = 0;
    }
    if (!path || !*path) return FALSE;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH) == 0) {
        return FALSE;
    }

    return GetFontFileInfoFromPathW(wPath, lastWriteTime, fileSizeOut);
}

static BOOL IsCurrentMainFontFileStillCurrentLocked(const char* fontFilePath) {
    if (!g_fontLoaded ||
        !g_currentFontFileInfoValid ||
        strcmp(g_currentFontPath, fontFilePath) != 0) {
        return FALSE;
    }

    DWORD now = GetTickCount();
    if (g_currentFontLastValidateTick != 0 &&
        (DWORD)(now - g_currentFontLastValidateTick) < MAIN_FONT_FILE_RECHECK_MS) {
        return TRUE;
    }
    g_currentFontLastValidateTick = now;

    FILETIME lastWriteTime;
    ULONGLONG fileSize = 0;
    if (!GetFontFileInfoFromPathUtf8(fontFilePath, &lastWriteTime, &fileSize)) {
        return FALSE;
    }

    return fileSize == g_currentFontFileSize &&
           CompareFileTime(&lastWriteTime, &g_currentFontLastWriteTime) == 0;
}

/* Accessors for external modules */
BOOL IsFontLoadedSTB(void) { return g_fontLoaded; }
BOOL IsFallbackFontLoadedSTB(void) { return g_fallbackFontLoaded; }
stbtt_fontinfo* GetMainFontInfoSTB(void) { return &g_fontInfo; }
stbtt_fontinfo* GetFallbackFontInfoSTB(void) { return &g_fallbackFontInfo; }

static unsigned char* LoadFontMappingW(const wchar_t* path, HANDLE* phFile, HANDLE* phMapping) {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    void* pView = NULL;

    if (!path || !phFile || !phMapping) return NULL;
    *phFile = INVALID_HANDLE_VALUE;
    *phMapping = NULL;

    hFile = CreateFileW(path, GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    if (!IsFontMappingSizeAllowed(hFile, path)) {
        CloseHandle(hFile);
        return NULL;
    }

    /* Create mapping for the whole file */
    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return NULL;
    }

    /* Map view of the file */
    pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!pView) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return NULL;
    }

    *phFile = hFile;
    *phMapping = hMapping;
    return (unsigned char*)pView;
}

/* Helper to map a UTF-8 file path into memory */
static unsigned char* LoadFontMapping(const char* path, HANDLE* phFile, HANDLE* phMapping) {
    if (!path || !phFile || !phMapping) return NULL;
    *phFile = INVALID_HANDLE_VALUE;
    *phMapping = NULL;

    wchar_t wPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH) == 0) {
        return NULL;
    }

    return LoadFontMappingW(wPath, phFile, phMapping);
}

void CleanupFontSTB(void) {
    if (!BeginFontUseSTB()) return;

    CleanupFontSTBLocked();

    EndFontUseSTB();
}

static void CleanupFontSTBLocked(void) {
    BOOL hadMainFontState = g_fontLoaded || g_fallbackFontLoaded ||
                            g_fontBuffer || g_fallbackFontBuffer;

    /* Cleanup main font */
    ReleaseMappedFont(g_fontBuffer, g_hFontFile, g_hFontMapping);
    g_fontBuffer = NULL;
    g_hFontFile = INVALID_HANDLE_VALUE;
    g_hFontMapping = NULL;

    /* Cleanup fallback font */
    ReleaseMappedFont(g_fallbackFontBuffer, g_hFallbackFontFile, g_hFallbackFontMapping);
    g_fallbackFontBuffer = NULL;
    g_hFallbackFontFile = INVALID_HANDLE_VALUE;
    g_hFallbackFontMapping = NULL;
    
    /* Cleanup font cache */
    ClearFontCacheSTBLocked();
    ClearGlyphMetricsCacheLocked();
    
    g_fontLoaded = FALSE;
    g_fallbackFontLoaded = FALSE;
    memset(g_currentFontPath, 0, sizeof(g_currentFontPath));
    ZeroMemory(&g_currentFontLastWriteTime, sizeof(g_currentFontLastWriteTime));
    g_currentFontFileSize = 0;
    g_currentFontLastValidateTick = 0;
    g_currentFontFileInfoValid = FALSE;
    
    /* Also cleanup effect buffers */
    CleanupDrawingEffects();

    if (hadMainFontState) {
        AdvanceFontStateGeneration();
    }
}

BOOL InitFontSTB(const char* fontFilePath) {
    if (!fontFilePath) return FALSE;

    if (!BeginFontUseSTB()) return FALSE;

    if (IsCurrentMainFontFileStillCurrentLocked(fontFilePath)) {
        EndFontUseSTB();
        return TRUE;
    }

    HANDLE hNewFile = INVALID_HANDLE_VALUE;
    HANDLE hNewMapping = NULL;
    
    // Load to temp buffer (view) first
    unsigned char* newBuffer = LoadFontMapping(fontFilePath, &hNewFile, &hNewMapping);
    if (!newBuffer) {
        EndFontUseSTB();
        return FALSE;
    }

    FILETIME newLastWriteTime;
    ULONGLONG newFileSize = 0;
    if (!GetFontFileInfoFromHandle(hNewFile, &newLastWriteTime, &newFileSize)) {
        ReleaseMappedFont(newBuffer, hNewFile, hNewMapping);
        EndFontUseSTB();
        return FALSE;
    }

    stbtt_fontinfo newInfo;
    if (!InitFontInfoFromBuffer(&newInfo, newBuffer, fontFilePath)) {
        ReleaseMappedFont(newBuffer, hNewFile, hNewMapping);
        EndFontUseSTB();
        return FALSE;
    }

    // Success - now replace the global state
    CleanupFontSTBLocked();
    
    g_fontBuffer = newBuffer;
    g_fontInfo = newInfo;
    g_hFontFile = hNewFile;
    g_hFontMapping = hNewMapping;
    strncpy(g_currentFontPath, fontFilePath, MAX_PATH - 1);
    g_currentFontPath[MAX_PATH - 1] = '\0';
    g_currentFontLastWriteTime = newLastWriteTime;
    g_currentFontFileSize = newFileSize;
    g_currentFontLastValidateTick = GetTickCount();
    g_currentFontFileInfoValid = TRUE;
    g_fontLoaded = TRUE;
    AdvanceFontStateGeneration();
    
    LOG_INFO("STB Font loaded successfully: %s", fontFilePath);

    /* Load Fallback Font */
    /* Priority:
       1. Microsoft YaHei (msyh.ttc) - Best coverage for CJK, Blocks & BW Emojis
       2. Microsoft YaHei (msyh.ttf) - Legacy
       3. Segoe UI Symbol (seguisym.ttf) - Good for blocks
       4. Segoe UI Emoji (seguiemj.ttf) - Last resort (might render blank in STB)
    */
    const char* fallbackPath = "C:\\Windows\\Fonts\\msyh.ttc";
    HANDLE hFallbackFile = INVALID_HANDLE_VALUE;
    HANDLE hFallbackMapping = NULL;
    unsigned char* fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    
    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\msyh.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguisym.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }
    
    if (!fallbackBuffer) {
        fallbackPath = "C:\\Windows\\Fonts\\seguiemj.ttf";
        fallbackBuffer = LoadFontMapping(fallbackPath, &hFallbackFile, &hFallbackMapping);
    }

    if (fallbackBuffer) {
        if (InitFontInfoFromBuffer(&g_fallbackFontInfo, fallbackBuffer, fallbackPath)) {
            g_fallbackFontBuffer = fallbackBuffer;
            g_hFallbackFontFile = hFallbackFile;
            g_hFallbackFontMapping = hFallbackMapping;
            g_fallbackFontLoaded = TRUE;
            LOG_INFO("STB Fallback Font loaded: %s", fallbackPath);
        } else {
            ReleaseMappedFont(fallbackBuffer, hFallbackFile, hFallbackMapping);
        }
    } else {
        LOG_WARNING("Failed to load fallback font (Emoji/Symbol)");
    }

    EndFontUseSTB();
    return TRUE;
}

/**
 * @brief Blend a single character bitmap into the destination buffer
 */
void BlendCharBitmapSTB(void* destBits, int destWidth, int destHeight,
                          int x_pos, int y_pos,
                          const unsigned char* bitmap, int w, int h,
                          int r, int g, int b) {
    BlendCharBitmapSTBWithEffect(destBits, destWidth, destHeight,
                                 x_pos, y_pos, bitmap, w, h, r, g, b,
                                 GetActiveEffect(), (int)GetTickCount());
}

void BlendCharBitmapSTBWithEffect(void* destBits, int destWidth, int destHeight,
                                  int x_pos, int y_pos,
                                  const unsigned char* bitmap, int w, int h,
                                  int r, int g, int b,
                                  EffectType effect, int timeOffset) {
    DWORD* pixels = (DWORD*)destBits;
    size_t pixelCount = 0;

    if (!pixels || !bitmap ||
        !CalculateBitmapPixelCount(destWidth, destHeight, &pixelCount) ||
        !CalculateBitmapPixelCount(w, h, &pixelCount)) {
        return;
    }

    /* Render glow or glass effect if enabled */
    if (effect == EFFECT_TYPE_GLOW) {
        RenderGlowEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
    } else if (effect == EFFECT_TYPE_GLASS) {
        RenderGlassEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_NEON) {
        RenderNeonEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL);
        /* Critical: Return early (Tube replaces solid text) */
        return;
    } else if (effect == EFFECT_TYPE_HOLOGRAPHIC) {
        RenderHolographicEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_LIQUID) {
        RenderLiquidEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h, r, g, b, NULL, NULL, timeOffset);
        /* Critical: Return early */
        return;
    }

    TextBitmapClip clip;
    if (!ClipTextBitmapToDestination(x_pos, y_pos, w, h, destWidth, destHeight, &clip)) {
        return;
    }

    for (int j = clip.srcTop; j < clip.srcBottom; ++j) {
        int destY = clip.destTop + (j - clip.srcTop);
        DWORD* destRow = pixels + (size_t)destY * (size_t)destWidth + (size_t)clip.destLeft;
        const unsigned char* srcRow = bitmap + (size_t)j * (size_t)w + (size_t)clip.srcLeft;

        for (int i = clip.srcLeft; i < clip.srcRight; ++i) {
            unsigned char alpha = *srcRow++;
            if (alpha == 0) {
                destRow++;
                continue;
            }

            DWORD currentPixel = *destRow;
            DWORD currentA = (currentPixel >> 24) & 0xFF;

            /* If new pixel is more opaque, overwrite */
            if (alpha > currentA) {
                /* Calculate premultiplied color values for UpdateLayeredWindow */
                DWORD finalR = (r * alpha) / 255;
                DWORD finalG = (g * alpha) / 255;
                DWORD finalB = (b * alpha) / 255;
                DWORD finalA = (DWORD)alpha;
                *destRow = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
            }
            destRow++;
        }
    }
}

/* Pre-calculated gradient LUT to reduce CPU usage */
#define LUT_SIZE GRADIENT_LUT_SIZE
#define GRADIENT_FIXED_ONE (1LL << 32)
static COLORREF g_gradientLUT[LUT_SIZE];
static GradientType g_lutType = GRADIENT_NONE;
static char g_lutName[GRADIENT_NAME_BUFFER];
static DWORD g_lutSignature = 0;

static long long GradientPositionFixed(long long x, int startX, int totalWidth) {
    if (totalWidth <= 0) return 0;

    long long offset = x - (long long)startX;
    if (offset <= 0) return 0;
    if (offset >= (long long)totalWidth) return GRADIENT_FIXED_ONE;

    return (offset * GRADIENT_FIXED_ONE) / (long long)totalWidth;
}

static int InterpolateGradientChannelFixed(int from, int to, long long position) {
    if (position <= 0) return from;
    if (position >= GRADIENT_FIXED_ONE) return to;

    long long weighted = (long long)from * (GRADIENT_FIXED_ONE - position) +
                         (long long)to * position;
    return (int)(weighted / GRADIENT_FIXED_ONE);
}

static void AdvanceGradientPositionFixed(long long* position, long long step) {
    if (!position || step <= 0 || *position >= GRADIENT_FIXED_ONE) return;

    *position += step;
    if (*position > GRADIENT_FIXED_ONE) {
        *position = GRADIENT_FIXED_ONE;
    }
}

/* Context for gradient glow callback */
typedef struct {
    const GradientInfo* info;
    int startX;
    int totalWidth;
    int timeOffset;
    BOOL isAnimated;
    int startR;
    int startG;
    int startB;
    int endR;
    int endG;
    int endB;
} GlowGradientContext;

static void InitGlowGradientContext(GlowGradientContext* ctx,
                                    const GradientInfo* info,
                                    int startX,
                                    int totalWidth,
                                    int timeOffset) {
    if (!ctx) return;

    ZeroMemory(ctx, sizeof(*ctx));
    ctx->info = info;
    ctx->startX = startX;
    ctx->totalWidth = totalWidth;
    ctx->timeOffset = timeOffset;
    ctx->isAnimated = info ? info->isAnimated : FALSE;

    if (info) {
        ctx->startR = GetRValue(info->startColor);
        ctx->startG = GetGValue(info->startColor);
        ctx->startB = GetBValue(info->startColor);
        ctx->endR = GetRValue(info->endColor);
        ctx->endG = GetGValue(info->endColor);
        ctx->endB = GetBValue(info->endColor);
    }
}

/**
 * @brief Callback to calculate gradient color for glow effect
 */
static void GetGlowGradientColor(int x, int y, int* r, int* g, int* b, void* userData) {
    const GlowGradientContext* ctx = (const GlowGradientContext*)userData;
    (void)y;

    if (!ctx || !ctx->info || !r || !g || !b) return;

    if (ctx->isAnimated) {
        long long lutPosition = 0;
        if (ctx->totalWidth > 0) {
            lutPosition = ((long long)(x - ctx->startX) * (long long)LUT_SIZE) /
                          (long long)ctx->totalWidth;
        }

        int lutIdx = (int)lutPosition - ctx->timeOffset;

        /* Wrap around */
        lutIdx = lutIdx & (LUT_SIZE - 1);

        COLORREF c = g_gradientLUT[lutIdx];
        *r = GetRValue(c);
        *g = GetGValue(c);
        *b = GetBValue(c);
    } else {
        long long position = GradientPositionFixed(x, ctx->startX, ctx->totalWidth);
        *r = InterpolateGradientChannelFixed(ctx->startR, ctx->endR, position);
        *g = InterpolateGradientChannelFixed(ctx->startG, ctx->endG, position);
        *b = InterpolateGradientChannelFixed(ctx->startB, ctx->endB, position);
    }
}

static DWORD ComputeGradientLUTSignature(const GradientInfo* info) {
    if (!info) return 0;

    DWORD signature = 2166136261u;
    signature = (signature ^ (DWORD)info->startColor) * 16777619u;
    signature = (signature ^ (DWORD)info->endColor) * 16777619u;
    signature = (signature ^ (DWORD)info->paletteCount) * 16777619u;

    if (info->palette && info->paletteCount > 0) {
        for (int i = 0; i < info->paletteCount; i++) {
            signature = (signature ^ (DWORD)info->palette[i]) * 16777619u;
        }
    }

    return signature ? signature : 1u;
}

static void SetGradientLUTKey(const GradientInfo* info) {
    g_lutType = info ? info->type : GRADIENT_NONE;
    g_lutSignature = info ? ComputeGradientLUTSignature(info) : 0;
    g_lutName[0] = '\0';
    if (info && info->type == GRADIENT_CUSTOM && info->name) {
        strncpy(g_lutName, info->name, sizeof(g_lutName) - 1);
        g_lutName[sizeof(g_lutName) - 1] = '\0';
    }
}

static BOOL GradientLUTMatches(const GradientInfo* info) {
    if (!info || g_lutType != info->type) return FALSE;
    if (g_lutSignature != ComputeGradientLUTSignature(info)) return FALSE;
    if (info->type == GRADIENT_CUSTOM) {
        return info->name && strcmp(g_lutName, info->name) == 0;
    }
    return TRUE;
}

static void InitializeGradientLUT(const GradientInfo* info) {
    if (!info) return;
    
    /* Fallback logic if palette is invalid but isAnimated is true */
    if (!info->palette || info->paletteCount < 2) {
        int r1 = GetRValue(info->startColor);
        int g1 = GetGValue(info->startColor);
        int b1 = GetBValue(info->startColor);
        
        int r2 = GetRValue(info->endColor);
        int g2 = GetGValue(info->endColor);
        int b2 = GetBValue(info->endColor);
        
        for (int i = 0; i < LUT_SIZE; i++) {
            float t = (float)i / (float)(LUT_SIZE - 1);
            int r = (int)(r1 + (r2 - r1) * t);
            int g = (int)(g1 + (g2 - g1) * t);
            int b = (int)(b1 + (b2 - b1) * t);
            g_gradientLUT[i] = RGB(r, g, b);
        }
        SetGradientLUTKey(info);
        return;
    }
    
    const int colorCount = info->paletteCount;
    const COLORREF* colors = info->palette;
    
    for (int i = 0; i < LUT_SIZE; i++) {
        float t = (float)i / (float)(LUT_SIZE - 1);
        
        /* Standard multi-stop gradient logic */
        float scaledT = t * (colorCount - 1);
        int idx = (int)scaledT;
        int nextIdx = idx + 1;
        if (nextIdx >= colorCount) nextIdx = colorCount - 1;
        
        float frac = scaledT - idx;
        
        COLORREF c1 = colors[idx];
        COLORREF c2 = colors[nextIdx];
        
        int r = (int)(GetRValue(c1) + (GetRValue(c2) - GetRValue(c1)) * frac);
        int g = (int)(GetGValue(c1) + (GetGValue(c2) - GetGValue(c1)) * frac);
        int b = (int)(GetBValue(c1) + (GetBValue(c2) - GetBValue(c1)) * frac);
        
        g_gradientLUT[i] = RGB(r, g, b);
    }
    SetGradientLUTKey(info);
}

void BlendCharBitmapGradientSTB(void* destBits, int destWidth, int destHeight,
                                int x_pos, int y_pos,
                                const unsigned char* bitmap, int w, int h,
                                int startX, int totalWidth, int gradientType,
                                int timeOffset) {
    BlendCharBitmapGradientSTBWithEffect(destBits, destWidth, destHeight,
                                         x_pos, y_pos, bitmap, w, h,
                                         startX, totalWidth, gradientType,
                                         timeOffset, GetActiveEffect());
}

void BlendCharBitmapGradientSTBWithEffect(void* destBits, int destWidth, int destHeight,
                                          int x_pos, int y_pos,
                                          const unsigned char* bitmap, int w, int h,
                                          int startX, int totalWidth, int gradientType,
                                          int timeOffset, EffectType effect) {
    GradientInfoSnapshot snapshot;
    if (!GetGradientInfoSnapshot((GradientType)gradientType, &snapshot)) return;

    BlendCharBitmapGradientSTBWithInfo(destBits, destWidth, destHeight,
                                       x_pos, y_pos, bitmap, w, h,
                                       startX, totalWidth, &snapshot.info,
                                       timeOffset, effect);
}

void BlendCharBitmapGradientSTBWithInfo(void* destBits, int destWidth, int destHeight,
                                        int x_pos, int y_pos,
                                        const unsigned char* bitmap, int w, int h,
                                        int startX, int totalWidth,
                                        const GradientInfo* gradientInfo,
                                        int timeOffset, EffectType effect) {
    const GradientInfo* info = gradientInfo;
    DWORD* pixels = (DWORD*)destBits;
    size_t destPixelCount = 0;
    size_t bitmapPixelCount = 0;

    if (!pixels || !bitmap ||
        !CalculateBitmapPixelCount(destWidth, destHeight, &destPixelCount) ||
        !CalculateBitmapPixelCount(w, h, &bitmapPixelCount)) {
        return;
    }

    if (!info) return;

    int r1 = 0, g1 = 0, b1 = 0;
    int r2 = 0, g2 = 0, b2 = 0;

    /* Animation parameters */
    float lutStep = 0.0f;
    
    if (info->isAnimated) {
        BOOL needLutUpdate = !GradientLUTMatches(info);
        if (needLutUpdate) InitializeGradientLUT(info);

        if (totalWidth > 0) {
            lutStep = (float)LUT_SIZE / (float)totalWidth;
        }
    } else {
        r1 = GetRValue(info->startColor);
        g1 = GetGValue(info->startColor);
        b1 = GetBValue(info->startColor);
        
        r2 = GetRValue(info->endColor);
        g2 = GetGValue(info->endColor);
        b2 = GetBValue(info->endColor);
    }

    /* Render glow effect if enabled - use gradient start color as base but use callback for per-pixel color */
    if (effect == EFFECT_TYPE_GLOW) {
        int glowR = GetRValue(info->startColor);
        int glowG = GetGValue(info->startColor);
        int glowB = GetBValue(info->startColor);
        
        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderGlowEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         glowR, glowG, glowB, GetGlowGradientColor, &ctx);
    } else if (effect == EFFECT_TYPE_GLASS) {
        int glassR = GetRValue(info->startColor);
        int glassG = GetGValue(info->startColor);
        int glassB = GetBValue(info->startColor);
        
        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderGlassEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         glassR, glassG, glassB, GetGlowGradientColor, &ctx);
        /* 
           CRITICAL: Return early to prevent solid gradient overwriting the glass effect.
        */
        return;
    } else if (effect == EFFECT_TYPE_NEON) {
        int neonR = GetRValue(info->startColor);
        int neonG = GetGValue(info->startColor);
        int neonB = GetBValue(info->startColor);
        
        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderNeonEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                         neonR, neonG, neonB, GetGlowGradientColor, &ctx);
        /* Neon replaces solid text */
        return;
    } else if (effect == EFFECT_TYPE_HOLOGRAPHIC) {
        int holoR = GetRValue(info->startColor);
        int holoG = GetGValue(info->startColor);
        int holoB = GetBValue(info->startColor);
        
        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderHolographicEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                                holoR, holoG, holoB, GetGlowGradientColor, &ctx, timeOffset);
        /* Critical: Return early */
        return;
    } else if (effect == EFFECT_TYPE_LIQUID) {
        int liquidR = GetRValue(info->startColor);
        int liquidG = GetGValue(info->startColor);
        int liquidB = GetBValue(info->startColor);
        
        GlowGradientContext ctx;
        InitGlowGradientContext(&ctx, info, startX, totalWidth, timeOffset);
        RenderLiquidEffect(pixels, destWidth, destHeight, x_pos, y_pos, bitmap, w, h,
                           liquidR, liquidG, liquidB, GetGlowGradientColor, &ctx, timeOffset);
        /* Critical: Return early */
        return;
    }

    TextBitmapClip clip;
    if (!ClipTextBitmapToDestination(x_pos, y_pos, w, h, destWidth, destHeight, &clip)) {
        return;
    }

    for (int j = clip.srcTop; j < clip.srcBottom; ++j) {
        int destY = clip.destTop + (j - clip.srcTop);
        size_t destIndex = (size_t)destY * (size_t)destWidth + (size_t)clip.destLeft;
        size_t srcIndex = (size_t)j * (size_t)w + (size_t)clip.srcLeft;
        if (destIndex >= destPixelCount || srcIndex >= bitmapPixelCount) continue;

        DWORD* destRow = pixels + destIndex;
        const unsigned char* srcRow = bitmap + srcIndex;

        /* Pre-calculate starting LUT index for this row if Animated */
        float currentLutIdxFloat = 0.0f;
        if (info->isAnimated) {
            long long rowStartX = (long long)clip.destLeft - (long long)startX;
            if (totalWidth > 0) {
                currentLutIdxFloat = ((float)rowStartX / (float)totalWidth) * LUT_SIZE;
            }
        }

        long long currentGradientFixed = 0;
        long long gradientFixedStep = 0;
        if (!info->isAnimated && totalWidth > 0) {
            currentGradientFixed = GradientPositionFixed(clip.destLeft, startX, totalWidth);
            gradientFixedStep = GRADIENT_FIXED_ONE / (long long)totalWidth;
        }

        for (int i = clip.srcLeft; i < clip.srcRight; ++i) {
            unsigned char alpha = *srcRow++;

            if (alpha == 0) {
                if (info->isAnimated) currentLutIdxFloat += lutStep;
                else AdvanceGradientPositionFixed(&currentGradientFixed, gradientFixedStep);
                destRow++;
                continue;
            }

            int r, g, b;

            if (info->isAnimated) {
                /* Optimized LUT Lookup */
                int lutIdx = (int)currentLutIdxFloat - timeOffset;
                currentLutIdxFloat += lutStep;
                
                /* Optimized wrap-around logic */
                lutIdx = lutIdx & (LUT_SIZE - 1);
                
                COLORREF c = g_gradientLUT[lutIdx];
                r = GetRValue(c);
                g = GetGValue(c);
                b = GetBValue(c);
            } else {
                r = InterpolateGradientChannelFixed(r1, r2, currentGradientFixed);
                g = InterpolateGradientChannelFixed(g1, g2, currentGradientFixed);
                b = InterpolateGradientChannelFixed(b1, b2, currentGradientFixed);
                AdvanceGradientPositionFixed(&currentGradientFixed, gradientFixedStep);
            }

            /* Blend */
            DWORD currentPixel = *destRow;
            DWORD currentA = (currentPixel >> 24) & 0xFF;
            
            if (alpha > currentA) {
                DWORD finalR = (r * alpha) / 255;
                DWORD finalG = (g * alpha) / 255;
                DWORD finalB = (b * alpha) / 255;
                DWORD finalA = (DWORD)alpha;
                
                *destRow = (finalA << 24) | (finalR << 16) | (finalG << 8) | finalB;
            }
            destRow++;
        }
    }
}

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
        out->advance = (int)(cached->advanceUnits * scale);
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
    out->advance = (int)(advanceUnits * scale);
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

BOOL MeasureTextSTB(const wchar_t* text, int fontSize, int* width, int* height) {
    if (!BeginFontUseSTB()) return FALSE;
    BOOL result = FALSE;

    if (!g_fontLoaded || !text) goto done;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)fontSize);
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)fontSize) : 0;

    int maxWidth = 0;
    int curLineWidth = 0;
    int lineCount = 1;
    size_t len = wcslen(text);

    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            if (curLineWidth > maxWidth) maxWidth = curLineWidth;
            curLineWidth = 0;
            lineCount++;
            continue;
        }
        if (text[i] == L'\r') continue;

        GlyphMetrics gm;
        GetCharMetricsSTB(text[i], (i < len - 1) ? text[i+1] : 0, scale, fallbackScale, &gm);
        curLineWidth = AddTextIntClamped(curLineWidth, gm.advance + gm.kern);
    }
    if (curLineWidth > maxWidth) maxWidth = curLineWidth;

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = (int)((ascent - descent + lineGap) * scale);

    if (width) *width = maxWidth;
    if (height) *height = MulTextIntClamped(lineCount, lineHeight);
    result = TRUE;

done:
    EndFontUseSTB();
    return result;
}

void RenderTextSTB(void* bits, int width, int height, const wchar_t* text,
                   COLORREF color, int fontSize, float fontScale, BOOL editMode) {
    UNREFERENCED_PARAMETER(editMode);

    if (!BeginFontUseSTB()) return;
    if (!g_fontLoaded || !text || !bits) goto done;

    float scale = stbtt_ScaleForPixelHeight(&g_fontInfo, (float)(fontSize * fontScale));
    float fallbackScale = g_fallbackFontLoaded ? stbtt_ScaleForPixelHeight(&g_fallbackFontInfo, (float)(fontSize * fontScale)) : 0;
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_fontInfo, &ascent, &descent, &lineGap);
    int lineHeight = (int)((ascent - descent + lineGap) * scale);
    int baselineOffset = (int)(ascent * scale);
    
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    EffectType effect = GetActiveEffect();
    int timeOffset = (int)GetTickCount();

    // Pre-calculate line widths for centering
    // We can do a quick pass or re-use Measure logic per line
    size_t len = wcslen(text);
    int currentY = 0;
    
    // Calculate total text height to vertically center the whole block.
    // Width metrics are computed per line below, so only line count is needed here.
    int lineCount = 1;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == L'\n') {
            lineCount++;
        }
    }
    int totalTextHeight = MulTextIntClamped(lineCount, lineHeight);
    
    int startY = (height - totalTextHeight) / 2;
    int currentLineStart = 0;
    
    for (size_t i = 0; i <= len; i++) {
        if (text[i] == L'\n' || text[i] == L'\0') {
            // Line complete, render it
            int lineWidth = 0;
            // Calculate width of this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                lineWidth = AddTextIntClamped(lineWidth, gm.advance + gm.kern);
            }

            int currentX = (width - lineWidth) / 2;
            int lineY = AddTextIntClamped(AddTextIntClamped(startY,
                                                            MulTextIntClamped(currentY, lineHeight)),
                                          baselineOffset);

            // Render line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;
                
                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                
                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;

                    const stbtt_fontinfo* glyphFontInfo = gm.isFallback ? &g_fallbackFontInfo : &g_fontInfo;
                    float glyphScale = gm.isFallback ? fallbackScale : scale;
                    if (!IsGlyphBitmapVisibleSTB(glyphFontInfo, gm.index, glyphScale, glyphScale,
                                                 currentX, lineY, width, height,
                                                 effect, 0)) {
                        currentX = AddTextIntClamped(currentX, gm.advance + gm.kern);
                        continue;
                    }

                    bitmap = stbtt_GetGlyphBitmap(glyphFontInfo, glyphScale, glyphScale,
                                                  gm.index, &w, &h, &xoff, &yoff);

                    if (bitmap) {
                        int glyphX = AddTextIntClamped(currentX, xoff);
                        int glyphY = AddTextIntClamped(lineY, yoff);
                        BlendCharBitmapSTBWithEffect(bits, width, height,
                                                     glyphX, glyphY,
                                                     bitmap, w, h, r, g, b,
                                                     effect, timeOffset);
                        stbtt_FreeBitmap(bitmap, NULL);
                    }
                }
                currentX = AddTextIntClamped(currentX, gm.advance + gm.kern);
            }
            
            currentY++;
            currentLineStart = i + 1;
        }
    }

done:
    EndFontUseSTB();
}

/* ============================================================================
 * Font Cache Implementation for <font:> Tags
 * ============================================================================ */

/**
 * @brief Resolve font path from tag value
 * 
 * Supports:
 * - Absolute paths: C:\Fonts\my.ttf, \\server\share\font.ttf
 * - Environment variables: %WINDIR%\Fonts\arial.ttf
 * - Relative paths: fonts/custom.ttf (resolved relative to plugins directory)
 * 
 * @param fontPath Font path from <font:> tag (wide string)
 * @param outPath Output buffer for resolved path (wide string)
 * @param pathSize Buffer size
 * @return TRUE if path resolved and file exists
 */
static BOOL ResolveFontTagPath(const wchar_t* fontPath, wchar_t* outPath, size_t pathSize) {
    if (!fontPath || !outPath || pathSize == 0) return FALSE;
    outPath[0] = L'\0';

    wchar_t expandedPath[MAX_PATH];
    wchar_t resolvedPath[MAX_PATH];
    
    /* Step 1: Expand environment variables if present */
    if (wcschr(fontPath, L'%') != NULL) {
        DWORD result = ExpandEnvironmentStringsW(fontPath, expandedPath, MAX_PATH);
        if (result == 0 || result > MAX_PATH) {
            LOG_WARNING("Failed to expand environment variables in font path: %ls", fontPath);
            return FALSE;
        }
    } else {
        if (wcslen(fontPath) >= MAX_PATH) {
            LOG_WARNING("Font path is too long: %ls", fontPath);
            return FALSE;
        }
        wcsncpy(expandedPath, fontPath, MAX_PATH - 1);
        expandedPath[MAX_PATH - 1] = L'\0';
    }
    
    /* Step 2: Check if absolute path (has drive letter or UNC path) */
    BOOL isAbsolute = FALSE;
    if (wcslen(expandedPath) >= 2) {
        /* Check for drive letter (e.g., C:\) */
        if (expandedPath[1] == L':') {
            isAbsolute = TRUE;
        }
        /* Check for UNC path (e.g., \\server\share) */
        else if (expandedPath[0] == L'\\' && expandedPath[1] == L'\\') {
            isAbsolute = TRUE;
        }
    }
    
    if (isAbsolute) {
        if (wcslen(expandedPath) >= MAX_PATH) {
            LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
            return FALSE;
        }
        wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
        resolvedPath[MAX_PATH - 1] = L'\0';
    } else {
        /* Step 3: Resolve relative path against plugins directory */
        wchar_t pluginsDir[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, pluginsDir))) {
            int written = _snwprintf_s(resolvedPath, MAX_PATH, _TRUNCATE,
                                       L"%ls\\Catime\\resources\\plugins\\%ls",
                                       pluginsDir, expandedPath);
            if (written < 0) {
                LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
                return FALSE;
            }
        } else {
            /* Fallback: try current directory */
            if (wcslen(expandedPath) >= MAX_PATH) {
                LOG_WARNING("Font path is too long: %ls", expandedPath);
                return FALSE;
            }
            wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
            resolvedPath[MAX_PATH - 1] = L'\0';
        }
    }
    
    /* Step 4: Check if file exists */
    DWORD attrs = GetFileAttributesW(resolvedPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_WARNING("Font file not found: %ls", resolvedPath);
        return FALSE;
    }

    /* Step 5: Return wide path directly to avoid UTF-8 round trip before CreateFileW. */
    if (wcslen(resolvedPath) >= pathSize) {
        LOG_WARNING("Resolved font path is too long: %ls", resolvedPath);
        return FALSE;
    }
    wcsncpy(outPath, resolvedPath, pathSize - 1);
    outPath[pathSize - 1] = L'\0';

    return TRUE;
}

void ClearFontCacheSTB(void) {
    if (!BeginFontUseSTB()) return;

    ClearFontCacheSTBLocked();

    EndFontUseSTB();
}

static void ReleaseCachedFontSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return;

    if (g_fontCache[slot].isLoaded) {
        ReleaseMappedFont(g_fontCache[slot].fontBuffer,
                          g_fontCache[slot].hFile,
                          g_fontCache[slot].hMapping);
        AdvanceFontStateGeneration();
    }
    ZeroMemory(&g_fontCache[slot], sizeof(CachedFont));
    g_fontCacheLRU[slot] = 0;
    ClearFontTagGlyphMetricsCacheSlotLocked(slot);
}

static void ClearFontCacheSTBLocked(void) {
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        ReleaseCachedFontSlotLocked(i);
    }
    ZeroMemory(g_failedFontCache, sizeof(g_failedFontCache));
    g_fontCacheAccessCounter = 0;
}

static BOOL IsCachedFontSlotCurrentLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return FALSE;

    CachedFont* entry = &g_fontCache[slot];
    if (!entry->isLoaded || !entry->fileInfoValid || entry->resolvedPath[0] == L'\0') {
        return FALSE;
    }

    DWORD now = GetTickCount();
    if (entry->lastValidateTick != 0 &&
        (DWORD)(now - entry->lastValidateTick) < MAIN_FONT_FILE_RECHECK_MS) {
        return TRUE;
    }
    entry->lastValidateTick = now;

    FILETIME lastWriteTime;
    ULONGLONG fileSize = 0;
    if (!GetFontFileInfoFromPathW(entry->resolvedPath, &lastWriteTime, &fileSize)) {
        return FALSE;
    }

    return entry->fileSize == fileSize &&
           CompareFileTime(&entry->lastWriteTime, &lastWriteTime) == 0;
}

static BOOL CopyFontCacheKeyW(const wchar_t* fontPath, wchar_t* outPath) {
    if (!outPath) return FALSE;
    outPath[0] = L'\0';
    if (!fontPath || fontPath[0] == L'\0') return FALSE;

    size_t len = 0;
    while (len < MAX_PATH && fontPath[len] != L'\0') {
        len++;
    }
    if (len >= MAX_PATH) {
        return FALSE;
    }

    memcpy(outPath, fontPath, (len + 1) * sizeof(wchar_t));
    return TRUE;
}

static BOOL IsRecentFontFailureCached(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return FALSE;

    DWORD now = GetTickCount();
    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] == L'\0') continue;
        if (wcscmp(g_failedFontCache[i].fontName, cacheKey) != 0) continue;

        if ((LONG)(g_failedFontCache[i].retryAfterFailureTick - now) > 0) {
            return TRUE;
        }

        ZeroMemory(&g_failedFontCache[i], sizeof(g_failedFontCache[i]));
        return FALSE;
    }

    return FALSE;
}

static void RemoveFailedFontCacheEntry(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return;

    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] != L'\0' &&
            wcscmp(g_failedFontCache[i].fontName, cacheKey) == 0) {
            ZeroMemory(&g_failedFontCache[i], sizeof(g_failedFontCache[i]));
            return;
        }
    }
}

static void RecordFailedFontCacheEntry(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return;

    DWORD now = GetTickCount();
    int target = -1;

    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] == L'\0') {
            target = i;
            break;
        }

        if (wcscmp(g_failedFontCache[i].fontName, cacheKey) == 0) {
            target = i;
            break;
        }

        if ((LONG)(g_failedFontCache[i].retryAfterFailureTick - now) <= 0) {
            target = i;
            break;
        }

        if (target < 0 ||
            (LONG)(g_failedFontCache[target].retryAfterFailureTick -
                   g_failedFontCache[i].retryAfterFailureTick) > 0) {
            target = i;
        }
    }

    if (target < 0) return;

    memcpy(g_failedFontCache[target].fontName, cacheKey, sizeof(cacheKey));
    DWORD retryAfter = now + FONT_FAILURE_RETRY_MS;
    g_failedFontCache[target].retryAfterFailureTick = retryAfter ? retryAfter : 1;
}

/**
 * @brief Get cached font by file path
 * 
 * The fontPath parameter should be a file path, not a font name.
 * Supports:
 * - Absolute paths: C:\Fonts\my.ttf
 * - Environment variables: %WINDIR%\Fonts\arial.ttf
 * - Relative paths: fonts/custom.ttf (resolved relative to plugins directory)
 * 
 * @param fontPath Font file path from <font:> tag
 * @return Font info pointer, or NULL if font cannot be loaded
 */
stbtt_fontinfo* GetCachedFontSTB(const wchar_t* fontPath) {
    if (!fontPath || fontPath[0] == L'\0') return NULL;

    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) {
        LOG_WARNING("Font cache key is too long: %ls", fontPath);
        return NULL;
    }

    int targetSlot = -1;

    /* Search cache for existing font (by original path for cache key) */
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        if (g_fontCache[i].isLoaded && wcscmp(g_fontCache[i].fontName, cacheKey) == 0) {
            if (IsCachedFontSlotCurrentLocked(i)) {
                TouchFontCacheSlotLocked(i);
                return &g_fontCache[i].fontInfo;
            }
            ReleaseCachedFontSlotLocked(i);
            targetSlot = i;
            break;
        }
    }

    if (IsRecentFontFailureCached(cacheKey)) {
        return NULL;
    }

    /* Font not in cache, resolve path and load it */
    wchar_t resolvedPath[MAX_PATH];
    if (!ResolveFontTagPath(fontPath, resolvedPath, _countof(resolvedPath))) {
        RecordFailedFontCacheEntry(cacheKey);
        LOG_WARNING("Font path resolution failed: %ls", fontPath);
        return NULL;
    }
    
    /* Find slot: empty or LRU */
    if (targetSlot < 0) {
        int minLRU = INT_MAX;
        for (int i = 0; i < MAX_CACHED_FONTS; i++) {
            if (!g_fontCache[i].isLoaded) {
                targetSlot = i;
                break;
            }
            if (g_fontCacheLRU[i] < minLRU) {
                minLRU = g_fontCacheLRU[i];
                targetSlot = i;
            }
        }
    }
    
    if (targetSlot < 0) targetSlot = 0;

    /* Load new font */
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    unsigned char* buffer = LoadFontMappingW(resolvedPath, &hFile, &hMapping);

    if (!buffer) {
        RecordFailedFontCacheEntry(cacheKey);
        LOG_WARNING("Failed to load font file: %ls", resolvedPath);
        return NULL;
    }

    FILETIME lastWriteTime;
    ULONGLONG fileSize = 0;
    if (!GetFontFileInfoFromHandle(hFile, &lastWriteTime, &fileSize)) {
        ReleaseMappedFont(buffer, hFile, hMapping);
        RecordFailedFontCacheEntry(cacheKey);
        return NULL;
    }

    stbtt_fontinfo newFontInfo;
    if (!InitFontInfoFromBufferW(&newFontInfo, buffer, resolvedPath)) {
        ReleaseMappedFont(buffer, hFile, hMapping);
        RecordFailedFontCacheEntry(cacheKey);
        return NULL;
    }

    /* Evict only after the replacement font is known to be usable. */
    if (g_fontCache[targetSlot].isLoaded) {
        ReleaseCachedFontSlotLocked(targetSlot);
    }

    /* Store in cache (use original path as cache key) */
    RemoveFailedFontCacheEntry(cacheKey);
    memcpy(g_fontCache[targetSlot].fontName, cacheKey, sizeof(cacheKey));
    wcscpy_s(g_fontCache[targetSlot].resolvedPath,
             _countof(g_fontCache[targetSlot].resolvedPath),
             resolvedPath);
    g_fontCache[targetSlot].fontInfo = newFontInfo;
    g_fontCache[targetSlot].fontBuffer = buffer;
    g_fontCache[targetSlot].hFile = hFile;
    g_fontCache[targetSlot].hMapping = hMapping;
    g_fontCache[targetSlot].lastWriteTime = lastWriteTime;
    g_fontCache[targetSlot].fileSize = fileSize;
    g_fontCache[targetSlot].lastValidateTick = GetTickCount();
    g_fontCache[targetSlot].fileInfoValid = TRUE;
    g_fontCache[targetSlot].isLoaded = TRUE;
    TouchFontCacheSlotLocked(targetSlot);
    AdvanceFontStateGeneration();

    LOG_INFO("Cached font loaded: %ls -> %ls", fontPath, resolvedPath);

    return &g_fontCache[targetSlot].fontInfo;
}
