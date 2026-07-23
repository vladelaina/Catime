            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                GlyphMetrics gm;
                GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);

                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;

                    const stbtt_fontinfo* glyphFontInfo = gm.isFallback ? &g_fallbackFontInfo : &g_fontInfo;
                    float glyphScale = gm.isFallback ? fallbackScale : scale;
                    int glyphMargin = (effect != EFFECT_TYPE_NONE) ? 24 : 0;
                    bitmap = CreateVisibleGlyphBitmapSTB(glyphFontInfo, gm.index,
                                                         glyphScale, glyphScale,
                                                         currentX, lineY,
                                                         width, height,
                                                         glyphMargin,
                                                         &w, &h, &xoff, &yoff);

                    if (bitmap) {
                        int glyphX = AddTextIntClamped(currentX, xoff);
                        int glyphY = AddTextIntClamped(lineY, yoff);
                        BlendCharBitmapSTBWithEffect(bits, width, height,
                                                     glyphX, glyphY,
                                                     bitmap, w, h, r, g, b,
                                                     effect, timeOffset);
                        stbtt_FreeBitmap(bitmap, NULL);
                    }
                }
                currentX = AddTextIntClamped(currentX, gm.advance + gm.kern);
            }

            currentY++;
            currentLineStart = i + 1;
        }
    }

done:
    EndFontUseSTB();
}

/* ============================================================================
 * Font Cache Implementation for <font:> Tags
 * ============================================================================ */

/**
 * @brief Resolve font path from tag value
 *
 * Supports:
 * - Absolute paths: C:\Fonts\my.ttf, \\server\share\font.ttf
 * - Environment variables: %WINDIR%\Fonts\arial.ttf
 * - Relative paths: fonts/custom.ttf (resolved relative to plugins directory)
 *
 * @param fontPath Font path from <font:> tag (wide string)
 * @param outPath Output buffer for resolved path (wide string)
 * @param pathSize Buffer size
 * @return TRUE if path resolved and file exists
 */
static BOOL ResolveFontTagPath(const wchar_t* fontPath, wchar_t* outPath, size_t pathSize) {
    if (!fontPath || !outPath || pathSize == 0) return FALSE;
    outPath[0] = L'\0';

    wchar_t expandedPath[MAX_PATH];
    wchar_t resolvedPath[MAX_PATH];

    /* Step 1: Expand environment variables if present. Package-aware
     * LocalAppData expansion keeps Store font tags inside the MSIX data root. */
    static const wchar_t localAppDataToken[] = L"%LOCALAPPDATA%";
    size_t localAppDataTokenLen = _countof(localAppDataToken) - 1;
    if (_wcsnicmp(fontPath, localAppDataToken, localAppDataTokenLen) == 0) {
        char fontPathUtf8[MAX_PATH] = {0};
        char expandedPathUtf8[MAX_PATH] = {0};
        if (!WideToUtf8(fontPath, fontPathUtf8, MAX_PATH) ||
            !ExpandEffectiveLocalAppDataPath(fontPathUtf8,
                                             expandedPathUtf8,
                                             sizeof(expandedPathUtf8)) ||
            !Utf8ToWide(expandedPathUtf8, expandedPath, MAX_PATH)) {
            LOG_WARNING("Failed to expand effective LocalAppData font path: %ls", fontPath);
            return FALSE;
        }
    } else if (wcschr(fontPath, L'%') != NULL) {
        DWORD result = ExpandEnvironmentStringsW(fontPath, expandedPath, MAX_PATH);
        if (result == 0 || result > MAX_PATH) {
            LOG_WARNING("Failed to expand environment variables in font path: %ls", fontPath);
            return FALSE;
        }
    } else {
        if (wcslen(fontPath) >= MAX_PATH) {
            LOG_WARNING("Font path is too long: %ls", fontPath);
            return FALSE;
        }
        wcsncpy(expandedPath, fontPath, MAX_PATH - 1);
        expandedPath[MAX_PATH - 1] = L'\0';
    }

    /* Step 2: Check if absolute path (has drive letter or UNC path) */
    BOOL isAbsolute = FALSE;
    if (wcslen(expandedPath) >= 2) {
        /* Check for drive letter (e.g., C:\) */
        if (expandedPath[1] == L':') {
            isAbsolute = TRUE;
        }
        /* Check for UNC path (e.g., \\server\share) */
        else if (expandedPath[0] == L'\\' && expandedPath[1] == L'\\') {
            isAbsolute = TRUE;
        }
    }

    if (isAbsolute) {
        if (wcslen(expandedPath) >= MAX_PATH) {
            LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
            return FALSE;
        }
        wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
        resolvedPath[MAX_PATH - 1] = L'\0';
    } else {
        /* Step 3: Resolve relative path against plugins directory */
        wchar_t pluginsDir[MAX_PATH];
        char pluginsDirUtf8[MAX_PATH] = {0};
        GetPluginsFolderPath(pluginsDirUtf8, MAX_PATH);
        if (pluginsDirUtf8[0] == '\0' ||
            MultiByteToWideChar(CP_UTF8, 0, pluginsDirUtf8, -1,
                                pluginsDir, MAX_PATH) <= 0) {
            /* Fallback: try current directory */
            if (wcslen(expandedPath) >= MAX_PATH) {
                LOG_WARNING("Font path is too long: %ls", expandedPath);
                return FALSE;
            }
            wcsncpy(resolvedPath, expandedPath, MAX_PATH - 1);
            resolvedPath[MAX_PATH - 1] = L'\0';
        } else {
            int written = _snwprintf_s(resolvedPath, MAX_PATH, _TRUNCATE,
                                       L"%ls\\%ls",
                                       pluginsDir, expandedPath);
            if (written < 0) {
                LOG_WARNING("Resolved font path is too long: %ls", expandedPath);
                return FALSE;
            }
        }
    }

    /* Step 4: Check if file exists */
    DWORD attrs = GetFileAttributesW(resolvedPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        LOG_WARNING("Font file not found: %ls", resolvedPath);
        return FALSE;
    }

    /* Step 5: Return wide path directly to avoid UTF-8 round trip before CreateFileW. */
    if (wcslen(resolvedPath) >= pathSize) {
        LOG_WARNING("Resolved font path is too long: %ls", resolvedPath);
        return FALSE;
    }
    wcsncpy(outPath, resolvedPath, pathSize - 1);
    outPath[pathSize - 1] = L'\0';

    return TRUE;
}

