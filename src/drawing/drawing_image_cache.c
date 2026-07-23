/**
 * @file drawing_image_cache.c
 * @brief Validated, pixel-budgeted LRU cache for GDI+ images
 */
#include "drawing_image_gdiplus_internal.h"

#include <limits.h>
#include <string.h>

#define MAX_CACHED_IMAGES 16
#define IMAGE_CACHE_REVALIDATE_MS 1000
#define MAX_SINGLE_CACHED_IMAGE_PIXELS (4096u * 4096u)
#define MAX_TOTAL_CACHED_IMAGE_PIXELS (4096u * 4096u)

static CachedImageEntry g_imageCache[MAX_CACHED_IMAGES] = {0};
static size_t g_cachedImagePixels = 0;

static void ReleaseEntry(CachedImageEntry* entry) {
    if (!entry) return;
    if (entry->bitmap && g_drawingImageRuntime.disposeImage) {
        g_drawingImageRuntime.disposeImage((GpImage)entry->bitmap);
    }
    if (entry->inUse && entry->pixelCount <= g_cachedImagePixels) {
        g_cachedImagePixels -= entry->pixelCount;
    } else if (entry->inUse) {
        g_cachedImagePixels = 0;
    }
    ZeroMemory(entry, sizeof(*entry));
}

void DrawingImageCache_Clear(void) {
    for (int i = 0; i < MAX_CACHED_IMAGES; ++i) {
        ReleaseEntry(&g_imageCache[i]);
    }
    g_cachedImagePixels = 0;
}

static BOOL CalculatePixelCount(UINT width, UINT height, size_t* pixels) {
    if (!pixels || width == 0 || height == 0 ||
        (size_t)width > ((size_t)-1) / (size_t)height) {
        return FALSE;
    }
    *pixels = (size_t)width * (size_t)height;
    return TRUE;
}

static BOOL IsOlder(DWORD candidate, DWORD oldest, DWORD now) {
    return (DWORD)(now - candidate) > (DWORD)(now - oldest);
}

static size_t PixelsAfterReservedRelease(
    const CachedImageEntry* reservedEntry) {
    if (!reservedEntry || !reservedEntry->inUse) return g_cachedImagePixels;
    return reservedEntry->pixelCount > g_cachedImagePixels
        ? 0
        : g_cachedImagePixels - reservedEntry->pixelCount;
}

static BOOL MakeRoom(const CachedImageEntry* reservedEntry,
                     size_t newPixelCount) {
    DWORD now = GetTickCount();

    if (newPixelCount > MAX_SINGLE_CACHED_IMAGE_PIXELS ||
        newPixelCount > MAX_TOTAL_CACHED_IMAGE_PIXELS) {
        return FALSE;
    }
    while (PixelsAfterReservedRelease(reservedEntry) >
           MAX_TOTAL_CACHED_IMAGE_PIXELS - newPixelCount) {
        CachedImageEntry* oldest = NULL;
        for (int i = 0; i < MAX_CACHED_IMAGES; ++i) {
            CachedImageEntry* entry = &g_imageCache[i];
            if (!entry->inUse || entry == reservedEntry) continue;
            if (!oldest || IsOlder(entry->lastAccessTick,
                                   oldest->lastAccessTick, now)) {
                oldest = entry;
            }
        }
        if (!oldest) return FALSE;
        ReleaseEntry(oldest);
    }
    return TRUE;
}

static CachedImageEntry* FindCacheTarget(const wchar_t* path, DWORD now,
                                         ImageFileInfo* fileInfo,
                                         BOOL* hasFileInfo,
                                         CachedImageEntry** replacement) {
    CachedImageEntry* freeSlot = NULL;
    CachedImageEntry* oldest = NULL;

    for (int i = 0; i < MAX_CACHED_IMAGES; ++i) {
        CachedImageEntry* entry = &g_imageCache[i];
        if (!entry->inUse) {
            if (!freeSlot) freeSlot = entry;
            continue;
        }
        if (_wcsicmp(entry->path, path) == 0) {
            if (entry->bitmap &&
                (DWORD)(now - entry->lastValidateTick) <
                    IMAGE_CACHE_REVALIDATE_MS) {
                entry->lastAccessTick = now;
                return entry;
            }
            if (!DrawingImageCache_GetFileInfo(path, fileInfo)) {
                ReleaseEntry(entry);
                DrawingImageFailure_Record(path, NULL, FALSE, now);
                return NULL;
            }
            *hasFileInfo = TRUE;
            if (fileInfo->isDirectory) {
                ReleaseEntry(entry);
                DrawingImageFailure_Record(path, &fileInfo->lastWriteTime,
                                           TRUE, now);
                return NULL;
            }
            if (CompareFileTime(&entry->lastWriteTime,
                                &fileInfo->lastWriteTime) == 0 &&
                entry->fileSizeBytes == fileInfo->fileSizeBytes &&
                entry->bitmap) {
                entry->lastAccessTick = now;
                entry->lastValidateTick = now;
                return entry;
            }
            *replacement = entry;
            break;
        }
        if (!oldest || IsOlder(entry->lastAccessTick,
                               oldest->lastAccessTick, now)) {
            oldest = entry;
        }
    }
    return *replacement ? *replacement : (freeSlot ? freeSlot : oldest);
}

