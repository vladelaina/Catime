/**
 * @file markdown_image_state.c
 * @brief Resolved-file metadata and common image path state helpers.
 */

#include "markdown_image_internal.h"
#include <limits.h>
#include <stdlib.h>

BOOL IsValidMarkdownImageNotifyWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }
    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

void WaitWhileLongEquals(volatile LONG* value, LONG expected) {
    DWORD spins = 0;
    while (InterlockedCompareExchange(value, 0, 0) == expected) {
        Sleep(spins++ < INIT_WAIT_SPIN_LIMIT ? 0 : 1);
    }
}

BOOL EnsureDirectoryExistsW(const wchar_t* path) {
    if (!path || !*path) {
        return FALSE;
    }
    if (CreateDirectoryW(path, NULL)) {
        return TRUE;
    }
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return FALSE;
    }

    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL GetExistingNonEmptyFileInfoW(const wchar_t* path,
                                  WIN32_FILE_ATTRIBUTE_DATA* attrsOut,
                                  ULONGLONG* sizeOut) {
    if (!path || !*path) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs) ||
        (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }

    ULONGLONG fileSize = ((ULONGLONG)attrs.nFileSizeHigh << 32) |
                         (ULONGLONG)attrs.nFileSizeLow;
    if (fileSize == 0) {
        return FALSE;
    }
    if (attrsOut) {
        *attrsOut = attrs;
    }
    if (sizeOut) {
        *sizeOut = fileSize;
    }
    return TRUE;
}

BOOL IsMarkdownImageFileUsable(const wchar_t* path) {
    return GetExistingNonEmptyFileInfoW(path, NULL, NULL);
}

void ClearMarkdownImageResolvedFileState(MarkdownImage* image) {
    if (!image) {
        return;
    }
    image->intrinsicWidth = 0;
    image->intrinsicHeight = 0;
    ZeroMemory(&image->resolvedLastWriteTime,
               sizeof(image->resolvedLastWriteTime));
    image->resolvedFileSize = 0;
    image->resolvedFileInfoValid = FALSE;
}

void StoreMarkdownImageResolvedFileState(
    MarkdownImage* image, const WIN32_FILE_ATTRIBUTE_DATA* attrs,
    ULONGLONG fileSize) {
    if (!image || !attrs) {
        return;
    }
    image->resolvedLastWriteTime = attrs->ftLastWriteTime;
    image->resolvedFileSize = fileSize;
    image->resolvedFileInfoValid = TRUE;
}

void FreeMarkdownImageResolvedPath(MarkdownImage* image) {
    if (!image) {
        return;
    }
    if (image->resolvedPath) {
        free(image->resolvedPath);
        image->resolvedPath = NULL;
    }
    ClearMarkdownImageResolvedFileState(image);
}

BOOL RefreshMarkdownImageResolvedFileState(MarkdownImage* image) {
    if (!image || !image->resolvedPath) {
        return FALSE;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    ULONGLONG fileSize = 0;
    if (!GetExistingNonEmptyFileInfoW(image->resolvedPath,
                                      &attrs, &fileSize)) {
        ClearMarkdownImageResolvedFileState(image);
        return FALSE;
    }

    if (image->resolvedFileInfoValid &&
        (CompareFileTime(&image->resolvedLastWriteTime,
                         &attrs.ftLastWriteTime) != 0 ||
         image->resolvedFileSize != fileSize)) {
        image->intrinsicWidth = 0;
        image->intrinsicHeight = 0;
    }
    StoreMarkdownImageResolvedFileState(image, &attrs, fileSize);
    return TRUE;
}

BOOL SetImageRenderRect(MarkdownImage* image, int x, int y,
                        int width, int height) {
    if (!image || width <= 0 || height <= 0) {
        return FALSE;
    }
    if (x > INT_MAX - width || y > INT_MAX - height) {
        return FALSE;
    }
    image->imageRect.left = x;
    image->imageRect.top = y;
    image->imageRect.right = x + width;
    image->imageRect.bottom = y + height;
    return TRUE;
}
