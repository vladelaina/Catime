    if (!pcds || !pcds->lpData || pcds->cbData == 0) {
        return DefWindowProc(hwnd, WM_COPYDATA, wp, lp);
    }

    // Handle CLI text
    if (pcds->dwData == COPYDATA_ID_CLI_TEXT) {
        const size_t maxLen = BUFFER_SIZE_CLI_INPUT - 1;
        char buf[BUFFER_SIZE_CLI_INPUT];
        size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
        memcpy(buf, pcds->lpData, n);
        buf[maxLen] = '\0';
        buf[n] = '\0';
        HandleCliArguments(hwnd, buf);
        return TRUE;
    }

    return DefWindowProc(hwnd, WM_COPYDATA, wp, lp);
}

LRESULT HandleQuickCountdownIndex(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    int idx = (int)lp;
    if (idx >= 1) {
        StartQuickCountdownByIndex(hwnd, idx);
    } else {
        StartDefaultCountDown(hwnd);
    }
    return 0;
}

LRESULT HandleShowCliHelp(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowCliHelpDialog(hwnd);
    return 0;
}

LRESULT HandleTrayUpdateIcon(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    if (TrayAnimation_HandleUpdateMessage(hwnd)) return 0;
    return DefWindowProc(hwnd, WM_USER + 100, wp, lp);
}

LRESULT HandleAppReregisterHotkeys(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    RegisterGlobalHotkeys(hwnd);
    return 0;
}

LRESULT HandleAnimationPreviewLoaded(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp; (void)lp;
    /* Update tray icon after preview animation is loaded */
    TrayAnimation_HandleUpdateMessage(hwnd);
    return 0;
}

LRESULT HandleMeasureItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp;
    LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
    if (lpmis->CtlType == ODT_MENU) {
        lpmis->itemHeight = 25;
        lpmis->itemWidth = BUFFER_SIZE_MENU_ITEM;
        return TRUE;
    }
    return FALSE;
}

LRESULT HandleDrawItem(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)hwnd; (void)wp;
    LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
    if (lpdis->CtlType != ODT_MENU) return FALSE;

    char hexColor[COLOR_HEX_BUFFER];
    if (!GetColorMenuColorFromId(lpdis->itemID, hexColor, sizeof(hexColor))) {
        return FALSE;
    }

    BOOL darkMenu = IsNativeMenuDarkModeActive();
    COLORREF itemBackground = darkMenu
        ? ((lpdis->itemState & ODS_SELECTED)
               ? RGB(62, 62, 62)
               : RGB(32, 32, 32))
        : GetSysColor((lpdis->itemState & ODS_SELECTED)
                          ? COLOR_HIGHLIGHT
                          : COLOR_MENU);
    HBRUSH backgroundBrush = CreateSolidBrush(itemBackground);
    if (backgroundBrush) {
        FillRect(lpdis->hDC, &lpdis->rcItem, backgroundBrush);
        DeleteObject(backgroundBrush);
    }

    GradientInfoSnapshot gradientSnapshot;
    GradientType gradientType = GetGradientInfoSnapshotByName(hexColor, &gradientSnapshot);

    /* Draw color/gradient with space for sequence number */
    RECT colorRect = lpdis->rcItem;
    colorRect.left += 28;  /* Leave space for number */

    if (gradientType != GRADIENT_NONE) {
        DrawGradientRect(lpdis->hDC, &colorRect, &gradientSnapshot.info);
    } else {
        COLORREF color = RGB(255, 255, 255);
        ColorStringToColorRef(hexColor, &color);

        HGDIOBJ hBrush = GetStockObject(DC_BRUSH);
        HGDIOBJ hPen = GetStockObject(DC_PEN);
        if (!hBrush || !hPen) return FALSE;

        COLORREF oldBrushColor = SetDCBrushColor(lpdis->hDC, color);
        COLORREF oldPenColor = SetDCPenColor(lpdis->hDC, RGB(200, 200, 200));
        HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
        HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);

        Rectangle(lpdis->hDC, colorRect.left, colorRect.top,
                 colorRect.right, colorRect.bottom);

        if (oldPen) SelectObject(lpdis->hDC, oldPen);
        if (oldBrush) SelectObject(lpdis->hDC, oldBrush);
        if (oldPenColor != CLR_INVALID) SetDCPenColor(lpdis->hDC, oldPenColor);
        if (oldBrushColor != CLR_INVALID) SetDCBrushColor(lpdis->hDC, oldBrushColor);
    }

    /* Draw sequence number on left side */
    wchar_t numStr[8];
    _snwprintf_s(numStr, 8, _TRUNCATE, L"%u",
                 (unsigned int)(lpdis->itemID - CMD_COLOR_OPTIONS_BASE + 1));
    RECT numRect = lpdis->rcItem;
    numRect.right = numRect.left + 26;
    int oldBkMode = SetBkMode(lpdis->hDC, TRANSPARENT);
    COLORREF numberColor = darkMenu
        ? RGB(255, 255, 255)
        : GetSysColor((lpdis->itemState & ODS_SELECTED)
                          ? COLOR_HIGHLIGHTTEXT
                          : COLOR_MENUTEXT);
    COLORREF oldTextColor = SetTextColor(lpdis->hDC, numberColor);
    DrawTextW(lpdis->hDC, numStr, -1, &numRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (lpdis->itemState & ODS_SELECTED) {
        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
    }

    if (oldTextColor != CLR_INVALID) {
        SetTextColor(lpdis->hDC, oldTextColor);
    }
    if (oldBkMode != 0) {
        SetBkMode(lpdis->hDC, oldBkMode);
    }

    return TRUE;
}

