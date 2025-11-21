/**
 * @file resource_cache.c
 * @brief Resource cache system implementation
 */

#include "cache/resource_cache.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static FontCache g_fontCache = {NULL, 0, 0, 0, FALSE, {0}};
static AnimationCache g_animCache = {NULL, 0, 0, 0, FALSE, {0}};

static HANDLE g_backgroundThread = NULL;
static volatile BOOL g_shutdownRequested = FALSE;
static volatile BOOL g_backgroundScanComplete = FALSE;
static volatile LONG g_refreshInProgress = 0;  // Use LONG for InterlockedCompareExchange
static BOOL g_initialized = FALSE;

#define MAX_RECURSION_DEPTH 10

/* ============================================================================
 * Internal Helpers - Path Management
 * ============================================================================ */

/**
 * @brief Get fonts folder path
 */
static BOOL GetFontsFolderPath(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 20 >= size) return FALSE;
    
    // Use _snwprintf for safety instead of wcscat
    int written = _snwprintf(outPath, size, L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath);
    if (written < 0 || written >= (int)size) return FALSE;
    
    return TRUE;
}

/**
 * @brief Get animations folder path
 */
static BOOL GetAnimationsFolderPathInternal(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 25 >= size) return FALSE;
    
    // Use _snwprintf for safety instead of wcscat
    int written = _snwprintf(outPath, size, L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath);
    if (written < 0 || written >= (int)size) return FALSE;
    
    return TRUE;
}

/**
 * @brief Get current font relative path from config
 */
static void GetCurrentFontRelativePath(char* outPath, size_t size) {
    extern char FONT_FILE_NAME[MAX_PATH];
    const char* prefix = FONTS_PATH_PREFIX;
    
    // Safety checks
    if (!prefix || !outPath || size == 0) {
        if (outPath && size > 0) outPath[0] = '\0';
        return;
    }
    
    size_t prefixLen = strlen(prefix);
    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        strncpy(outPath, FONT_FILE_NAME + prefixLen, size - 1);
        outPath[size - 1] = '\0';
    } else {
        outPath[0] = '\0';
    }
}

/**
 * @brief Get current animation name from tray system
 */
static void GetCurrentAnimationNameInternal(char* outName, size_t size) {
    if (!outName || size == 0) return;
    
    extern const char* GetCurrentAnimationName(void);
    const char* name = GetCurrentAnimationName();
    if (name) {
        strncpy(outName, name, size - 1);
        outName[size - 1] = '\0';
    } else {
        outName[0] = '\0';
    }
}

/* ============================================================================
 * Internal Helpers - Cache Entry Management
 * ============================================================================ */

/**
 * @brief Add font entry to cache (must hold write lock)
 */
