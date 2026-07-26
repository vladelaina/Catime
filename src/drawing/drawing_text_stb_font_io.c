/**
 * @file drawing_text_stb_font_io.c
 * @brief Font mapping, file metadata, and rendering arithmetic helpers.
 */

#include "drawing_text_stb_internal.h"

void ReleaseMappedFont(unsigned char* buffer, HANDLE hFile, HANDLE hMapping) {
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

BOOL InitFontInfoFromBuffer(stbtt_fontinfo* fontInfo,
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

BOOL InitFontInfoFromBufferW(stbtt_fontinfo* fontInfo,
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

BOOL CalculateBitmapPixelCount(int width, int height, size_t* outPixelCount) {
    if (!outPixelCount || width <= 0 || height <= 0) return FALSE;

    size_t sw = (size_t)width;
    size_t sh = (size_t)height;
    if (sw > (size_t)-1 / sh) return FALSE;

    *outPixelCount = sw * sh;
    return TRUE;
}

BOOL ClipTextBitmapToDestination(int x, int y,
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

int ClampTextInt64(long long value) {
    if (value > (long long)INT_MAX) return INT_MAX;
    if (value < (long long)INT_MIN) return INT_MIN;
    return (int)value;
}

int AddTextIntClamped(int value, int delta) {
    return ClampTextInt64((long long)value + (long long)delta);
}

int MulTextIntClamped(int value, int factor) {
    return ClampTextInt64((long long)value * (long long)factor);
}

BOOL IsFontMappingSizeAllowed(HANDLE hFile, const wchar_t* pathForLog) {
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

BOOL GetFontFileInfoFromHandle(HANDLE hFile,
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

BOOL GetFontFileInfoFromPathW(const wchar_t* path,
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

BOOL GetFontFileInfoFromPathUtf8(const char* path,
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

BOOL IsCurrentMainFontFileStillCurrentLocked(const char* fontFilePath) {
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