static UINT g_pendingAnimationPreviewItem = 0;
static UINT g_lastPreviewMenuItem = 0;
static BOOL g_lastWasColorOrFontPreview = FALSE;
static BOOL g_lastWasAnimationPreview = FALSE;

UINT GetPendingAnimationPreviewItem(void) {
    return g_pendingAnimationPreviewItem;
}

static void ClearPendingMenuPreview(HWND hwnd) {
    KillTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY);
    g_pendingAnimationPreviewItem = 0;
}

static void DispatchPendingMenuPreview(HWND hwnd) {
    UINT menuItem = GetPendingAnimationPreviewItem();
    ClearPendingMenuPreview(hwnd);
    if (menuItem != 0) {
        DispatchMenuPreview(hwnd, menuItem);
    }
}

static void ResetMenuPreviewTracking(HWND hwnd) {
    ClearPendingMenuPreview(hwnd);
    g_lastPreviewMenuItem = 0;
    g_lastWasColorOrFontPreview = FALSE;
    g_lastWasAnimationPreview = FALSE;
}

void StopMenuPreviewTrackingForCommand(HWND hwnd) {
    ResetMenuPreviewTracking(hwnd);
    KillTimer(hwnd, IDT_MENU_DEBOUNCE);
}

static void StartMenuDebounceTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_MENU_DEBOUNCE, MENU_DEBOUNCE_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start debounce timer (error=%lu)",
                    GetLastError());
        CancelPreview(hwnd);
        RestoreWindowVisibility(hwnd);
    }
}

static void StartAnimationPreviewDelayTimer(HWND hwnd) {
    if (!SetTimer(hwnd, IDT_ANIMATION_PREVIEW_DELAY,
                  ANIMATION_PREVIEW_DELAY_MS, NULL)) {
        LOG_WARNING("MenuPreview: Failed to start preview delay timer (error=%lu)",
                    GetLastError());
        DispatchPendingMenuPreview(hwnd);
    }
}

static BOOL IsThemeSettingChange(WPARAM wp, LPARAM lp) {
    if (wp == SPI_SETHIGHCONTRAST) return TRUE;
    if (!lp) return FALSE;

    static const wchar_t* settingNames[] = {
        L"ImmersiveColorSet",
        L"WindowsThemeElement",
        L"AppsUseLightTheme",
        L"SystemUsesLightTheme",
        L"HighContrast"
    };
    const wchar_t* settingName = (const wchar_t*)lp;
    for (size_t i = 0; i < _countof(settingNames); i++) {
        if (_wcsicmp(settingName, settingNames[i]) == 0) return TRUE;
    }
    return FALSE;
}

LRESULT HandleSettingChange(HWND hwnd, WPARAM wp, LPARAM lp) {
    /* Theme-related setting names vary across Windows releases and Insider
     * builds. Refreshing for every settings notification is inexpensive, and
     * the popup path still performs a final refresh before showing a menu. */
    RefreshNativeMenuTheme();
    if (IsThemeSettingChange(wp, lp)) {
        Dialog_RefreshOpenThemes();
    }

    if (wp != SPI_SETWORKAREA || CLOCK_IS_DRAGGING) {
        return DefWindowProc(hwnd, WM_SETTINGCHANGE, wp, lp);
    }

    if (!BeginSystemPositionChangeGuard(hwnd)) {
        RestoreWindowPositionAfterSystemChange(hwnd);
    }
    return DefWindowProc(hwnd, WM_SETTINGCHANGE, wp, lp);
}

LRESULT HandleThemeChanged(HWND hwnd, WPARAM wp, LPARAM lp) {
    RefreshNativeMenuTheme();
    Dialog_RefreshOpenThemes();
    return DefWindowProc(hwnd, WM_THEMECHANGED, wp, lp);
}

static BOOL g_sessionSettingsPrepared = FALSE;

LRESULT HandleQueryEndSession(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    CancelScheduledConfigSave(hwnd);
    if (CLOCK_EDIT_MODE) {
        EndEditMode(hwnd);
    }
    g_sessionSettingsPrepared = SaveWindowSettings(hwnd);
    return TRUE;
}

LRESULT HandleEndSession(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)lp;
    if (wp && !g_sessionSettingsPrepared) {
        CancelScheduledConfigSave(hwnd);
        if (CLOCK_EDIT_MODE) {
            EndEditMode(hwnd);
        }
        SaveWindowSettings(hwnd);
    }
    g_sessionSettingsPrepared = FALSE;
    return 0;
}

static BOOL IsSelectableCommandMenuItem(HMENU hMenu, UINT menuItem) {
    if (!hMenu) {
        return FALSE;
    }

    MENUITEMINFOW itemInfo = {0};
    itemInfo.cbSize = sizeof(itemInfo);
    itemInfo.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