static BOOL AddFontEntry(FontCache* cache, const wchar_t* fileName, 
                         const wchar_t* fullPath, const wchar_t* relativePath,
                         int depth) {
    if (cache->count >= cache->capacity) {
        int newCapacity = cache->capacity == 0 ? 32 : cache->capacity * 2;
        if (newCapacity > MAX_CACHE_ENTRIES) {
            WriteLog(LOG_LEVEL_WARNING, "Font cache capacity limit reached (%d fonts), skipping: %ls", 
                     MAX_CACHE_ENTRIES, fileName);
            return FALSE;
        }
        
        FontCacheEntry* newEntries = (FontCacheEntry*)realloc(
            cache->entries, newCapacity * sizeof(FontCacheEntry));
        if (!newEntries) return FALSE;
        
        cache->entries = newEntries;
        cache->capacity = newCapacity;
    }
    
    FontCacheEntry* entry = &cache->entries[cache->count];
    wcsncpy(entry->fileName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->fileName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    
    wcsncpy(entry->fullPath, fullPath, MAX_PATH - 1);
    entry->fullPath[MAX_PATH - 1] = L'\0';
    
    wcsncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = L'\0';
    
    wcsncpy(entry->displayName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->displayName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
    if (dotPos) *dotPos = L'\0';
    
    entry->isCurrentFont = FALSE;
    entry->depth = depth;
    
    cache->count++;
    return TRUE;
}

/**
 * @brief Add animation entry to cache (must hold write lock)
 */
static BOOL AddAnimationEntry(AnimationCache* cache, const char* fileName,
                               const wchar_t* fullPath, const char* relativePath,
                               BOOL isSpecial) {
    if (cache->count >= cache->capacity) {
        int newCapacity = cache->capacity == 0 ? 32 : cache->capacity * 2;
        if (newCapacity > MAX_CACHE_ENTRIES) {
            WriteLog(LOG_LEVEL_WARNING, "Animation cache capacity limit reached (%d animations), skipping: %s", 
                     MAX_CACHE_ENTRIES, fileName);
            return FALSE;
        }
        
        AnimationCacheEntry* newEntries = (AnimationCacheEntry*)realloc(
            cache->entries, newCapacity * sizeof(AnimationCacheEntry));
        if (!newEntries) return FALSE;
        
        cache->entries = newEntries;
        cache->capacity = newCapacity;
    }
    
    AnimationCacheEntry* entry = &cache->entries[cache->count];
    strncpy(entry->fileName, fileName, MAX_ANIM_NAME_LENGTH - 1);
    entry->fileName[MAX_ANIM_NAME_LENGTH - 1] = '\0';
    
    strncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = '\0';
    
    wcsncpy(entry->fullPath, fullPath, MAX_PATH - 1);
    entry->fullPath[MAX_PATH - 1] = L'\0';
    
    entry->isSpecial = isSpecial;
    entry->isCurrent = FALSE;
    
    cache->count++;
    return TRUE;
}

/* ============================================================================
 * Internal - Font Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan font folder
 */
static void ScanFontFolderRecursive(const wchar_t* folderPath, 
                                    const wchar_t* basePath,
                                    const wchar_t* relativePath,
                                    FontCache* cache,
                                    int depth) {
    // Prevent infinite recursion from symlinks or deep folder structures
    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }
    
    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf(searchPath, MAX_PATH, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        WriteLog(LOG_LEVEL_WARNING, "Path too long: %ls", folderPath);
        return;
    }
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to scan font folder: %ls", folderPath);
        return;
    }
    
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || 
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        wchar_t fullPath[MAX_PATH];
        int len1 = _snwprintf(fullPath, MAX_PATH, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0 || len1 >= MAX_PATH) continue;  // Skip if path too long
        
        wchar_t newRelativePath[MAX_PATH];
        if (relativePath[0] == L'\0') {
            wcsncpy(newRelativePath, findData.cFileName, MAX_PATH - 1);
            newRelativePath[MAX_PATH - 1] = L'\0';
        } else {
            int len2 = _snwprintf(newRelativePath, MAX_PATH, L"%s\\%s", relativePath, findData.cFileName);
            if (len2 < 0 || len2 >= MAX_PATH) continue;  // Skip if path too long
        }
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursive scan subdirectory
            ScanFontFolderRecursive(fullPath, basePath, newRelativePath, cache, depth + 1);
        } else {
            // Check if font file
            wchar_t* ext = wcsrchr(findData.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                if (!AddFontEntry(cache, findData.cFileName, fullPath, newRelativePath, depth)) {
                    // Cache full, stop scanning
                    FindClose(hFind);
                    return;
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

/**
 * @brief Scan font folder and populate cache
 */
static BOOL ScanFontFolder(FontCache* cache) {
    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderPath(fontsPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to get fonts folder path");
        return FALSE;
    }
    
    // Check if folder exists
    DWORD attribs = GetFileAttributesW(fontsPath);
    if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Fonts folder does not exist: %ls", fontsPath);
        return FALSE;
    }
    
    // Optional: Shrink cache if significantly underused (memory optimization)
    // Check before clearing count
    if (cache->capacity > 64 && cache->count < cache->capacity / 4) {
        int newCapacity = cache->capacity / 2;
        if (newCapacity < 32) newCapacity = 32;
        
        FontCacheEntry* newEntries = (FontCacheEntry*)realloc(
            cache->entries, newCapacity * sizeof(FontCacheEntry));
        if (newEntries) {
            cache->entries = newEntries;
            cache->capacity = newCapacity;
            WriteLog(LOG_LEVEL_INFO, "Font cache shrunk to %d entries (memory optimization)", newCapacity);
        }
    }
    
    // Clear existing entries (reuse allocated memory)
    cache->count = 0;
    
    // Start recursive scan
    ScanFontFolderRecursive(fontsPath, fontsPath, L"", cache, 0);
    
    // Mark current font
    char currentFontRelPath[MAX_PATH];
    GetCurrentFontRelativePath(currentFontRelPath, MAX_PATH);
    
    if (currentFontRelPath[0] != '\0') {
        wchar_t wCurrentRelPath[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, currentFontRelPath, -1, wCurrentRelPath, MAX_PATH);
        
        for (int i = 0; i < cache->count; i++) {
            if (_wcsicmp(cache->entries[i].relativePath, wCurrentRelPath) == 0) {
                cache->entries[i].isCurrentFont = TRUE;
                break;
            }
        }
    }
    
    cache->scanTime = time(NULL);
    cache->isValid = TRUE;
    
    WriteLog(LOG_LEVEL_INFO, "Font cache scan complete: %d fonts found", cache->count);
    return TRUE;
}

/* ============================================================================
 * Internal - Animation Folder Scanning
 * ============================================================================ */

/**
 * @brief Check if file extension is supported animation format
 */
static BOOL IsAnimationFile(const wchar_t* fileName) {
    wchar_t* ext = wcsrchr(fileName, L'.');
    if (!ext) return FALSE;
    
    return _wcsicmp(ext, L".gif") == 0 ||
           _wcsicmp(ext, L".webp") == 0 ||
           _wcsicmp(ext, L".png") == 0 ||
           _wcsicmp(ext, L".jpg") == 0 ||
           _wcsicmp(ext, L".jpeg") == 0 ||
           _wcsicmp(ext, L".bmp") == 0;
}

/**
 * @brief Scan animation folder and populate cache
 */
static BOOL ScanAnimationFolder(AnimationCache* cache) {
    wchar_t animPath[MAX_PATH];
    if (!GetAnimationsFolderPathInternal(animPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to get animations folder path");
        return FALSE;
    }
    
    DWORD attribs = GetFileAttributesW(animPath);
    if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Animations folder does not exist: %ls", animPath);
        return FALSE;
    }
    
    // Optional: Shrink cache if significantly underused (memory optimization)
    // Check before clearing count
    if (cache->capacity > 64 && cache->count < cache->capacity / 4) {
        int newCapacity = cache->capacity / 2;
        if (newCapacity < 32) newCapacity = 32;
        
        AnimationCacheEntry* newEntries = (AnimationCacheEntry*)realloc(
            cache->entries, newCapacity * sizeof(AnimationCacheEntry));
        if (newEntries) {
            cache->entries = newEntries;
            cache->capacity = newCapacity;
            WriteLog(LOG_LEVEL_INFO, "Animation cache shrunk to %d entries (memory optimization)", newCapacity);
        }
    }
    
    cache->count = 0;
    
    // Add special entries
    AddAnimationEntry(cache, "__logo__", L"", "__logo__", TRUE);
    AddAnimationEntry(cache, "__cpu__", L"", "__cpu__", TRUE);
    AddAnimationEntry(cache, "__mem__", L"", "__mem__", TRUE);
    
    // Scan folder
    wchar_t searchPath[MAX_PATH];
    int pathLen = _snwprintf(searchPath, MAX_PATH, L"%s\\*", animPath);
    if (pathLen < 0 || pathLen >= MAX_PATH) {
        WriteLog(LOG_LEVEL_ERROR, "Animation search path too long");
        return FALSE;
    }
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || 
                wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (IsAnimationFile(findData.cFileName)) {
                    wchar_t fullPath[MAX_PATH];
                    int fullPathLen = _snwprintf(fullPath, MAX_PATH, L"%s\\%s", animPath, findData.cFileName);
                    if (fullPathLen < 0 || fullPathLen >= MAX_PATH) {
                        WriteLog(LOG_LEVEL_WARNING, "Animation path too long, skipping: %ls", findData.cFileName);
                        continue;  // Skip this file
                    }
                    
                    char fileNameUtf8[MAX_ANIM_NAME_LENGTH];
                    WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1, 
                                       fileNameUtf8, MAX_ANIM_NAME_LENGTH, NULL, NULL);
                    
                    if (!AddAnimationEntry(cache, fileNameUtf8, fullPath, fileNameUtf8, FALSE)) {
                        // Cache full, stop scanning
                        FindClose(hFind);
                        return FALSE;
                    }
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
    
    // Mark current animation
    char currentAnim[MAX_ANIM_NAME_LENGTH];
    GetCurrentAnimationNameInternal(currentAnim, MAX_ANIM_NAME_LENGTH);
    
    if (currentAnim[0] != '\0') {
        for (int i = 0; i < cache->count; i++) {
            if (strcmp(cache->entries[i].fileName, currentAnim) == 0) {
                cache->entries[i].isCurrent = TRUE;
                break;
            }
        }
    }
    
    cache->scanTime = time(NULL);
    cache->isValid = TRUE;
    
    WriteLog(LOG_LEVEL_INFO, "Animation cache scan complete: %d animations found", cache->count);
    return TRUE;
}

/* ============================================================================
 * Background Thread
 * ============================================================================ */

/**
 * @brief Background scanning thread function
 */
static DWORD WINAPI BackgroundScanThread(LPVOID lpParam) {
    (void)lpParam;
    
    WriteLog(LOG_LEVEL_INFO, "Background resource scan started");
    
    // Small delay to let main window initialize
    Sleep(100);
    
    // Check if shutdown requested
    if (g_shutdownRequested) {
        WriteLog(LOG_LEVEL_INFO, "Background scan aborted (shutdown requested)");
        InterlockedExchange(&g_refreshInProgress, 0);
        return 0;
    }
    
    // Scan fonts (with write lock)
    AcquireSRWLockExclusive(&g_fontCache.lock);
    if (!g_shutdownRequested) {
        ScanFontFolder(&g_fontCache);
    }
    ReleaseSRWLockExclusive(&g_fontCache.lock);
    
    // Check again before scanning animations
    if (g_shutdownRequested) {
        WriteLog(LOG_LEVEL_INFO, "Background scan aborted after fonts (shutdown requested)");
        InterlockedExchange(&g_refreshInProgress, 0);
        return 0;
    }
    
    // Scan animations (with write lock)
    AcquireSRWLockExclusive(&g_animCache.lock);
    if (!g_shutdownRequested) {
        ScanAnimationFolder(&g_animCache);
    }
    ReleaseSRWLockExclusive(&g_animCache.lock);
    
    g_backgroundScanComplete = TRUE;
    InterlockedExchange(&g_refreshInProgress, 0);
    
    WriteLog(LOG_LEVEL_INFO, "Background resource scan complete");
    
    return 0;
}

/* ============================================================================
 * Public API - Initialization
 * ============================================================================ */

BOOL ResourceCache_Initialize(BOOL startBackgroundScan) {
    if (g_initialized) {
        return TRUE; // Already initialized
    }
    
    WriteLog(LOG_LEVEL_INFO, "Initializing resource cache system");
    
    // Initialize locks
    InitializeSRWLock(&g_fontCache.lock);
    InitializeSRWLock(&g_animCache.lock);
    
    g_shutdownRequested = FALSE;
    g_backgroundScanComplete = FALSE;
    InterlockedExchange(&g_refreshInProgress, 0);
    g_initialized = TRUE;
    
    if (startBackgroundScan) {
        InterlockedExchange(&g_refreshInProgress, 1);
        g_backgroundThread = CreateThread(
            NULL, 0, BackgroundScanThread, NULL, 0, NULL);
        
        if (g_backgroundThread == NULL) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create background scan thread");
            InterlockedExchange(&g_refreshInProgress, 0);
            return FALSE;
        }
        
        // Set low priority
        SetThreadPriority(g_backgroundThread, THREAD_PRIORITY_BELOW_NORMAL);
    }
    
    WriteLog(LOG_LEVEL_INFO, "Resource cache initialized (background=%d)", startBackgroundScan);
    return TRUE;
}

void ResourceCache_Shutdown(void) {
    if (!g_initialized) {
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Shutting down resource cache system");
    
    g_shutdownRequested = TRUE;
    
    // Wait for background thread
    if (g_backgroundThread != NULL) {
        DWORD waitResult = WaitForSingleObject(g_backgroundThread, 5000); // 5 second timeout
        
        if (waitResult == WAIT_TIMEOUT) {
            WriteLog(LOG_LEVEL_ERROR, "Background thread did not exit gracefully within 5 seconds");
            WriteLog(LOG_LEVEL_ERROR, "Abandoning thread and leaking memory to avoid deadlock");
            // DO NOT call TerminateThread - can corrupt locks
            // DO NOT try to free memory - thread may be using it
            // Accept the leak as the safest option
            CloseHandle(g_backgroundThread);
            g_backgroundThread = NULL;
            g_initialized = FALSE;
            return;  // Early exit, skip cleanup
        }
        
        CloseHandle(g_backgroundThread);
        g_backgroundThread = NULL;
    }
    
    // Thread exited gracefully, safe to clean up
    AcquireSRWLockExclusive(&g_fontCache.lock);
    if (g_fontCache.entries) {
        free(g_fontCache.entries);
        g_fontCache.entries = NULL;
    }
    g_fontCache.count = 0;
    g_fontCache.capacity = 0;
    g_fontCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_fontCache.lock);
    
    AcquireSRWLockExclusive(&g_animCache.lock);
    if (g_animCache.entries) {
        free(g_animCache.entries);
        g_animCache.entries = NULL;
    }
    g_animCache.count = 0;
    g_animCache.capacity = 0;
    g_animCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_animCache.lock);
    
    g_initialized = FALSE;
    
    WriteLog(LOG_LEVEL_INFO, "Resource cache shutdown complete");
}

