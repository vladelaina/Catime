    }
    g_fontMapCount = 0;
    g_fontMapCapacity = 0;
}

static BOOL CleanupCompletedFontEnumeration(void) {
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            LOG_WARNING("FontPicker: Failed to query font enumeration thread (error=%lu)",
                        GetLastError());
        }
        return FALSE;
    }

    CloseHandle(g_fontEnumThread);
    g_fontEnumThread = NULL;
    if (g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
    return TRUE;
}

static BOOL StopFontEnumerationWithTimeout(DWORD timeoutMs) {
    if (g_fontEnumThread || g_fontEnumStopEvent) {
        InterlockedIncrement(&g_fontEnumGeneration);
    }
    if (g_fontEnumStopEvent) {
        SetEvent(g_fontEnumStopEvent);
    }
    if (!g_fontEnumThread) {
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }

    DWORD waitResult = WaitForSingleObject(g_fontEnumThread, timeoutMs);
    if (waitResult == WAIT_OBJECT_0) {
        CloseHandle(g_fontEnumThread);
        g_fontEnumThread = NULL;
        if (g_fontEnumStopEvent) {
            CloseHandle(g_fontEnumStopEvent);
            g_fontEnumStopEvent = NULL;
        }
        return TRUE;
    }
    if (waitResult == WAIT_TIMEOUT) {
        LOG_WARNING("FontPicker: Font enumeration stop timed out after %lu ms",
                    timeoutMs);
    } else {
        LOG_WARNING("FontPicker: Font enumeration stop wait failed (wait=%lu, error=%lu)",
                    waitResult, GetLastError());
    }
    if (g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
    return FALSE;
}

static BOOL StartFontEnumPollTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_POLL_TIMER_ID, FONT_ENUM_POLL_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration poll timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static BOOL StartFontEnumRetryTimer(HWND hdlg) {
    if (!SetTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID,
                  FONT_ENUM_START_RETRY_INTERVAL_MS, NULL)) {
        LOG_WARNING("FontPicker: Failed to start enumeration retry timer (error=%lu)",
                    GetLastError());
        return FALSE;
    }
    return TRUE;
}

static VOID CALLBACK FontEnumDeferredCleanupTimerProc(HWND hwnd, UINT msg,
                                                      UINT_PTR idEvent,
                                                      DWORD time) {
    (void)msg;
    (void)time;

    if (idEvent != FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID) {
        return;
    }

    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER)) {
        KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
        return;
    }

    if (!CleanupCompletedFontEnumeration()) {
        return;
    }

    KillTimer(hwnd, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    ResetFontMap();
    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

static void ScheduleDeferredFontEnumerationCleanup(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (!hwndMain) {
        return;
    }

    if (!SetTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID,
                  FONT_ENUM_DEFERRED_CLEANUP_INTERVAL_MS,
                  FontEnumDeferredCleanupTimerProc)) {
        LOG_WARNING("FontPicker: Failed to schedule deferred enumeration cleanup (error=%lu)",
                    GetLastError());
    }
}

void CleanupSystemFontDialogResources(void) {
    HWND hwndMain = FindCurrentProcessMainWindow();
    if (hwndMain) {
        KillTimer(hwndMain, FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID);
    }

    BOOL enumStopped = StopFontEnumerationWithTimeout(FONT_ENUM_SHUTDOWN_WAIT_MS);
    if (!enumStopped) {
        LOG_WARNING("FontPicker: Font enumeration still running during shutdown; leaving resources to process teardown");
        return;
    }

    ResetFontMap();
    g_currentFontIndex = -1;
    g_previewFontIndex = -1;
    g_fontListReady = FALSE;
    g_fontEnumRestartAfterCleanup = FALSE;
}

