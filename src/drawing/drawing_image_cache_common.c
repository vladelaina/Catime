/**
 * @file drawing_image_cache_common.c
 * @brief Image file metadata and bounded failed-load suppression
 */
#include "drawing_image_gdiplus_internal.h"

#include "log.h"

#include <string.h>

#define MAX_FAILED_IMAGE_LOADS 256
#define IMAGE_FAILURE_RETRY_MS 3000
#define MAX_IMAGE_FILE_BYTES (32ull * 1024ull * 1024ull)

typedef struct {
    BOOL inUse;
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    BOOL hasWriteTime;
    DWORD retryAfterTick;
} FailedImageEntry;

static FailedImageEntry g_failedImages[MAX_FAILED_IMAGE_LOADS] = {0};

BOOL DrawingImageCache_CopyPath(const wchar_t* source,
                                wchar_t destination[MAX_PATH]) {
    size_t length = 0;

    if (!source || !destination) return FALSE;
    destination[0] = L'\0';
    while (length < MAX_PATH && source[length] != L'\0') ++length;
    if (length >= MAX_PATH) return FALSE;
    memcpy(destination, source, (length + 1) * sizeof(wchar_t));
    return TRUE;
}

static BOOL PathsEqual(const wchar_t* first, const wchar_t* second) {
    return first && second && _wcsicmp(first, second) == 0;
}

BOOL DrawingImageCache_GetFileInfo(const wchar_t* path,
                                   ImageFileInfo* info) {
    WIN32_FILE_ATTRIBUTE_DATA attributes;

    if (!path || !info ||
        !GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) {
        return FALSE;
    }
    info->lastWriteTime = attributes.ftLastWriteTime;
    info->fileSizeBytes =
        ((ULONGLONG)attributes.nFileSizeHigh << 32) |
        attributes.nFileSizeLow;
    info->isDirectory =
        (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return TRUE;
}

BOOL DrawingImageCache_IsFileSizeAllowed(const wchar_t* path,
                                         ULONGLONG fileSize) {
    if (!path) return FALSE;
    if (fileSize > MAX_IMAGE_FILE_BYTES) {
        LOG_WARNING("Image file is too large: %ls (%llu bytes; limit %llu)",
                    path, fileSize, (ULONGLONG)MAX_IMAGE_FILE_BYTES);
        return FALSE;
    }
    return TRUE;
}

static BOOL FailureVersionMatches(const FailedImageEntry* entry,
                                  const FILETIME* writeTime,
                                  BOOL hasWriteTime) {
    if (!entry || entry->hasWriteTime != hasWriteTime) return FALSE;
    return !hasWriteTime ||
           (writeTime && CompareFileTime(&entry->lastWriteTime,
                                         writeTime) == 0);
}

BOOL DrawingImageFailure_IsCached(const wchar_t* path,
                                  const FILETIME* writeTime,
                                  BOOL hasWriteTime, DWORD now) {
    wchar_t cachePath[MAX_PATH] = {0};

    if (!DrawingImageCache_CopyPath(path, cachePath)) return FALSE;
    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; ++i) {
        FailedImageEntry* entry = &g_failedImages[i];
        if (!entry->inUse || !PathsEqual(entry->path, cachePath)) continue;
        if ((LONG)(entry->retryAfterTick - now) <= 0) {
            ZeroMemory(entry, sizeof(*entry));
            return FALSE;
        }
        if (!entry->hasWriteTime) {
            ImageFileInfo currentInfo = {0};
            if (DrawingImageCache_GetFileInfo(cachePath, &currentInfo) &&
                !currentInfo.isDirectory) {
                ZeroMemory(entry, sizeof(*entry));
                return FALSE;
            }
        }
        return FailureVersionMatches(entry, writeTime, hasWriteTime);
    }
    return FALSE;
}

static FailedImageEntry* FindFailureSlot(const wchar_t* path, DWORD now) {
    FailedImageEntry* freeSlot = NULL;
    FailedImageEntry* oldestSlot = NULL;

    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; ++i) {
        FailedImageEntry* entry = &g_failedImages[i];
        if (!entry->inUse || (LONG)(entry->retryAfterTick - now) <= 0) {
            if (entry->inUse) ZeroMemory(entry, sizeof(*entry));
            if (!freeSlot) freeSlot = entry;
            continue;
        }
        if (PathsEqual(entry->path, path)) return entry;
        if (!oldestSlot ||
            (LONG)(oldestSlot->retryAfterTick - entry->retryAfterTick) > 0) {
            oldestSlot = entry;
        }
    }
    return freeSlot ? freeSlot : oldestSlot;
}

void DrawingImageFailure_Record(const wchar_t* path,
                                const FILETIME* writeTime,
                                BOOL hasWriteTime, DWORD now) {
    wchar_t cachePath[MAX_PATH] = {0};
    FailedImageEntry* target;
    DWORD retryAt;

    if (!DrawingImageCache_CopyPath(path, cachePath)) return;
    target = FindFailureSlot(cachePath, now);
    if (!target) return;
    ZeroMemory(target, sizeof(*target));
    target->inUse = TRUE;
    target->hasWriteTime = hasWriteTime;
    if (hasWriteTime && writeTime) target->lastWriteTime = *writeTime;
    retryAt = now + IMAGE_FAILURE_RETRY_MS;
    target->retryAfterTick = retryAt ? retryAt : 1;
    memcpy(target->path, cachePath, sizeof(cachePath));
}

void DrawingImageFailure_Clear(const wchar_t* path) {
    wchar_t cachePath[MAX_PATH] = {0};

    if (!DrawingImageCache_CopyPath(path, cachePath)) return;
    for (int i = 0; i < MAX_FAILED_IMAGE_LOADS; ++i) {
        if (g_failedImages[i].inUse &&
            PathsEqual(g_failedImages[i].path, cachePath)) {
            ZeroMemory(&g_failedImages[i], sizeof(g_failedImages[i]));
            return;
        }
    }
}

void DrawingImageFailure_Reset(void) {
    ZeroMemory(g_failedImages, sizeof(g_failedImages));
}