BOOL ResourceCache_IsReady(void) {
    return g_initialized;
}

/* ============================================================================
 * Public API - Font Cache
 * ============================================================================ */

CacheStatus FontCache_GetStatus(void) {
    if (!g_initialized) return CACHE_ERROR;
    
    // WARNING: Internal function - assumes caller holds g_fontCache.lock
    // Do not call directly from outside this module
    if (!g_fontCache.isValid) {
        return CACHE_INVALID;
    } else if (g_fontCache.count == 0) {
        return CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_fontCache.scanTime) > RESOURCE_CACHE_EXPIRY_SECONDS) {
            return CACHE_EXPIRED;
        } else {
            return CACHE_OK;
        }
    }
}

CacheStatus FontCache_GetEntries(const FontCacheEntry** outEntries, int* outCount) {
    if (!g_initialized || !outEntries || !outCount) {
        return CACHE_ERROR;
    }
    
    AcquireSRWLockShared(&g_fontCache.lock);
    
    // Inline status check to avoid nested lock calls
    CacheStatus status;
    if (!g_fontCache.isValid) {
        status = CACHE_INVALID;
    } else if (g_fontCache.count == 0) {
        status = CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_fontCache.scanTime) > RESOURCE_CACHE_EXPIRY_SECONDS) {
            status = CACHE_EXPIRED;
        } else {
            status = CACHE_OK;
        }
    }
    
    if (status == CACHE_OK || status == CACHE_EXPIRED) {
        *outEntries = g_fontCache.entries;
        *outCount = g_fontCache.count;
    } else {
        *outEntries = NULL;
        *outCount = 0;
    }
    
    ReleaseSRWLockShared(&g_fontCache.lock);
    return status;
}

