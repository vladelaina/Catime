
/**
 * @brief Get fonts folder path
 */
static BOOL GetFontsFolderPath(wchar_t* outPath, size_t size) {
    return GetFontsFolderW(outPath, size, FALSE);
}

/**
 * @brief Get current font relative path for matching
 */
static void GetCurrentFontRelativePath(wchar_t* outPath, size_t size) {
    if (!outPath || size == 0 || size > INT_MAX) return;
    outPath[0] = L'\0';

    const char* prefix = FONTS_PATH_PREFIX;
    size_t prefixLen = strlen(prefix);
    const char* source = NULL;

    if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
        /* Custom font - extract relative path */
        source = FONT_FILE_NAME + prefixLen;
    } else if (strchr(FONT_FILE_NAME, ':') == NULL &&
               (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL)) {
        /* Relative path without prefix */
        source = FONT_FILE_NAME;
    }

    if (source && MultiByteToWideChar(CP_UTF8, 0, source, -1, outPath, (int)size) <= 0) {
        outPath[0] = L'\0';
    }
    /* System fonts or just filename - leave empty */
}

static const wchar_t* GetPathBaseNameW(const wchar_t* path) {
    if (!path) return L"";

    const wchar_t* slash = wcsrchr(path, L'\\');
    const wchar_t* forwardSlash = wcsrchr(path, L'/');
    if (!slash || (forwardSlash && forwardSlash > slash)) {
        slash = forwardSlash;
    }

    return slash ? slash + 1 : path;
}

/* ============================================================================
 * Font Entry Management
 * ============================================================================ */

/**
 * @brief Add font entry to context
 */
static BOOL AddFontEntry(FontScanContext* ctx, const wchar_t* fileName,
                         const wchar_t* relativePath) {
    if (ctx->count >= ctx->capacity) {
        if (!ctx->full) {
            WriteLog(LOG_LEVEL_WARNING, "Font list capacity reached (%d), stopping scan",
                     ctx->capacity);
        }
        ctx->full = TRUE;
        return FALSE;
    }

    FontEntry* entry = &ctx->entries[ctx->count];
    wcsncpy(entry->fileName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->fileName[MAX_FONT_NAME_LENGTH - 1] = L'\0';

    wcsncpy(entry->relativePath, relativePath, MAX_PATH - 1);
    entry->relativePath[MAX_PATH - 1] = L'\0';

    /* Display name = filename without extension */
    wcsncpy(entry->displayName, fileName, MAX_FONT_NAME_LENGTH - 1);
    entry->displayName[MAX_FONT_NAME_LENGTH - 1] = L'\0';
    wchar_t* dotPos = wcsrchr(entry->displayName, L'.');
    if (dotPos) *dotPos = L'\0';

    ctx->count++;
    return TRUE;
}

/* ============================================================================
 * Folder Scanning
 * ============================================================================ */

/**
 * @brief Recursively scan font folder
 */
static void ScanFontFolderRecursive(const wchar_t* folderPath,
                                    const wchar_t* relativePath,
                                    FontScanContext* ctx,
                                    int depth,
                                    LONG generation) {
    if (ctx->full || IsFontMenuScanCanceled(generation)) return;

    if (depth >= MAX_RECURSION_DEPTH) {
        WriteLog(LOG_LEVEL_WARNING, "Max recursion depth reached at: %ls", folderPath);
        return;
    }

    wchar_t searchPath[MAX_PATH];
    int written = _snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", folderPath);
    if (written < 0 || written >= MAX_PATH) {
        WriteLog(LOG_LEVEL_WARNING, "Font scan path is too long: %ls", folderPath);
        ctx->failed = TRUE;
        return;
    }

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND &&
            error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_NO_MORE_FILES) {
            WriteLog(LOG_LEVEL_WARNING, "Font folder scan failed: %ls (error=%lu)",
                     folderPath, error);
            ctx->failed = TRUE;
        }
        return;
    }

    BOOL stoppedEarly = FALSE;
    do {
        if (ctx->full || IsFontMenuScanCanceled(generation)) {
            stoppedEarly = TRUE;
            break;
        }

        if (wcscmp(findData.cFileName, L".") == 0 ||
            wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (ctx->scannedEntries >= MAX_FONT_SCAN_ENTRIES) {
            if (!ctx->truncated) {
                WriteLog(LOG_LEVEL_WARNING, "Font scan entry limit reached (%d)",
                         MAX_FONT_SCAN_ENTRIES);
            }
            ctx->truncated = TRUE;
            ctx->full = TRUE;
            stoppedEarly = TRUE;
            break;
        }
        ctx->scannedEntries++;

        wchar_t fullPath[MAX_PATH];
        int len1 = _snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPath, findData.cFileName);
        if (len1 < 0 || len1 >= MAX_PATH) continue;

        wchar_t newRelativePath[MAX_PATH];
        if (!relativePath || relativePath[0] == L'\0') {
            wcsncpy(newRelativePath, findData.cFileName, MAX_PATH - 1);
            newRelativePath[MAX_PATH - 1] = L'\0';
        } else {
            int len2 = _snwprintf_s(newRelativePath, MAX_PATH, _TRUNCATE, L"%s\\%s", relativePath, findData.cFileName);
            if (len2 < 0 || len2 >= MAX_PATH) continue;
        }

        BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory) {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                ScanFontFolderRecursive(fullPath, newRelativePath, ctx, depth + 1, generation);
                if (ctx->failed) {
                    stoppedEarly = TRUE;
                    break;
                }
            }
        } else {
            const wchar_t* ext = wcsrchr(findData.cFileName, L'.');
            if (ext && (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0)) {
                if (!AddFontEntry(ctx, findData.cFileName, newRelativePath)) {
                    stoppedEarly = TRUE;
                    break;
                }
            }
        }
    } while (!ctx->full && FindNextFileW(hFind, &findData));

    DWORD findError = stoppedEarly ? ERROR_SUCCESS : GetLastError();
    FindClose(hFind);
    if (!stoppedEarly && findError != ERROR_NO_MORE_FILES) {
        WriteLog(LOG_LEVEL_WARNING, "Font folder enumeration failed: %ls (error=%lu)",
                 folderPath, findError);
        ctx->failed = TRUE;
    }
}