static BOOL CheckFontHasRequiredGlyphs(HDC hdc, const wchar_t* fontName, HANDLE stopEvent) {
    if (ShouldStopFontEnumeration(stopEvent)) {
        return FALSE;
    }

    /* Required characters for countdown timer: 0-9 and colon */
    const wchar_t requiredChars[] = L"0123456789:";
    const int requiredCount = 11;

    if (!hdc) {
        LOG_ERROR("GlyphCheck: ✗ Invalid DC for font '%S'", fontName);
        return FALSE;
    }

    /* Create font for testing */
    HFONT hFont = CreateFontW(
        -24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        VARIABLE_PITCH | FF_SWISS,
        fontName
    );

    if (!hFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to create test font for '%S'", fontName);
        return FALSE;
    }

    if (ShouldStopFontEnumeration(stopEvent)) {
        DeleteObject(hFont);
        return FALSE;
    }

    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    if (!oldFont) {
        LOG_ERROR("GlyphCheck: ✗ Failed to select test font for '%S'", fontName);
        DeleteObject(hFont);
        return FALSE;
    }

    /* Check if all required glyphs exist */
    WORD glyphIndices[11];
    DWORD result = GetGlyphIndicesW(hdc, requiredChars, requiredCount, glyphIndices, GGI_MARK_NONEXISTING_GLYPHS);

    SelectObject(hdc, oldFont);
    DeleteObject(hFont);

    if (result == GDI_ERROR) {
        LOG_ERROR("GlyphCheck: ✗ GetGlyphIndicesW failed for '%S'", fontName);
        return FALSE;
    }

    /* Check if any glyph is missing (marked as 0xFFFF) */
    int missingCount = 0;
    for (int i = 0; i < requiredCount; i++) {
        if (glyphIndices[i] == 0xFFFF) {
            missingCount++;
        }
    }

    if (missingCount > 0) {
        return FALSE;
    }
    return TRUE;
}

static BOOL AddOrUpdateFontMap(const wchar_t* fontName, const char* fontPath) {
    /* Check if this path already exists */
    for (int i = 0; i < g_fontMapCount; i++) {
        if (strcmp(g_fontMap[i].fontPath, fontPath) == 0) {
            /* Same file - keep the shorter name, or alphabetically first if same length */
            size_t newLen = wcslen(fontName);
            size_t existingLen = wcslen(g_fontMap[i].fontName);

            BOOL shouldReplace = newLen < existingLen ||
                                 (newLen == existingLen && _wcsicmp(fontName, g_fontMap[i].fontName) < 0);

            if (shouldReplace) {
                wcscpy_s(g_fontMap[i].fontName, LF_FACESIZE, fontName);
            }
            return TRUE;
        }
    }

    /* New file - add to map */
    if (g_fontMapCount >= MAX_FONT_PICKER_ENTRIES) {
        LOG_WARNING("FontPicker: font entry limit reached (%d), stopping enumeration",
                    MAX_FONT_PICKER_ENTRIES);
        return FALSE;
    }

    if (g_fontMapCount >= g_fontMapCapacity) {
        if (g_fontMapCapacity > INT_MAX / 2) return FALSE;
        int newCapacity = g_fontMapCapacity <= 0 ? 256 : g_fontMapCapacity * 2;
        if (newCapacity > MAX_FONT_PICKER_ENTRIES) {
            newCapacity = MAX_FONT_PICKER_ENTRIES;
        }
        if ((size_t)newCapacity > ((size_t)-1) / sizeof(FontMapEntry)) return FALSE;
        FontMapEntry* newMap = (FontMapEntry*)realloc(g_fontMap, (size_t)newCapacity * sizeof(FontMapEntry));
        if (!newMap) return FALSE;
        g_fontMap = newMap;
        g_fontMapCapacity = newCapacity;
    }

    wcscpy_s(g_fontMap[g_fontMapCount].fontName, LF_FACESIZE, fontName);
    strncpy(g_fontMap[g_fontMapCount].fontPath, fontPath, MAX_PATH - 1);
    g_fontMap[g_fontMapCount].fontPath[MAX_PATH - 1] = '\0';
    g_fontMapCount++;
    return TRUE;
}

static int CALLBACK EnumFontFamiliesProc(const LOGFONTW* lpelf, const TEXTMETRICW* lpntm,
                                        DWORD fontType, LPARAM lParam) {
    (void)lpntm;

    if (lParam && WaitForSingleObject((HANDLE)lParam, 0) == WAIT_OBJECT_0) {
        return 0;
    }

    if (fontType & TRUETYPE_FONTTYPE) {
        const wchar_t* faceName = lpelf->lfFaceName;

        /* Skip vertical fonts */
        if (faceName[0] == L'@') {
            return 1;
        }

        /* Skip symbol fonts by charset */
        if (lpelf->lfCharSet == SYMBOL_CHARSET) {
            return 1;