void FontCache_RequestRefresh(void) {
    if (!g_initialized) return;
    
    // Atomically check and set refresh flag to prevent race conditions
    if (InterlockedCompareExchange(&g_refreshInProgress, 1, 0) != 0) {
        WriteLog(LOG_LEVEL_INFO, "Font cache refresh already in progress, skipping");
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Font cache refresh requested (async)");
    
    // Invalidate cache
    AcquireSRWLockExclusive(&g_fontCache.lock);
    g_fontCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_fontCache.lock);
    
    // Start background rescan
    HANDLE hThread = CreateThread(NULL, 0, BackgroundScanThread, NULL, 0, NULL);
    if (hThread) {
        SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(hThread); // Fire and forget
    } else {
        InterlockedExchange(&g_refreshInProgress, 0);
        WriteLog(LOG_LEVEL_ERROR, "Failed to create refresh thread");
    }
}

BOOL FontCache_RefreshSync(void) {
    if (!g_initialized) return FALSE;
    
    WriteLog(LOG_LEVEL_INFO, "Font cache refresh (sync)");
    
    AcquireSRWLockExclusive(&g_fontCache.lock);
    BOOL result = ScanFontFolder(&g_fontCache);
    ReleaseSRWLockExclusive(&g_fontCache.lock);
    
    return result;
}

