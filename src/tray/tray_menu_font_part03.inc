            while (token) {
                wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);
                if (nextToken) {
                    size_t currentLen = wcslen(currentPath);
                    size_t tokenLen = wcslen(token);

                    if (currentLen + tokenLen + 2 < MAX_PATH) {
                        if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                        wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                    }

                    BOOL found = FALSE;
                    for (int j = 0; j < parentDirCount; j++) {
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
                            found = TRUE;
                            break;
                        }
                    }
                    if (!found && parentDirCount < MAX_PARENT_DIRS) {
                        wcsncpy(parentDirs[parentDirCount], currentPath, MAX_PATH - 1);
                        parentDirs[parentDirCount][MAX_PATH - 1] = L'\0';
                        parentDirCount++;
                    }
                }
                token = nextToken;
            }
        }
    }

    /* Build menu tree */
    for (int i = 0; i < count; i++) {
        const FontEntry* entry = sortedEntries[i].entry;
        BOOL isCurrentFont = IsFontEntryCurrent(entry, currentFontRelPath);

        wchar_t pathCopy[MAX_PATH];
        wcsncpy(pathCopy, entry->relativePath, MAX_PATH - 1);
        pathCopy[MAX_PATH - 1] = L'\0';

        wchar_t* context = NULL;
        wchar_t* token = wcstok_s(pathCopy, L"\\/", &context);

        HMENU hCurrent = hRootMenu;
        wchar_t currentPath[MAX_PATH] = L"";

        while (token) {
            wchar_t* nextToken = wcstok_s(NULL, L"\\/", &context);

            if (nextToken) {
                size_t currentLen = wcslen(currentPath);
                size_t tokenLen = wcslen(token);

                if (currentLen + tokenLen + 2 < MAX_PATH) {
                    if (currentLen > 0) wcsncat_s(currentPath, MAX_PATH, L"\\", 1);
                    wcsncat_s(currentPath, MAX_PATH, token, tokenLen);
                }

                BOOL shouldCheck = FALSE;
                if (parentDirs) {
                    for (int j = 0; j < parentDirCount; j++) {
                        if (_wcsicmp(parentDirs[j], currentPath) == 0) {
                            shouldCheck = TRUE;
                            break;
                        }
                    }
                }

                hCurrent = EnsureSubMenu(hCurrent, token, shouldCheck);
                if (!hCurrent) break;
            } else {
                UINT flags = MF_STRING;
                if (isCurrentFont) flags |= MF_CHECKED;
                UINT id = (UINT)(*fontId);
                if (hCurrent && RememberFontMenuId(id, entry->relativePath)) {
                    if (AppendMenuW(hCurrent, flags, id, entry->displayName)) {
                        (*fontId)++;
                    } else {
                        ForgetLastFontMenuId(id);
                    }
                }
            }

            token = nextToken;
        }
    }

    free(parentDirs);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Build font submenu with cached folder scan
 */