void ClearFontCacheSTB(void) {
    if (!BeginFontUseSTB()) return;

    ClearFontCacheSTBLocked();

    EndFontUseSTB();
}

static void ReleaseCachedFontSlotLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return;

    if (g_fontCache[slot].isLoaded) {
        ReleaseMappedFont(g_fontCache[slot].fontBuffer,
                          g_fontCache[slot].hFile,
                          g_fontCache[slot].hMapping);
        AdvanceFontStateGeneration();
    }
    ZeroMemory(&g_fontCache[slot], sizeof(CachedFont));
    g_fontCacheLRU[slot] = 0;
    ClearFontTagGlyphMetricsCacheSlotLocked(slot);
}

static void ClearFontCacheSTBLocked(void) {
    for (int i = 0; i < MAX_CACHED_FONTS; i++) {
        ReleaseCachedFontSlotLocked(i);
    }
    ZeroMemory(g_failedFontCache, sizeof(g_failedFontCache));
    g_fontCacheAccessCounter = 0;
}

static BOOL IsCachedFontSlotCurrentLocked(int slot) {
    if (slot < 0 || slot >= MAX_CACHED_FONTS) return FALSE;

    CachedFont* entry = &g_fontCache[slot];
    if (!entry->isLoaded || !entry->fileInfoValid || entry->resolvedPath[0] == L'\0') {
        return FALSE;
    }

    DWORD now = GetTickCount();
    if (entry->lastValidateTick != 0 &&
        (DWORD)(now - entry->lastValidateTick) < MAIN_FONT_FILE_RECHECK_MS) {
        return TRUE;
    }
    entry->lastValidateTick = now;

    FILETIME lastWriteTime;
    ULONGLONG fileSize = 0;
    if (!GetFontFileInfoFromPathW(entry->resolvedPath, &lastWriteTime, &fileSize)) {
        return FALSE;
    }

    return entry->fileSize == fileSize &&
           CompareFileTime(&entry->lastWriteTime, &lastWriteTime) == 0;
}

static BOOL CopyFontCacheKeyW(const wchar_t* fontPath, wchar_t* outPath) {
    if (!outPath) return FALSE;
    outPath[0] = L'\0';
    if (!fontPath || fontPath[0] == L'\0') return FALSE;

    size_t len = 0;
    while (len < MAX_PATH && fontPath[len] != L'\0') {
        len++;
    }
    if (len >= MAX_PATH) {
        return FALSE;
    }

    memcpy(outPath, fontPath, (len + 1) * sizeof(wchar_t));
    return TRUE;
}

static BOOL IsRecentFontFailureCached(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return FALSE;

    DWORD now = GetTickCount();
    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] == L'\0') continue;
        if (wcscmp(g_failedFontCache[i].fontName, cacheKey) != 0) continue;

        if ((LONG)(g_failedFontCache[i].retryAfterFailureTick - now) > 0) {
            return TRUE;
        }

        ZeroMemory(&g_failedFontCache[i], sizeof(g_failedFontCache[i]));
        return FALSE;
    }

    return FALSE;
}

static void RemoveFailedFontCacheEntry(const wchar_t* fontPath) {
    wchar_t cacheKey[MAX_PATH] = {0};
    if (!CopyFontCacheKeyW(fontPath, cacheKey)) return;

    for (int i = 0; i < MAX_FAILED_FONT_CACHE; i++) {
        if (g_failedFontCache[i].fontName[0] != L'\0' &&
            wcscmp(g_failedFontCache[i].fontName, cacheKey) == 0) {
            ZeroMemory(&g_failedFontCache[i], sizeof(g_failedFontCache[i]));