void FontCache_UpdateCurrentFont(const char* fontRelativePath) {
    if (!g_initialized || !fontRelativePath) return;
    
    wchar_t wRelPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontRelativePath, -1, wRelPath, MAX_PATH);
    
    AcquireSRWLockExclusive(&g_fontCache.lock);
    
    // Clear all current flags
    for (int i = 0; i < g_fontCache.count; i++) {
        g_fontCache.entries[i].isCurrentFont = FALSE;
    }
    
    // Set new current
    for (int i = 0; i < g_fontCache.count; i++) {
        if (_wcsicmp(g_fontCache.entries[i].relativePath, wRelPath) == 0) {
            g_fontCache.entries[i].isCurrentFont = TRUE;
            break;
        }
    }
    
    ReleaseSRWLockExclusive(&g_fontCache.lock);
}

/* ============================================================================
 * Public API - Animation Cache
 * ============================================================================ */

CacheStatus AnimationCache_GetStatus(void) {
    if (!g_initialized) return CACHE_ERROR;
    
    // WARNING: Internal function - assumes caller holds g_animCache.lock
    // Do not call directly from outside this module
    if (!g_animCache.isValid) {
        return CACHE_INVALID;
    } else if (g_animCache.count == 0) {
        return CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_animCache.scanTime) > RESOURCE_CACHE_EXPIRY_SECONDS) {
            return CACHE_EXPIRED;
        } else {
            return CACHE_OK;
        }
    }
}

