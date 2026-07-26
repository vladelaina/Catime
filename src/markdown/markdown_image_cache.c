/**
 * @file markdown_image_cache.c
 * @brief Cache filename validation and size/age based pruning.
 */

#include "markdown_image_internal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>

static BOOL IsHexDigitW(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') ||
           (ch >= L'a' && ch <= L'f') ||
           (ch >= L'A' && ch <= L'F');
}

static BOOL IsGeneratedImageCacheFileName(const wchar_t* name) {
    if (!name) {
        return FALSE;
    }
    const wchar_t* ext = wcsrchr(name, L'.');
    if (!ext || ext - name != 16) {
        return FALSE;
    }
    for (int i = 0; i < 16; i++) {
        if (!IsHexDigitW(name[i])) {
            return FALSE;
        }
    }
    return _wcsicmp(ext, L".png") == 0 ||
           _wcsicmp(ext, L".jpg") == 0 ||
           _wcsicmp(ext, L".gif") == 0 ||
           _wcsicmp(ext, L".bmp") == 0 ||
           _wcsicmp(ext, L".webp") == 0;
}

static int CompareImageCachePruneEntryByAge(const void* lhs,
                                             const void* rhs) {
    const ImageCachePruneEntry* a = (const ImageCachePruneEntry*)lhs;
    const ImageCachePruneEntry* b = (const ImageCachePruneEntry*)rhs;
    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime);
}

static ULONGLONG AddImageCacheBytesSaturated(ULONGLONG total,
                                              ULONGLONG value) {
    const ULONGLONG maxValue = (ULONGLONG)~0ull;
    return value > maxValue - total ? maxValue : total + value;
}

static int FindNewestImageCachePruneEntry(
    const ImageCachePruneEntry* entries, int entryCount) {
    int newest = 0;
    for (int i = 1; i < entryCount; i++) {
        if (CompareFileTime(&entries[newest].lastWriteTime,
                            &entries[i].lastWriteTime) < 0) {
            newest = i;
        }
    }
    return newest;
}

static BOOL AddImageCachePruneEntry(ImageCachePruneEntry** entries,
                                    int* entryCount,
                                    int* entryCapacity,
                                    int* newestEntry,
                                    const wchar_t* path,
                                    const FILETIME* lastWriteTime,
                                    ULONGLONG size) {
    if (!entries || !entryCount || !entryCapacity || !newestEntry ||
        !path || !lastWriteTime) {
        return FALSE;
    }

    if (*entryCount < IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
        if (*entryCount == *entryCapacity) {
            int newCapacity = *entryCapacity == 0
                ? 64
                : *entryCapacity * 2;
            if (newCapacity > IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
                newCapacity = IMAGE_CACHE_PRUNE_SCAN_LIMIT;
            }
            ImageCachePruneEntry* newEntries =
                (ImageCachePruneEntry*)realloc(
                    *entries,
                    (size_t)newCapacity * sizeof(ImageCachePruneEntry));
            if (!newEntries) {
                return FALSE;
            }
            *entries = newEntries;
            *entryCapacity = newCapacity;
        }

        ImageCachePruneEntry* entry = &(*entries)[(*entryCount)++];
        wcscpy_s(entry->path, MAX_PATH, path);
        entry->lastWriteTime = *lastWriteTime;
        entry->size = size;
        if (*entryCount == 1 ||
            CompareFileTime(&(*entries)[*newestEntry].lastWriteTime,
                            lastWriteTime) < 0) {
            *newestEntry = *entryCount - 1;
        }
        return TRUE;
    }

    if (*entryCount <= 0 ||
        CompareFileTime(lastWriteTime,
                        &(*entries)[*newestEntry].lastWriteTime) >= 0) {
        return *entryCount > 0;
    }

    ImageCachePruneEntry* entry = &(*entries)[*newestEntry];
    wcscpy_s(entry->path, MAX_PATH, path);
    entry->lastWriteTime = *lastWriteTime;
    entry->size = size;
    *newestEntry = FindNewestImageCachePruneEntry(*entries, *entryCount);
    return TRUE;
}

void PruneImageCacheDirectory(const wchar_t* cacheDir,
                              const wchar_t* keepPath) {
    if (!cacheDir || !*cacheDir) {
        return;
    }

    wchar_t searchPath[MAX_PATH];
    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE,
                     L"%s\\*", cacheDir) < 0) {
        return;
    }

    int totalRemoved = 0;
    for (;;) {
        ImageCachePruneEntry* entries = NULL;
        int entryCount = 0;
        int entryCapacity = 0;
        int newestEntry = 0;
        int cacheFileCount = 0;
        ULONGLONG cacheBytes = 0;

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            break;
        }

        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                !IsGeneratedImageCacheFileName(findData.cFileName)) {
                continue;
            }

            wchar_t fullPath[MAX_PATH];
            if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE,
                             L"%s\\%s", cacheDir,
                             findData.cFileName) < 0) {
                continue;
            }

            ULONGLONG fileSize =
                ((ULONGLONG)findData.nFileSizeHigh << 32) |
                (ULONGLONG)findData.nFileSizeLow;
            cacheFileCount++;
            cacheBytes = AddImageCacheBytesSaturated(cacheBytes, fileSize);
            if (keepPath && _wcsicmp(fullPath, keepPath) == 0) {
                continue;
            }
            if (!AddImageCachePruneEntry(
                    &entries, &entryCount, &entryCapacity,
                    &newestEntry, fullPath,
                    &findData.ftLastWriteTime, fileSize)) {
                break;
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);

        if ((cacheBytes <= IMAGE_CACHE_MAX_BYTES &&
             cacheFileCount <= IMAGE_CACHE_MAX_FILES) ||
            !entries || entryCount == 0) {
            free(entries);
            break;
        }

        qsort(entries, (size_t)entryCount, sizeof(entries[0]),
              CompareImageCachePruneEntryByAge);
        int removedThisPass = 0;
        for (int i = 0;
             i < entryCount &&
             (cacheBytes > IMAGE_CACHE_MAX_BYTES ||
              cacheFileCount > IMAGE_CACHE_MAX_FILES);
             i++) {
            if (DeleteFileW(entries[i].path)) {
                cacheBytes = entries[i].size <= cacheBytes
                    ? cacheBytes - entries[i].size
                    : 0;
                if (cacheFileCount > 0) {
                    cacheFileCount--;
                }
                removedThisPass++;
            }
        }
        free(entries);
        if (removedThisPass == 0) {
            break;
        }
        totalRemoved += removedThisPass;
    }

    if (totalRemoved > 0) {
        LOG_INFO("Pruned %d cached markdown image file(s)", totalRemoved);
    }
}
