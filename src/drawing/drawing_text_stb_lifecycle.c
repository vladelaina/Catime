/**
 * @file drawing_text_stb_lifecycle.c
 * @brief Main and fallback font lifecycle management.
 */

#include "drawing_text_stb_internal.h"

static void CleanupFontSTBLocked(void);

BOOL IsFontLoadedSTB(void) { return g_fontLoaded; }
BOOL IsFallbackFontLoadedSTB(void) { return g_fallbackFontLoaded; }
stbtt_fontinfo* GetMainFontInfoSTB(void) { return &g_fontInfo; }
stbtt_fontinfo* GetFallbackFontInfoSTB(void) { return &g_fallbackFontInfo; }

unsigned char* LoadFontMappingW(const wchar_t* path, HANDLE* phFile, HANDLE* phMapping) {
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
    ClearGlyphBitmapCacheLocked();

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