static CachedImageEntry* FailLoad(CachedImageEntry* replacement,
                                  const wchar_t* path,
                                  const FILETIME* writeTime,
                                  BOOL hasWriteTime, DWORD now) {
    if (replacement) ReleaseEntry(replacement);
    DrawingImageFailure_Record(path, writeTime, hasWriteTime, now);
    return NULL;
}

CachedImageEntry* DrawingImageCache_Get(const wchar_t* imagePath) {
    wchar_t path[MAX_PATH] = {0};
    DWORD now = GetTickCount();
    ImageFileInfo fileInfo = {0};
    BOOL hasFileInfo = FALSE;
    CachedImageEntry* replacement = NULL;
    CachedImageEntry* target;
    GpBitmap bitmap = NULL;
    UINT width = 0;
    UINT height = 0;
    size_t pixelCount = 0;

    if (!g_drawingImageRuntime.token || !imagePath ||
        !g_drawingImageRuntime.createBitmapFromFile ||
        !g_drawingImageRuntime.getImageWidth ||
        !g_drawingImageRuntime.getImageHeight ||
        !DrawingImageCache_CopyPath(imagePath, path)) {
        return NULL;
    }
    if (DrawingImageFailure_IsCached(path, NULL, FALSE, now)) return NULL;
    target = FindCacheTarget(path, now, &fileInfo, &hasFileInfo,
                             &replacement);
    if (!target || (target->inUse && target->bitmap && !replacement &&
                    _wcsicmp(target->path, path) == 0)) {
        return target;
    }
    if (!hasFileInfo && !DrawingImageCache_GetFileInfo(path, &fileInfo)) {
        return FailLoad(replacement, path, NULL, FALSE, now);
    }
    if (fileInfo.isDirectory) {
        return FailLoad(replacement, path, &fileInfo.lastWriteTime,
                        TRUE, now);
    }
    if (DrawingImageFailure_IsCached(path, &fileInfo.lastWriteTime,
                                     TRUE, now)) {
        if (replacement) ReleaseEntry(replacement);
        return NULL;
    }
    if (!DrawingImageCache_IsFileSizeAllowed(path, fileInfo.fileSizeBytes)) {
        return FailLoad(replacement, path, &fileInfo.lastWriteTime,
                        TRUE, now);
    }
    if (g_drawingImageRuntime.createBitmapFromFile(path, &bitmap) !=
            GDIPLUS_STATUS_OK ||
        !bitmap) {
        return FailLoad(replacement, path, &fileInfo.lastWriteTime,
                        TRUE, now);
    }
    if (g_drawingImageRuntime.getImageWidth((GpImage)bitmap, &width) !=
            GDIPLUS_STATUS_OK ||
        g_drawingImageRuntime.getImageHeight((GpImage)bitmap, &height) !=
            GDIPLUS_STATUS_OK ||
        width == 0 || height == 0 || width > INT_MAX || height > INT_MAX ||
        !CalculatePixelCount(width, height, &pixelCount) ||
        !MakeRoom(target, pixelCount)) {
        g_drawingImageRuntime.disposeImage((GpImage)bitmap);
        return FailLoad(replacement, path, &fileInfo.lastWriteTime,
                        TRUE, now);
    }

    DrawingImageFailure_Clear(path);
    if (target->inUse) ReleaseEntry(target);
    target->inUse = TRUE;
    target->bitmap = bitmap;
    target->width = width;
    target->height = height;
    target->pixelCount = pixelCount;
    target->lastWriteTime = fileInfo.lastWriteTime;
    target->fileSizeBytes = fileInfo.fileSizeBytes;
    target->lastAccessTick = now;
    target->lastValidateTick = now;
    g_cachedImagePixels += pixelCount;
    memcpy(target->path, path, sizeof(path));
    return target;
}
