/**
 * @file drawing_render_images.c
 * @brief Per-frame markdown image preparation and ownership.
 */

#include "drawing_render_internal.h"

void FreePaintMarkdownImages(MarkdownImage* images, int imageCount, BOOL heapAllocated) {
    if (!images) return;

    if (heapAllocated) {
        FreeMarkdownImages(images, imageCount);
    } else {
        FreeMarkdownImageEntries(images, imageCount);
    }
}

BOOL HasTickReached(DWORD tick) {
    return (DWORD)(GetTickCount() - tick) < 0x80000000u;
}

BOOL IsMarkdownImageRetryPending(const MarkdownImage* image) {
    return image && image->downloadFailed && image->downloadRetryScheduled &&
           !HasTickReached(image->downloadRetryTick);
}

void PreparePaintMarkdownImagesForFrame(MarkdownImage* images, int imageCount) {
    if (!images || imageCount <= 0) return;

    DWORD now = GetTickCount();
    BOOL checkResolvedFiles = s_nextMarkdownImageFileCheckTick == 0 ||
                              HasTickReached(s_nextMarkdownImageFileCheckTick);
    if (checkResolvedFiles) {
        s_nextMarkdownImageFileCheckTick = now + MARKDOWN_IMAGE_FILE_RECHECK_MS;
    }

    for (int i = 0; i < imageCount; i++) {
        SetRectEmpty(&images[i].imageRect);

        if (checkResolvedFiles &&
            images[i].resolvedPath &&
            !RefreshMarkdownImageResolvedFileState(&images[i])) {
            free(images[i].resolvedPath);
            images[i].resolvedPath = NULL;
            images[i].isDownloaded = FALSE;
            images[i].intrinsicWidth = 0;
            images[i].intrinsicHeight = 0;
            ZeroMemory(&images[i].resolvedLastWriteTime,
                       sizeof(images[i].resolvedLastWriteTime));
            images[i].resolvedFileSize = 0;
            images[i].resolvedFileInfoValid = FALSE;
        }

        if (images[i].isNetworkImage) {
            if (!images[i].isDownloaded) {
                images[i].intrinsicWidth = 0;
                images[i].intrinsicHeight = 0;

                if (images[i].isDownloading &&
                    !IsMarkdownImageDownloadInProgress(images[i].imagePath)) {
                    images[i].isDownloading = FALSE;
                    DWORD retryTick = 0;
                    if (GetMarkdownImageDownloadRetryTick(images[i].imagePath, &retryTick)) {
                        images[i].downloadFailed = TRUE;
                        images[i].downloadRetryScheduled = TRUE;
                        images[i].downloadRetryTick = retryTick;
                    }
                }

                if (IsMarkdownImageRetryPending(&images[i])) {
                    continue;
                }

                if (images[i].downloadRetryScheduled &&
                    HasTickReached(images[i].downloadRetryTick)) {
                    images[i].downloadRetryScheduled = FALSE;
                    images[i].downloadRetryTick = 0;
                    images[i].downloadFailed = FALSE;
                } else if (!images[i].downloadRetryScheduled) {
                    images[i].downloadFailed = FALSE;
                }
            }
        }
    }
}

BOOL EnsurePaintMarkdownImageCapacity(MarkdownImage** images,
                                             int* imageCapacity,
                                             BOOL* heapAllocated,
                                             MarkdownImage* stackImages) {
    if (!images || !*images || !imageCapacity || !heapAllocated || !stackImages) {
        return FALSE;
    }

    int oldCapacity = *imageCapacity;
    if (oldCapacity <= 0 ||
        (size_t)oldCapacity > ((size_t)-1) / 2u / sizeof(MarkdownImage)) {
        return FALSE;
    }

    int newCapacity = oldCapacity * 2;
    MarkdownImage* newImages = NULL;

    if (*heapAllocated) {
        newImages = (MarkdownImage*)realloc(*images, (size_t)newCapacity * sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        ZeroMemory(newImages + oldCapacity, (size_t)(newCapacity - oldCapacity) * sizeof(MarkdownImage));
    } else {
        newImages = (MarkdownImage*)calloc((size_t)newCapacity, sizeof(MarkdownImage));
        if (!newImages) return FALSE;
        memcpy(newImages, stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        ZeroMemory(stackImages, (size_t)oldCapacity * sizeof(MarkdownImage));
        *heapAllocated = TRUE;
    }

    *images = newImages;
    *imageCapacity = newCapacity;
    return TRUE;
}

MarkdownImage* MovePaintMarkdownImagesToHeap(MarkdownImage* images,
                                                    int imageCount,
                                                    BOOL* heapAllocated,
                                                    MarkdownImage* stackImages) {
    if (!images || imageCount <= 0 || !heapAllocated) {
        return NULL;
    }

    if (*heapAllocated) {
        *heapAllocated = FALSE;
        return images;
    }

    MarkdownImage* heapImages =
        (MarkdownImage*)calloc((size_t)imageCount, sizeof(MarkdownImage));
    if (!heapImages) {
        return NULL;
    }

    memcpy(heapImages, images, (size_t)imageCount * sizeof(MarkdownImage));
    if (images == stackImages && stackImages) {
        ZeroMemory(stackImages, (size_t)imageCount * sizeof(MarkdownImage));
    }
    return heapImages;
}