CacheStatus AnimationCache_GetEntries(const AnimationCacheEntry** outEntries, int* outCount) {
    if (!g_initialized || !outEntries || !outCount) {
        return CACHE_ERROR;
    }
    
    AcquireSRWLockShared(&g_animCache.lock);
    
    // Inline status check to avoid nested lock calls
    CacheStatus status;
    if (!g_animCache.isValid) {
        status = CACHE_INVALID;
    } else if (g_animCache.count == 0) {
        status = CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_animCache.scanTime) > RESOURCE_CACHE_EXPIRY_SECONDS) {
            status = CACHE_EXPIRED;
        } else {
            status = CACHE_OK;
        }
    }
    
    if (status == CACHE_OK || status == CACHE_EXPIRED) {
        *outEntries = g_animCache.entries;
        *outCount = g_animCache.count;
    } else {
        *outEntries = NULL;
        *outCount = 0;
    }
    
    ReleaseSRWLockShared(&g_animCache.lock);
    return status;
}

void AnimationCache_RequestRefresh(void) {
    if (!g_initialized) return;
    
    // Atomically check and set refresh flag to prevent race conditions
    if (InterlockedCompareExchange(&g_refreshInProgress, 1, 0) != 0) {
        WriteLog(LOG_LEVEL_INFO, "Animation cache refresh already in progress, skipping");
        return;
    }
    
    WriteLog(LOG_LEVEL_INFO, "Animation cache refresh requested (async)");
    
    // Invalidate cache
    AcquireSRWLockExclusive(&g_animCache.lock);
    g_animCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_animCache.lock);
    
    // Start background rescan
    HANDLE hThread = CreateThread(NULL, 0, BackgroundScanThread, NULL, 0, NULL);
    if (hThread) {
        SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
        CloseHandle(hThread); // Fire and forget
    } else {
        InterlockedExchange(&g_refreshInProgress, 0);
        WriteLog(LOG_LEVEL_ERROR, "Failed to create refresh thread");
    }
}