void BuildFontSubmenu(HMENU hMenu) {
    HMENU hFontSubMenu = CreatePopupMenu();
    if (!hFontSubMenu) {
        WriteLog(LOG_LEVEL_ERROR, "Failed to create font submenu");
        return;
    }
    ResetFontMenuIdMap();

    int g_advancedFontId = CMD_FONT_SELECTION_BASE;

    if (NeedsFontLicenseVersionAcceptance()) {
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_LICENSE_AGREE,
                   GetLocalizedString(NULL, L"Click to agree to license agreement"));
    } else {
        FontMenu_RequestScanAsync();

        /* Get current font relative path */
        wchar_t currentFontRelPath[MAX_PATH] = L"";
        GetCurrentFontRelativePath(currentFontRelPath, MAX_PATH);

        BOOL cacheReady = FALSE;
        int fontCount = 0;
        BOOL isSystemFont = TRUE;
        FontEntry* fontSnapshot =
            (FontEntry*)malloc((size_t)MAX_FONT_ENTRIES * sizeof(*fontSnapshot));
        if (!fontSnapshot) {
            LOG_WARNING("Failed to allocate font menu cache snapshot");
        }

        AcquireSRWLockShared(&g_fontMenuCacheLock);
        cacheReady = g_fontMenuCacheReady || g_fontMenuCacheFailed;
        fontCount = g_fontMenuCacheCount;
        if (fontCount > MAX_FONT_ENTRIES) {
            fontCount = MAX_FONT_ENTRIES;
        }
        if (fontCount > 0 && fontSnapshot) {
            memcpy(fontSnapshot, g_fontMenuCache, (size_t)fontCount * sizeof(*fontSnapshot));
        } else if (fontCount > 0) {
            fontCount = 0;
            cacheReady = FALSE;
        }
        ReleaseSRWLockShared(&g_fontMenuCacheLock);

        if (fontCount == 0) {
            AppendMenuW(hFontSubMenu, MF_STRING | MF_GRAYED, 0,
                        cacheReady
                            ? GetLocalizedString(NULL, L"No font files found")
                            : GetLocalizedString(NULL, L"Loading..."));
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        } else {
            BuildFontMenuFromEntries(hFontSubMenu, fontSnapshot, fontCount,
                                     currentFontRelPath, &g_advancedFontId);
            AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        }

        /* Determine if current font is a system font */
        const char* prefix = FONTS_PATH_PREFIX;
        size_t prefixLen = strlen(prefix);

        if (_strnicmp(FONT_FILE_NAME, prefix, prefixLen) == 0) {
            isSystemFont = FALSE;
        } else if (strchr(FONT_FILE_NAME, ':') != NULL) {
            isSystemFont = (strstr(FONT_FILE_NAME, "Windows\\Fonts") != NULL ||
                           strstr(FONT_FILE_NAME, "WINDOWS\\Fonts") != NULL);
        } else if (strchr(FONT_FILE_NAME, '\\') != NULL || strchr(FONT_FILE_NAME, '/') != NULL) {
            isSystemFont = FALSE;
        } else {
            /* Just filename - check if in scanned fonts */
            wchar_t wFontName[MAX_PATH] = L"";
            if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, wFontName, MAX_PATH) > 0 &&
                wFontName[0] != L'\0') {
                for (int i = 0; i < fontCount; i++) {
                    if (_wcsicmp(GetPathBaseNameW(fontSnapshot[i].relativePath), wFontName) == 0) {
                        isSystemFont = FALSE;
                        break;
                    }
                }
            }
        }
        free(fontSnapshot);

        UINT systemFontFlags = MF_STRING;
        if (isSystemFont) systemFontFlags |= MF_CHECKED;

        AppendMenuW(hFontSubMenu, systemFontFlags, CLOCK_IDM_SYSTEM_FONT_PICKER,
                   GetLocalizedString(NULL, L"System Fonts..."));

        AppendMenuW(hFontSubMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hFontSubMenu, MF_STRING, CLOCK_IDC_FONT_ADVANCED,
                   GetLocalizedString(NULL, L"Open fonts folder"));
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFontSubMenu,
                     GetLocalizedString(NULL, L"Font"))) {
        DestroyMenu(hFontSubMenu);
        WriteLog(LOG_LEVEL_ERROR, "Failed to attach font submenu");
    }
}

/**
 * @brief Get font path from the current menu ID map
 */
BOOL GetFontPathFromMenuId(UINT id, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return FALSE;
    outPath[0] = '\0';
    if (outPathSize > INT_MAX) return FALSE;
    if (id < CMD_FONT_SELECTION_BASE) return FALSE;

    for (int i = 0; i < g_fontMenuIdMapCount; i++) {
        if (g_fontMenuIdMap[i].id == id) {
            if (WideCharToMultiByte(CP_UTF8, 0, g_fontMenuIdMap[i].relativePath, -1,
                                    outPath, (int)outPathSize, NULL, NULL) <= 0) {
                outPath[0] = '\0';
                return FALSE;
            }
            return TRUE;
        }
    }

    return FALSE;
}
