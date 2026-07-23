}

static BOOL SetImageRenderRect(MarkdownImage* image, int x, int y,
                               int width, int height) {
    if (!image || width <= 0 || height <= 0) return FALSE;
    if (x > INT_MAX - width || y > INT_MAX - height) return FALSE;

    image->imageRect.left = x;
    image->imageRect.top = y;
    image->imageRect.right = x + width;
    image->imageRect.bottom = y + height;
    return TRUE;
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

BOOL GetPluginsDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_pluginsDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_pluginsDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_pluginsDirInit, IMAGE_CACHE_DIR_INITIALIZING);

        if (InterlockedCompareExchange(&g_pluginsDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }

        if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_pluginsDir);
        return TRUE;
    }

    char pluginsDirUtf8[MAX_PATH] = {0};
    GetPluginsFolderPath(pluginsDirUtf8, MAX_PATH);
    if (pluginsDirUtf8[0] == '\0' ||
        MultiByteToWideChar(CP_UTF8, 0, pluginsDirUtf8, -1,
                            g_pluginsDir, MAX_PATH) <= 0) {
        InterlockedExchange(&g_pluginsDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    InterlockedExchange(&g_pluginsDirInit, IMAGE_CACHE_DIR_INITIALIZED);

    if (wcslen(g_pluginsDir) >= bufferSize) return FALSE;
    wcscpy_s(buffer, bufferSize, g_pluginsDir);
    return TRUE;
}

BOOL GetImageCacheDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    LONG initState = InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0);
    if (initState == IMAGE_CACHE_DIR_INITIALIZED) {
        if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    if (InterlockedCompareExchange(&g_imageCacheDirInit,
                                   IMAGE_CACHE_DIR_INITIALIZING,
                                   IMAGE_CACHE_DIR_UNINITIALIZED) !=
        IMAGE_CACHE_DIR_UNINITIALIZED) {
        WaitWhileLongEquals(&g_imageCacheDirInit, IMAGE_CACHE_DIR_INITIALIZING);

        if (InterlockedCompareExchange(&g_imageCacheDirInit, 0, 0) !=
            IMAGE_CACHE_DIR_INITIALIZED) {
            return FALSE;
        }

        if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
        wcscpy_s(buffer, bufferSize, g_imageCacheDir);
        return TRUE;
    }

    /* Use system temp directory: %TEMP%\Catime\images */
    wchar_t tempDir[MAX_PATH];
    DWORD result = GetTempPathW(MAX_PATH, tempDir);
    if (result == 0 || result >= MAX_PATH) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    /* Build path: %TEMP%\Catime\images */
    wchar_t catimeDir[MAX_PATH];
    int catimeWritten = _snwprintf_s(catimeDir, MAX_PATH, _TRUNCATE, L"%sCatime", tempDir);
    if (catimeWritten < 0) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }
    if (!EnsureDirectoryExistsW(catimeDir)) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wchar_t imageDir[MAX_PATH];
    int imageDirWritten = _snwprintf_s(imageDir, MAX_PATH, _TRUNCATE, L"%sCatime\\images", tempDir);
    if (imageDirWritten < 0) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }
    if (!EnsureDirectoryExistsW(imageDir)) {
        InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_UNINITIALIZED);
        return FALSE;
    }

    wcscpy_s(g_imageCacheDir, MAX_PATH, imageDir);
    InterlockedExchange(&g_imageCacheDirInit, IMAGE_CACHE_DIR_INITIALIZED);

    if (wcslen(g_imageCacheDir) >= bufferSize) return FALSE;
    wcscpy_s(buffer, bufferSize, g_imageCacheDir);

    return TRUE;
}

/**
 * @brief Check if path is a network URL
 */
static BOOL IsNetworkUrl(const wchar_t* path) {
    if (!path) return FALSE;
    return (_wcsnicmp(path, L"http://", 7) == 0 ||
            _wcsnicmp(path, L"https://", 8) == 0);
}

