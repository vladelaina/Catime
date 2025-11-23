/**
 * @file animation_cache.c
 * @brief Animation resource cache implementation
 */

#include "cache/animation_cache.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CACHE_EXPIRY_SECONDS 60

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct {
    AnimationCacheEntry* entries;
    int count;
    int capacity;
    time_t scanTime;
    BOOL isValid;
    SRWLOCK lock;
} AnimationCacheInternal;

static AnimationCacheInternal g_cache = {NULL, 0, 0, 0, FALSE, {0}};

/* ============================================================================
 * Path Management
 * ============================================================================ */

static BOOL GetAnimationsFolderPathInternal(wchar_t* outPath, size_t size) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    wchar_t wconfigPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    wchar_t* lastSep = wcsrchr(wconfigPath, L'\\');
    if (!lastSep) return FALSE;
    
    size_t dirLen = (size_t)(lastSep - wconfigPath);
    if (dirLen + 25 >= size) return FALSE;
    
    int written = _snwprintf(outPath, size, L"%.*ls\\resources\\animations", (int)dirLen, wconfigPath);
    if (written < 0 || written >= (int)size) return FALSE;
    
    return TRUE;
}

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
 * Cache Entry Management
 * ============================================================================ */

static BOOL AddAnimationEntry(AnimationCacheInternal* cache, const char* fileName,
                               const wchar_t* fullPath, const char* relativePath,
                               int depth, BOOL isSpecial) {
    if (cache->count >= cache->capacity) {
        int newCapacity;
        if (cache->capacity == 0) {
            newCapacity = 32;
        } else {
            // Check for overflow before doubling
            if (cache->capacity > MAX_ANIM_CACHE_ENTRIES / 2) {
                newCapacity = MAX_ANIM_CACHE_ENTRIES;
            } else {
                newCapacity = cache->capacity * 2;
            }
        }
        
        if (newCapacity > MAX_ANIM_CACHE_ENTRIES) {
            WriteLog(LOG_LEVEL_WARNING, "Animation cache capacity limit reached (%d animations), skipping: %s",
                     MAX_ANIM_CACHE_ENTRIES, fileName);
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
    entry->depth = depth;
    
    cache->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

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

static void ScanAnimationsRecursive(const wchar_t* folderPath,
                                  const wchar_t* basePath,
                                  const char* relativePathUtf8,
                                  AnimationCacheInternal* cache,
                                  int depth) {
    if (depth >= 10) { // Max recursion depth
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf(searchPath, MAX_PATH, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) return;

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        wchar_t fullPath[MAX_PATH];
        int len1 = _snwprintf(fullPath, MAX_PATH, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0 || len1 >= MAX_PATH) continue;

        char fileNameUtf8[MAX_ANIM_NAME_LENGTH];
        WideCharToMultiByte(CP_UTF8, 0, findData.cFileName, -1,
                           fileNameUtf8, MAX_ANIM_NAME_LENGTH, NULL, NULL);

        char newRelativePath[MAX_PATH];
        if (relativePathUtf8[0] == '\0') {
            strncpy(newRelativePath, fileNameUtf8, MAX_PATH - 1);
        } else {
            _snprintf(newRelativePath, MAX_PATH, "%s\\%s", relativePathUtf8, fileNameUtf8);
        }
        newRelativePath[MAX_PATH - 1] = '\0';

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanAnimationsRecursive(fullPath, basePath, newRelativePath, cache, depth + 1);
        } else {
            if (IsAnimationFile(findData.cFileName)) {
                if (!AddAnimationEntry(cache, fileNameUtf8, fullPath, newRelativePath, depth, FALSE)) {
                    FindClose(hFind);
                    return;
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static BOOL ScanAnimationsInternal(AnimationCacheInternal* cache) {
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
    
    // Memory optimization: shrink if underused
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
    AddAnimationEntry(cache, "__logo__", L"", "__logo__", 0, TRUE);
    AddAnimationEntry(cache, "__cpu__", L"", "__cpu__", 0, TRUE);
    AddAnimationEntry(cache, "__mem__", L"", "__mem__", 0, TRUE);
    
    // Scan folder recursively
    ScanAnimationsRecursive(animPath, animPath, "", cache, 0);
    
    // Mark current animation
    char currentAnim[MAX_ANIM_NAME_LENGTH];
    GetCurrentAnimationNameInternal(currentAnim, MAX_ANIM_NAME_LENGTH);
    
    if (currentAnim[0] != '\0') {
        for (int i = 0; i < cache->count; i++) {
            if (strcmp(cache->entries[i].relativePath, currentAnim) == 0) {
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
 * Public API Implementation
 * ============================================================================ */

BOOL AnimationCache_Initialize(void) {
    InitializeSRWLock(&g_cache.lock);
    g_cache.isValid = FALSE;
    g_cache.count = 0;
    g_cache.capacity = 0;
    g_cache.entries = NULL;
    WriteLog(LOG_LEVEL_INFO, "Animation cache initialized");
    return TRUE;
}

void AnimationCache_Shutdown(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    if (g_cache.entries) {
        free(g_cache.entries);
        g_cache.entries = NULL;
    }
    g_cache.count = 0;
    g_cache.capacity = 0;
    g_cache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_cache.lock);
    WriteLog(LOG_LEVEL_INFO, "Animation cache shutdown");
}

BOOL AnimationCache_Scan(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    BOOL result = ScanAnimationsInternal(&g_cache);
    ReleaseSRWLockExclusive(&g_cache.lock);
    return result;
}

AnimationCacheStatus AnimationCache_GetEntries(AnimationCacheEntry** outEntries, int* outCount) {
    if (!outEntries || !outCount) {
        return ANIM_CACHE_ERROR;
    }
    
    AcquireSRWLockShared(&g_cache.lock);
    
    AnimationCacheStatus status;
    if (!g_cache.isValid) {
        status = ANIM_CACHE_INVALID;
    } else if (g_cache.count == 0) {
        status = ANIM_CACHE_EMPTY;
    } else {
        time_t now = time(NULL);
        if ((now - g_cache.scanTime) > CACHE_EXPIRY_SECONDS) {
            status = ANIM_CACHE_EXPIRED;
        } else {
            status = ANIM_CACHE_OK;
        }
    }
    
    if (status == ANIM_CACHE_OK || status == ANIM_CACHE_EXPIRED) {
        *outCount = g_cache.count;
        if (g_cache.count > 0) {
            *outEntries = (AnimationCacheEntry*)malloc(g_cache.count * sizeof(AnimationCacheEntry));
            if (*outEntries) {
                memcpy(*outEntries, g_cache.entries, g_cache.count * sizeof(AnimationCacheEntry));
            } else {
                // Allocation failure
                *outCount = 0;
                status = ANIM_CACHE_ERROR;
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

void AnimationCache_Invalidate(void) {
    AcquireSRWLockExclusive(&g_cache.lock);
    g_cache.isValid = FALSE;
    ReleaseSRWLockExclusive(&g_cache.lock);
    WriteLog(LOG_LEVEL_INFO, "Animation cache invalidated");
}

void AnimationCache_UpdateCurrent(const char* animationName) {
    if (!animationName) return;
    
    AcquireSRWLockExclusive(&g_cache.lock);
    
    for (int i = 0; i < g_cache.count; i++) {
        g_cache.entries[i].isCurrent =
            (strcmp(g_cache.entries[i].fileName, animationName) == 0);
    }
    
    ReleaseSRWLockExclusive(&g_cache.lock);
}

void AnimationCache_GetStatistics(int* outCount, time_t* outScanTime) {
    AcquireSRWLockShared(&g_cache.lock);
    if (outCount) *outCount = g_cache.count;
    if (outScanTime) *outScanTime = g_cache.scanTime;
    ReleaseSRWLockShared(&g_cache.lock);
}

BOOL AnimationCache_IsValid(void) {
    AcquireSRWLockShared(&g_cache.lock);
    BOOL valid = g_cache.isValid;
    ReleaseSRWLockShared(&g_cache.lock);
    return valid;
}
