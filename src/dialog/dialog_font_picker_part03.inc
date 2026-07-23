        }

        /* Blacklist of known incompatible fonts */
        const wchar_t* blacklist[] = {
            L"Symbol",
            L"Webdings",
            L"Wingdings",
            L"Wingdings 2",
            L"Wingdings 3",
            L"Marlett",
            L"MT Extra",
            L"MS Outlook",
            L"MS Reference Specialty",
            L"Bookshelf Symbol 7",
            NULL
        };

        for (int i = 0; blacklist[i] != NULL; i++) {
            if (_wcsicmp(faceName, blacklist[i]) == 0) {
                return 1;  /* Skip blacklisted font */
            }
        }

        /* Try to resolve font path */
        char fontPath[MAX_PATH];
        if (GetSystemFontPath(faceName, fontPath, sizeof(fontPath), (HANDLE)lParam)) {
            if (!AddOrUpdateFontMap(faceName, fontPath)) {
                return 0;
            }
        }
    }
    return 1;
}

static int CalculateFontListStorageBytes(void) {
    size_t storageBytes = 0;

    for (int i = 0; i < g_fontMapCount; i++) {
        size_t nameBytes = (wcslen(g_fontMap[i].fontName) + 1) * sizeof(wchar_t);
        if (nameBytes > (size_t)INT_MAX ||
            storageBytes > (size_t)INT_MAX - nameBytes) {
            return INT_MAX;
        }
        storageBytes += nameBytes;
    }

    return (int)storageBytes;
}

static void SelectCurrentFontInList(HWND hdlg, HWND hwndList) {
    if (!hdlg || !hwndList) return;

    /* Check if current font is from Windows Fonts directory and select it */
    wchar_t fontsDir[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, 0, fontsDir) != S_OK) {
        return;
    }

    wchar_t currentFontW[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, FONT_FILE_NAME, -1, currentFontW, MAX_PATH) <= 0) {
        return;
    }

    if (wcsstr(currentFontW, fontsDir) == currentFontW) {
        BOOL found = FALSE;
        for (int i = 0; i < g_fontMapCount; i++) {
            if (_stricmp(g_fontMap[i].fontPath, FONT_FILE_NAME) == 0) {
                LRESULT idx = SendMessageW(hwndList, LB_FINDSTRINGEXACT,
                                           (WPARAM)-1, (LPARAM)g_fontMap[i].fontName);

                if (idx != LB_ERR) {
                    SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)idx, 0);
                    if (idx >= 0 && idx <= INT_MAX) {
                        g_currentFontIndex = (int)idx;
                        g_previewFontIndex = (int)idx;
                    } else {
                        g_currentFontIndex = -1;
                        g_previewFontIndex = -1;
                    }

                    SendMessageW(hwndList, LB_SETTOPINDEX, (WPARAM)idx, 0);
                    SetFocus(hwndList);
                    InvalidateRect(hwndList, NULL, FALSE);
                    UpdateWindow(hwndList);
                    found = TRUE;
                }
                break;
            }
        }

        if (!found) {
            LOG_WARNING("FontPicker: Current system font not in font map (may be filtered)");
            SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
            g_currentFontIndex = -1;
            g_previewFontIndex = -1;
        }
    } else {
        SendMessageW(hwndList, LB_SETCURSEL, (WPARAM)-1, 0);
        g_currentFontIndex = -1;
        g_previewFontIndex = -1;
    }

    InvalidateRect(hwndList, NULL, FALSE);
}

static void PopulateFontList(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (!hwndList) {
        return;
    }

    SendMessageW(hwndList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
    if (g_fontMapCount > 0) {
        LRESULT reserveResult = SendMessageW(hwndList, LB_INITSTORAGE,
                                             (WPARAM)g_fontMapCount,
                                             (LPARAM)CalculateFontListStorageBytes());
        if (reserveResult == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox storage reservation failed for %d fonts", g_fontMapCount);
        }
    }

    int addedCount = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        LRESULT index = SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)g_fontMap[i].fontName);
        if (index == LB_ERRSPACE) {
            LOG_WARNING("FontPicker: Listbox ran out of storage after %d/%d fonts", addedCount, g_fontMapCount);
            break;
        }
        if (index == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to add font '%S' to listbox", g_fontMap[i].fontName);
            continue;
        }
        if (SendMessageW(hwndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)i) == LB_ERR) {
            LOG_WARNING("FontPicker: Failed to bind font map index for '%S'", g_fontMap[i].fontName);
        }
        addedCount++;
    }
    SendMessageW(hwndList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndList, NULL, FALSE);

    EnableWindow(hwndList, TRUE);
    EnableWindow(GetDlgItem(hdlg, IDOK), TRUE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Font families (variants filtered):"));

    SelectCurrentFontInList(hdlg, hwndList);

}