/**
 * @brief Check if path is absolute
 */
static BOOL IsAbsolutePath(const wchar_t* path) {
    if (!path || path[0] == L'\0' || path[1] == L'\0') return FALSE;
    /* Check for drive letter (C:\...) or UNC path (\\...) */
    return ((path[1] == L':') || (path[0] == L'\\' && path[1] == L'\\'));
}

static BOOL GetPluginRelativeImageBaseDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return FALSE;
    buffer[0] = L'\0';

    wchar_t sourcePath[MAX_PATH];
    if (!PluginData_GetDisplaySourcePath(sourcePath, MAX_PATH)) {
        return FALSE;
    }

    wchar_t* lastBackslash = wcsrchr(sourcePath, L'\\');
    wchar_t* lastSlash = wcsrchr(sourcePath, L'/');
    wchar_t* lastSeparator = lastBackslash;
    if (lastSlash && (!lastSeparator || lastSlash > lastSeparator)) {
        lastSeparator = lastSlash;
    }

    if (lastSeparator && lastSeparator > sourcePath) {
        *lastSeparator = L'\0';
        if (wcslen(sourcePath) < bufferSize) {
            wcscpy_s(buffer, bufferSize, sourcePath);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Generate cache filename from URL
 */
static unsigned long long HashUrl64(const wchar_t* url) {
    /* 64-bit FNV-1a keeps cache names compact while avoiding easy 32-bit collisions. */
    unsigned long long hash = 1469598103934665603ULL;
    const wchar_t* p = url;
    while (*p) {
        hash ^= (unsigned int)(*p);
        hash *= 1099511628211ULL;
        p++;
    }
    if (hash == 0ULL) hash = 1ULL;
    return hash;
}

static void GenerateCacheFilename(const wchar_t* url, wchar_t* filename, size_t size) {
    unsigned long long hash = HashUrl64(url);

    /* Extract extension from URL */
    const wchar_t* ext = L".png";  /* Default */
    const wchar_t* lastDot = wcsrchr(url, L'.');
    if (lastDot) {
        if (_wcsnicmp(lastDot, L".jpg", 4) == 0 || _wcsnicmp(lastDot, L".jpeg", 5) == 0) {
            ext = L".jpg";
        } else if (_wcsnicmp(lastDot, L".gif", 4) == 0) {
            ext = L".gif";
        } else if (_wcsnicmp(lastDot, L".bmp", 4) == 0) {
            ext = L".bmp";
        } else if (_wcsnicmp(lastDot, L".webp", 5) == 0) {
            ext = L".webp";
        }
    }

    _snwprintf_s(filename, size, _TRUNCATE, L"%016llX%s", hash, ext);
}

typedef struct {
    wchar_t path[MAX_PATH];
    FILETIME lastWriteTime;
    ULONGLONG size;
} ImageCachePruneEntry;

static BOOL IsHexDigitW(wchar_t ch) {
    return (ch >= L'0' && ch <= L'9') ||
           (ch >= L'a' && ch <= L'f') ||
           (ch >= L'A' && ch <= L'F');
}

static BOOL IsGeneratedImageCacheFileName(const wchar_t* name) {
    if (!name) return FALSE;

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

static int CompareImageCachePruneEntryByAge(const void* lhs, const void* rhs) {
    const ImageCachePruneEntry* a = (const ImageCachePruneEntry*)lhs;
    const ImageCachePruneEntry* b = (const ImageCachePruneEntry*)rhs;
    return CompareFileTime(&a->lastWriteTime, &b->lastWriteTime);
}

static ULONGLONG AddImageCacheBytesSaturated(ULONGLONG total, ULONGLONG value) {
    const ULONGLONG maxValue = (ULONGLONG)~0ull;
    if (value > maxValue - total) {
        return maxValue;
    }
    return total + value;
}

static int FindNewestImageCachePruneEntry(const ImageCachePruneEntry* entries,
                                          int entryCount) {
    int newest = 0;