/**
 * @brief Scan fonts folder and return entries
 */
static int ScanFontsFolder(FontEntry* entries, int capacity, LONG generation) {
    FontScanContext ctx = {0};
    ctx.entries = entries;
    ctx.count = 0;
    ctx.capacity = capacity;

    wchar_t fontsPath[MAX_PATH];
    if (!GetFontsFolderPath(fontsPath, MAX_PATH)) {
        WriteLog(LOG_LEVEL_WARNING, "Failed to get fonts folder path");
        return FONT_MENU_SCAN_FAILED;
    }

    DWORD attribs = GetFileAttributesW(fontsPath);
    if (attribs == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return 0;
        }
        WriteLog(LOG_LEVEL_WARNING, "Failed to stat fonts folder: %ls (error=%lu)",
                 fontsPath, error);
        return FONT_MENU_SCAN_FAILED;
    }
    if (!(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        WriteLog(LOG_LEVEL_WARNING, "Fonts path is not a folder: %ls", fontsPath);
        return FONT_MENU_SCAN_FAILED;
    }

    ScanFontFolderRecursive(fontsPath, L"", &ctx, 0, generation);
    if (ctx.failed || IsFontMenuScanCanceled(generation)) {
        return FONT_MENU_SCAN_FAILED;
    }

    WriteLog(LOG_LEVEL_INFO, "Font scan complete: %d fonts found%s",
             ctx.count, ctx.truncated ? " (truncated)" : "");
    return ctx.count;
}

static DWORD WINAPI FontScanThread(LPVOID lpParam) {
    LONG generation = (LONG)(INT_PTR)lpParam;

    FontEntry* entries = (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*entries));
    int count = FONT_MENU_SCAN_FAILED;
    if (entries) {
        ZeroMemory(entries, (size_t)MAX_FONT_ENTRIES * sizeof(*entries));
        count = ScanFontsFolder(entries, MAX_FONT_ENTRIES, generation);
    } else {
        LOG_WARNING("Failed to allocate font menu scan buffer");
    }
    BOOL scanFailed = (count < 0);

    if (!IsFontMenuScanCanceled(generation)) {
        AcquireSRWLockExclusive(&g_fontMenuCacheLock);
        if (!IsFontMenuScanCanceled(generation)) {
            if (!scanFailed && count > 0 && entries) {
                memcpy(g_fontMenuCache, entries, (size_t)count * sizeof(FontEntry));
            }
            if (scanFailed) {
                count = 0;
            }
            if (count < MAX_FONT_ENTRIES) {
                ZeroMemory(&g_fontMenuCache[count],
                           (size_t)(MAX_FONT_ENTRIES - count) * sizeof(FontEntry));
            }
            g_fontMenuCacheCount = count;
            g_fontMenuCacheReady = !scanFailed;
            g_fontMenuCacheFailed = scanFailed;
            InterlockedExchange(&g_fontMenuLastScanTick, (LONG)GetTickCount());
        }