static void BuildFontMap(HANDLE stopEvent) {
    HDC hdc = GetDC(NULL);
    if (!hdc) {
        return;
    }

    LOGFONTW lf = {0};
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)EnumFontFamiliesProc, (LPARAM)stopEvent, 0);

    if (ShouldStopFontEnumeration(stopEvent)) {
        ReleaseDC(NULL, hdc);
        return;
    }

    int writeIndex = 0;
    for (int i = 0; i < g_fontMapCount; i++) {
        if (ShouldStopFontEnumeration(stopEvent)) {
            ReleaseDC(NULL, hdc);
            return;
        }

        if (CheckFontHasRequiredGlyphs(hdc, g_fontMap[i].fontName, stopEvent)) {
            if (writeIndex != i) {
                g_fontMap[writeIndex] = g_fontMap[i];
            }
            writeIndex++;
        }
    }

    g_fontMapCount = writeIndex;
    ReleaseDC(NULL, hdc);
}

static DWORD WINAPI FontEnumerationThread(LPVOID param) {
    FontEnumerationThreadParams* params = (FontEnumerationThreadParams*)param;
    HWND hdlg = params ? params->hdlg : NULL;
    HANDLE stopEvent = params ? params->stopEvent : NULL;
    LONG generation = params ? params->generation : 0;
    free(params);

    BuildFontMap(stopEvent);

    if (!ShouldStopFontEnumeration(stopEvent) &&
        InterlockedCompareExchange(&g_fontEnumGeneration, 0, 0) == generation &&
        hdlg &&
        IsWindow(hdlg) &&
        Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER) == hdlg) {
        PostMessageW(hdlg, WM_APP_FONT_ENUM_COMPLETE, (WPARAM)generation, 0);
    }

    if (stopEvent) {
        CloseHandle(stopEvent);
    }

    return 0;
}

static HANDLE StartFontEnumerationThread(HWND hdlg) {
    FontEnumerationThreadParams* params =
        (FontEnumerationThreadParams*)malloc(sizeof(FontEnumerationThreadParams));
    if (!params) {
        return NULL;
    }

    params->hdlg = hdlg;
    params->stopEvent = NULL;
    params->generation = InterlockedIncrement(&g_fontEnumGeneration);
    if (g_fontEnumStopEvent &&
        !DuplicateHandle(GetCurrentProcess(), g_fontEnumStopEvent,
                         GetCurrentProcess(), &params->stopEvent,
                         0, FALSE, DUPLICATE_SAME_ACCESS)) {
        LOG_WARNING("FontPicker: Failed to duplicate enumeration stop event (error=%lu)",
                    GetLastError());
        free(params);
        return NULL;
    }

    HANDLE hThread = CreateThread(NULL, 0, FontEnumerationThread, params, 0, NULL);
    if (!hThread) {
        if (params->stopEvent) {
            CloseHandle(params->stopEvent);
        }
        free(params);
    }
    return hThread;
}

static void CloseFontEnumStopEventIfIdle(void) {
    if (!g_fontEnumThread && g_fontEnumStopEvent) {
        CloseHandle(g_fontEnumStopEvent);
        g_fontEnumStopEvent = NULL;
    }
}

static void ShowFontEnumerationUnavailable(HWND hdlg) {
    HWND hwndList = GetDlgItem(hdlg, IDC_FONT_LIST_SIMPLE);
    if (hwndList) {
        SendMessageW(hwndList, LB_RESETCONTENT, 0, 0);
        EnableWindow(hwndList, FALSE);
    }
    EnableWindow(GetDlgItem(hdlg, IDOK), FALSE);
    SetDlgItemTextW(hdlg, IDC_FONT_PICKER_LABEL,
                    GetLocalizedString(NULL, L"Loading..."));
    g_fontListReady = FALSE;
}

static BOOL StartFontEnumerationAsync(HWND hdlg) {
    CloseFontEnumStopEventIfIdle();

    g_fontEnumStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_fontEnumStopEvent) {
        LOG_WARNING("FontPicker: Failed to create enumeration stop event (error=%lu)",
                    GetLastError());
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    g_fontEnumThread = StartFontEnumerationThread(hdlg);
    if (!g_fontEnumThread) {
        LOG_WARNING("FontPicker: Background enumeration unavailable; retrying asynchronously");
        CloseFontEnumStopEventIfIdle();
        ShowFontEnumerationUnavailable(hdlg);
        StartFontEnumRetryTimer(hdlg);
        return FALSE;
    }

    KillTimer(hdlg, FONT_ENUM_START_RETRY_TIMER_ID);
    StartFontEnumPollTimer(hdlg);
