/**
 * @file font_cache.c
 * @brief Font resource cache implementation
 */

#include "cache/font_cache.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_RECURSION_DEPTH 10
#define CACHE_EXPIRY_SECONDS 60

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct {
    FontCacheEntry* entries;
    int count;
    int capacity;
    time_t scanTime;
    BOOL isValid;
    SRWLOCK lock;
} FontCacheInternal;

static FontCacheInternal g_cache = {NULL, 0, 0, 0, FALSE, {0}};

/* ============================================================================
 * Path Management
 * ============================================================================ */

static BOOL GetFontsFolderPath(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 20 >= size) return FALSE;
    
    int written = _snwprintf_s(outPath, size, _TRUNCATE, L"%.*ls\\resources\\fonts", (int)dirLen, wconfigPath);
    if (written < 0) return FALSE;
    
    return TRUE;
}

static void GetCurrentFontRelativePath(char* outPath, size_t size) {
    extern char FONT_FILE_NAME[MAX_PATH];
    const char* prefix = FONTS_PATH_PREFIX;
    
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

/* ============================================================================
 * Cache Entry Management
 * ============================================================================ */

static BOOL AddFontEntry(FontCacheInternal* cache, const wchar_t* fileName,
                         const wchar_t* fullPath, const wchar_t* relativePath,
                         int depth) {
    if (cache->count >= cache->capacity) {
        int newCapacity;
        if (cache->capacity == 0) {
            newCapacity = 32;
        } else {
            // Check for overflow before doubling
            if (cache->capacity > MAX_FONT_CACHE_ENTRIES / 2) {
                newCapacity = MAX_FONT_CACHE_ENTRIES;
            } else {
                newCapacity = cache->capacity * 2;
            }
        }
        
        if (newCapacity > MAX_FONT_CACHE_ENTRIES) {
            WriteLog(LOG_LEVEL_WARNING, "Font cache capacity limit reached (%d fonts), skipping: %ls",
                     MAX_FONT_CACHE_ENTRIES, fileName);
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

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

static void ScanFontFolderRecursive(const wchar_t* folderPath,
                                    const wchar_t* basePath,
                                    const wchar_t* relativePath,
                                    FontCacheInternal* cache,
                                    int depth) {
    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }
    
    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0) {
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
        int len1 = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0) continue;
        
        wchar_t newRelativePath[MAX_PATH];
        if (!relativePath || relativePath[0] == L'\0') {
            wcsncpy(newRelativePath, findData.cFileName, MAX_PATH - 1);
            newRelativePath[MAX_PATH - 1] = L'\0';
        } else {
            int len2 = _snwprintf_s(newRelativePath, MAX_PATH, _TRUNCATE, L"%s\\%s", relativePath, findData.cFileName);
            if (len2 < 0) continue;
        }
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanFontFolderRecursive(fullPath, basePath, newRelativePath, cache, depth + 1);
        } else {
            wchar_t* ext = wcsrchr(findData.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                if (!AddFontEntry(cache, findData.cFileName, fullPath, newRelativePath, depth)) {
                    FindClose(hFind);
                    return;
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

static BOOL ScanFontsInternal(FontCacheInternal* cache) {
    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderPath(fontsPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to get fonts folder path");
        return FALSE;
    }
    
    DWORD attribs = GetFileAttributesW(fontsPath);
    if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Fonts folder does not exist: %ls", fontsPath);
        return FALSE;
    }
    
    // Memory optimization: shrink if underused
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
    
    cache->count = 0;
    
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
 * Public API Implementation
 * ============================================================================ */

BOOL FontCache_Initialize(void) {
    InitializeSRWLock(&g_cache.lock);
    g_cache.isValid = FALSE;
    g_cache.count = 0;
    g_cache.capacity = 0;
    g_cache.entries = NULL;
    WriteLog(LOG_LEVEL_INFO, "Font cache initialized");
    return TRUE;
}

void FontCache_Shutdown(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    if (g_cache.entries) {
        free(g_cache.entries);
        g_cache.entries = NULL;
    }
    g_cache.count = 0;
    g_cache.capacity = 0;
    g_cache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_cache.lock);
    WriteLog(LOG_LEVEL_INFO, "Font cache shutdown");
}

BOOL FontCache_Scan(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    BOOL result = ScanFontsInternal(&g_cache);
    ReleaseSRWLockExclusive(&g_cache.lock);
    return result;
}

FontCacheStatus FontCache_GetEntries(FontCacheEntry** outEntries, int* outCount) {
    if (!outEntries || !outCount) {
        return FONT_CACHE_ERROR;
    }
    
    /* Use blocking acquire - scan is fast (milliseconds), better than showing "Loading" */
    AcquireSRWLockShared(&g_cache.lock);
    
    FontCacheStatus status;
    if (!g_cache.isValid) {
        status = FONT_CACHE_INVALID;
    } else if (g_cache.count == 0) {
        status = FONT_CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_cache.scanTime) > CACHE_EXPIRY_SECONDS) {
            status = FONT_CACHE_EXPIRED;
        } else {
            status = FONT_CACHE_OK;
        }
    }
    
    if (status == FONT_CACHE_OK || status == FONT_CACHE_EXPIRED) {
        *outCount = g_cache.count;
        if (g_cache.count > 0) {
            *outEntries = (FontCacheEntry*)malloc(g_cache.count * sizeof(FontCacheEntry));
            if (*outEntries) {
                memcpy(*outEntries, g_cache.entries, g_cache.count * sizeof(FontCacheEntry));
            } else {
                // Allocation failure
                *outCount = 0;
                status = FONT_CACHE_ERROR;
            }
        } else {
            *outEntries = NULL;
        }
    } else {
        *outEntries = NULL;
        *outCount = 0;
    }
    
    ReleaseSRWLockShared(&g_cache.lock);
    return status;
}

void FontCache_Invalidate(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    g_cache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_cache.lock);
    WriteLog(LOG_LEVEL_INFO, "Font cache invalidated");
}

void FontCache_UpdateCurrent(const char* fontRelativePath) {
    if (!fontRelativePath) return;
    
    wchar_t wRelPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fontRelativePath, -1, wRelPath, MAX_PATH);
    
    AcquireSRWLockExclusive(&g_cache.lock);
    
    for (int i = 0; i < g_cache.count; i++) {
        g_cache.entries[i].isCurrentFont = FALSE;
    }
    
    for (int i = 0; i < g_cache.count; i++) {
        if (_wcsicmp(g_cache.entries[i].relativePath, wRelPath) == 0) {
            g_cache.entries[i].isCurrentFont = TRUE;
            break;
        }
    }
    
    ReleaseSRWLockExclusive(&g_cache.lock);
}

void FontCache_GetStatistics(int* outCount, time_t* outScanTime) {
    AcquireSRWLockShared(&g_cache.lock);
    if (outCount) *outCount = g_cache.count;
    if (outScanTime) *outScanTime = g_cache.scanTime;
    ReleaseSRWLockShared(&g_cache.lock);
}

BOOL FontCache_IsValid(void) {
    AcquireSRWLockShared(&g_cache.lock);
    BOOL valid = g_cache.isValid;
    ReleaseSRWLockShared(&g_cache.lock);
    return valid;
}