BOOL AnimationCache_RefreshSync(void) {
    if (!g_initialized) return FALSE;
    
    WriteLog(LOG_LEVEL_INFO, "Animation cache refresh (sync)");
    
    AcquireSRWLockExclusive(&g_animCache.lock);
    BOOL result = ScanAnimationFolder(&g_animCache);
    ReleaseSRWLockExclusive(&g_animCache.lock);
    
    return result;
}

void AnimationCache_UpdateCurrentAnimation(const char* animationName) {
    if (!g_initialized || !animationName) return;
    
    AcquireSRWLockExclusive(&g_animCache.lock);
    
    for (int i = 0; i < g_animCache.count; i++) {
        g_animCache.entries[i].isCurrent = 
            (strcmp(g_animCache.entries[i].fileName, animationName) == 0);
    }
    
    ReleaseSRWLockExclusive(&g_animCache.lock);
}

/* ============================================================================
 * Public API - Utilities
 * ============================================================================ */

void ResourceCache_GetStatistics(int* outFontCount, int* outAnimCount,
                                  time_t* outFontScanTime, time_t* outAnimScanTime) {
    if (outFontCount) {
        AcquireSRWLockShared(&g_fontCache.lock);
        *outFontCount = g_fontCache.count;
        ReleaseSRWLockShared(&g_fontCache.lock);
    }
    
    if (outAnimCount) {
        AcquireSRWLockShared(&g_animCache.lock);
        *outAnimCount = g_animCache.count;
        ReleaseSRWLockShared(&g_animCache.lock);
    }
    
    if (outFontScanTime) {
        AcquireSRWLockShared(&g_fontCache.lock);
        *outFontScanTime = g_fontCache.scanTime;
        ReleaseSRWLockShared(&g_fontCache.lock);
    }
    
    if (outAnimScanTime) {
        AcquireSRWLockShared(&g_animCache.lock);
        *outAnimScanTime = g_animCache.scanTime;
        ReleaseSRWLockShared(&g_animCache.lock);
    }
}

void ResourceCache_InvalidateAll(void) {
    if (!g_initialized) return;
    
    WriteLog(LOG_LEVEL_INFO, "Invalidating all resource caches");
    
    AcquireSRWLockExclusive(&g_fontCache.lock);
    g_fontCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_fontCache.lock);
    
    AcquireSRWLockExclusive(&g_animCache.lock);
    g_animCache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_animCache.lock);
}

BOOL ResourceCache_IsBackgroundScanComplete(void) {
    return g_backgroundScanComplete;
}
