            return;
        }
    }
}

static void RecordFailedFontCacheEntry(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return;

    DWORD now = GetTickCount();
    int target = -1;

    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] == L'\0') {
            target = i;
            break;
        }

        if (wcscmp(g_failedFontCache[i].fontName, cacheKey) == 0) {
            target = i;
            break;
        }

        if ((LONG)(g_failedFontCache[i].retryAfterFailureTick - now) <= 0) {
            target = i;
            break;
        }

        if (target < 0 ||
            (LONG)(g_failedFontCache[target].retryAfterFailureTick -
                   g_failedFontCache[i].retryAfterFailureTick) > 0) {
            target = i;
        }
    }

    if (target < 0) return;

    memcpy(g_failedFontCache[target].fontName, cacheKey, sizeof(cacheKey));
    DWORD retryAfter = now + FONT_FAILURE_RETRY_MS;
    g_failedFontCache[target].retryAfterFailureTick = retryAfter ? retryAfter : 1;
}

/**
 * @brief Get cached font by file path
 *
 * The fontPath parameter should be a file path, not a font name.
 * Supports:
 * - Absolute paths: C:\Fonts\my.ttf
 * - Environment variables: %WINDIR%\Fonts\arial.ttf
 * - Relative paths: fonts/custom.ttf (resolved relative to plugins directory)
 *
 * @param fontPath Font file path from <font:> tag
 * @return Font info pointer, or NULL if font cannot be loaded
 */
stbtt_fontinfo* GetCachedFontSTB(const wchar_t* fontPath) {
    if (!fontPath || fontPath[0] == L'\0') return NULL;

    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) {
        LOG_WARNING("Font cache key is too long: %ls", fontPath);
        return NULL;
    }

    int targetSlot = -1;

    /* Search cache for existing font (by original path for cache key) */
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        if (g_fontCache[i].isLoaded && wcscmp(g_fontCache[i].fontName, cacheKey) == 0) {
            if (IsCachedFontSlotCurrentLocked(i)) {
                TouchFontCacheSlotLocked(i);
                return &g_fontCache[i].fontInfo;
            }
            ReleaseCachedFontSlotLocked(i);
            targetSlot = i;
            break;
        }
    }

    if (IsRecentFontFailureCached(cacheKey)) {
        return NULL;
    }

    /* Font not in cache, resolve path and load it */
    wchar_t resolvedPath[MAX_PATH];
    if (!ResolveFontTagPath(fontPath, resolvedPath, _countof(resolvedPath))) {
        RecordFailedFontCacheEntry(cacheKey);
        LOG_WARNING("Font path resolution failed: %ls", fontPath);
        return NULL;
    }

    /* Find slot: empty or LRU */
    if (targetSlot < 0) {
        int minLRU = INT_MAX;
        for (int i = 0; i < MAX_CACHED_FONTS; i++) {
            if (!g_fontCache[i].isLoaded) {
                targetSlot = i;
                break;
            }
            if (g_fontCacheLRU[i] < minLRU) {
                minLRU = g_fontCacheLRU[i];
                targetSlot = i;
            }
        }
    }

    if (targetSlot < 0) targetSlot = 0;

    /* Load new font */
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
    unsigned char* buffer = LoadFontMappingW(resolvedPath, &hFile, &hMapping);

    if (!buffer) {
        RecordFailedFontCacheEntry(cacheKey);
        LOG_WARNING("Failed to load font file: %ls", resolvedPath);
        return NULL;
    }

    FILETIME lastWriteTime;
    ULONGLONG fileSize = 0;
    if (!GetFontFileInfoFromHandle(hFile, &lastWriteTime, &fileSize)) {
        ReleaseMappedFont(buffer, hFile, hMapping);
        RecordFailedFontCacheEntry(cacheKey);
        return NULL;
    }

    stbtt_fontinfo newFontInfo;
    if (!InitFontInfoFromBufferW(&newFontInfo, buffer, resolvedPath)) {
        ReleaseMappedFont(buffer, hFile, hMapping);
        RecordFailedFontCacheEntry(cacheKey);
        return NULL;
    }

    /* Evict only after the replacement font is known to be usable. */
    if (g_fontCache[targetSlot].isLoaded) {
        ReleaseCachedFontSlotLocked(targetSlot);
    }

    /* Store in cache (use original path as cache key) */
    RemoveFailedFontCacheEntry(cacheKey);
    memcpy(g_fontCache[targetSlot].fontName, cacheKey, sizeof(cacheKey));
    wcscpy_s(g_fontCache[targetSlot].resolvedPath,
             _countof(g_fontCache[targetSlot].resolvedPath),
             resolvedPath);
    g_fontCache[targetSlot].fontInfo = newFontInfo;
    g_fontCache[targetSlot].fontBuffer = buffer;
    g_fontCache[targetSlot].hFile = hFile;
    g_fontCache[targetSlot].hMapping = hMapping;
    g_fontCache[targetSlot].lastWriteTime = lastWriteTime;
    g_fontCache[targetSlot].fileSize = fileSize;
    g_fontCache[targetSlot].lastValidateTick = GetTickCount();
    g_fontCache[targetSlot].fileInfoValid = TRUE;
    g_fontCache[targetSlot].isLoaded = TRUE;
    TouchFontCacheSlotLocked(targetSlot);
    AdvanceFontStateGeneration();

    LOG_INFO("Cached font loaded: %ls -> %ls", fontPath, resolvedPath);

    return &g_fontCache[targetSlot].fontInfo;
}
