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

static int ClampTextInt64(long long value) {
    if (value > (long long)INT_MAX) return INT_MAX;
    if (value < (long long)INT_MIN) return INT_MIN;
    return (int)value;
}

static int AddTextIntClamped(int value, int delta) {
    return ClampTextInt64((long long)value + (long long)delta);
}

static int MulTextIntClamped(int value, int factor) {
    return ClampTextInt64((long long)value * (long long)factor);
}

static BOOL IsFontMappingSizeAllowed(HANDLE hFile, const wchar_t* pathForLog) {
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

static BOOL GetFontFileInfoFromHandle(HANDLE hFile,
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

static BOOL GetFontFileInfoFromPathW(const wchar_t* path,
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

static BOOL GetFontFileInfoFromPathUtf8(const char* path,
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

static BOOL IsCurrentMainFontFileStillCurrentLocked(const char* fontFilePath) {
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
BOOL IsFontLoadedSTB(void) { return g_fontLoaded; }
BOOL IsFallbackFontLoadedSTB(void) { return g_fallbackFontLoaded; }
stbtt_fontinfo* GetMainFontInfoSTB(void) { return &g_fontInfo; }
stbtt_fontinfo* GetFallbackFontInfoSTB(void) { return &g_fallbackFontInfo; }

static unsigned char* LoadFontMappingW(const wchar_t* path, HANDLE* phFile, HANDLE* phMapping) {
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
