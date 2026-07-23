/**
 * @brief Register for taskbar recreation events
 * @note Enables auto-restore when Explorer restarts
 */
void RegisterTaskbarCreatedMessage() {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

/**
 * @brief Get initial icon for percent animations
 * @return HICON with warm-up sampling, or NULL for non-percent types
 * @note Takes two samples to avoid initial 0% reading
 */
static HICON GetInitialPercentIcon(AnimationType type) {
    if (!IsPercentIcon(type)) return NULL;

    int percent = 0;

    if (type == ANIM_TYPE_BATTERY) {
        int batteryPercent = 0;
        if (SystemMonitor_GetBatteryPercent(&batteryPercent)) {
            percent = batteryPercent;
        }
    } else {
        float cpu = 0.0f, mem = 0.0f;
        EnsureTraySystemMonitorActive();
        SystemMonitor_ForceRefresh();
        SystemMonitor_GetUsage(&cpu, &mem);
        percent = (type == ANIM_TYPE_CPU) ? (int)(cpu + 0.5f) : (int)(mem + 0.5f);
        if (type == ANIM_TYPE_CPU && percent == 0) {
            return NULL;
        }
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    return CreatePercentIcon16(percent);
}

static HICON GetInitialDynamicBuiltinIcon(AnimationType type) {
    if (IsPercentIcon(type)) {
        return GetInitialPercentIcon(type);
    }

    if (type == ANIM_TYPE_CAPSLOCK) {
        return CreateCapsLockIcon(IsCapsLockOn());
    }

    return NULL;
}

/**
 * @brief Initialize and add tray icon
 * @param hwnd Window handle for callbacks
 * @param hInstance App instance for icon resources
 */
static void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
                                 BOOL preloadAnimation,
                                 BOOL startBackgroundWork,
                                 BOOL useAnimationInitialIcon) {
    if (!IsValidTrayMainWindow(hwnd)) {
        LOG_WARNING("InitTrayIconInternal called with invalid main window");
        return;
    }

    g_mainHwnd = hwnd;
    g_hInstance = hInstance;
    g_lastTrayTooltip[0] = L'\0';
    TrayHoverRectCache_Reset(&g_trayIconRectCache);

    if (startBackgroundWork) {
        g_trayBackgroundWorkEnabled = TRUE;
        ReadPercentIconColorsConfig();
    }
    if (preloadAnimation) {
        PreloadAnimationFromConfig();
    }

    BOOL destroyInitialIcon = FALSE;
    HICON hInitial = NULL;
    if (useAnimationInitialIcon) {
        const char* animName = GetCurrentAnimationName();
        AnimationType type = GetAnimationType(animName);
        hInitial = GetInitialDynamicBuiltinIcon(type);
        if (hInitial) {
            destroyInitialIcon = TRUE;
        } else {
            HICON animationIcon = GetInitialAnimationHicon();
            if (animationIcon) {
                HICON ownedCopy = CopyIcon(animationIcon);
                hInitial = ownedCopy ? ownedCopy : animationIcon;
                destroyInitialIcon = ownedCopy != NULL;
            }
        }
    }

    memset(&nid, 0, sizeof(nid));
    g_trayIconActive = FALSE;
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = hInitial ? hInitial : LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CATIME));
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
    _snwprintf_s(nid.szTip, _countof(nid.szTip), _TRUNCATE,
                 L"%s\n%s\n%s\n%s",
                 GetLocalizedString(NULL, L"Tray Tooltip CPU"),
                 GetLocalizedString(NULL, L"Tray Tooltip Memory"),
                 GetLocalizedString(NULL, L"Tray Tooltip Upload"),
                 GetLocalizedString(NULL, L"Tray Tooltip Download"));

    BOOL iconAdded = Shell_NotifyIconW(NIM_ADD, &nid);
    if (iconAdded) {
        g_trayIconActive = TRUE;
        wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip), nid.szTip);
        CancelTrayRecreateRetry(hwnd);
    } else {
        LOG_WARNING("Failed to add tray icon (error=%lu)", GetLastError());
        ZeroMemory(&nid, sizeof(nid));
        ScheduleTrayRecreateRetry(hwnd);
    }
    if (destroyInitialIcon && iconAdded) {
        TrayIconLifetime_Retain(hInitial);
    } else if (destroyInitialIcon) {
        DestroyIcon(hInitial);
    }
    if (iconAdded) {
        nid.hIcon = NULL;
        nid.uFlags = NIF_MESSAGE | NIF_TIP;
    }

    /* Note: We don't use NOTIFYICON_VERSION_4 because it changes message format
     * and doesn't reliably send WM_MOUSEMOVE. Instead, we use a timer-based
     * approach to detect mouse hover over tray icon. */

    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }

    RefreshTrayBackgroundWorkState();

    /* Mouse hook is now installed on-demand when mouse hovers over tray icon */
}

void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    g_trayShuttingDown = FALSE;
    InitTrayIconInternal(hwnd, hInstance, TRUE, TRUE, TRUE);
}

static void RemoveTrayIconInternal(BOOL finalCleanup) {
    HWND trayHwnd = nid.hWnd;
    BOOL hadActiveTrayItem = g_trayIconActive && trayHwnd;
    if (finalCleanup) {
        g_trayShuttingDown = TRUE;
        CancelTrayRecreateRetry(trayHwnd ? trayHwnd : g_mainHwnd);
    }
    g_trayIconActive = FALSE;

    if (trayHwnd) {
        KillTimer(trayHwnd, TRAY_TIP_TIMER_ID);
        g_trayTipTimerActive = FALSE;
        KillTimer(trayHwnd, TRAY_OPACITY_SAVE_TIMER_ID);
        CompleteTrayOpacityFeedback(trayHwnd, FALSE);
        if (finalCleanup) {
            DiscardPendingTrayOpacitySave();
        } else {
            ReschedulePendingTrayOpacitySave(trayHwnd);
        }
    }

    /* Stop hover detection timer */
    StopTrayHoverDetection();

    /* Uninstall mouse hook */
    TryReleaseTrayMouseHook();

    if (finalCleanup) {
        SystemMonitor_Shutdown();
        g_traySystemMonitorActive = FALSE;
        CleanupTraySubmenuResources();
        g_trayBackgroundWorkEnabled = FALSE;
    }
    if (hadActiveTrayItem) {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
    TrayIconLifetime_ReleaseAll();
    InterlockedExchange(&g_trayTooltipActive, 0);
    g_trayTipTimerActive = FALSE;
    ZeroMemory(&nid, sizeof(nid));
    g_mainHwnd = NULL;
    g_lastTrayTooltip[0] = L'\0';
    TrayHoverRectCache_Reset(&g_trayIconRectCache);
    if (finalCleanup) {
        DiscardPendingTrayOpacitySave();
    }
}

/** @brief Remove tray icon and cleanup */
void RemoveTrayIcon(void) {
    RemoveTrayIconInternal(TRUE);
}

static BOOL IsUtf8ContinuationByte(unsigned char ch) {
    return (ch & 0xC0u) == 0x80u;
}

static size_t FindUtf8PrefixForWideCapacity(const char* text, size_t wideCapacity) {
    if (!text || wideCapacity <= 1) return 0;

    size_t bytes = 0;
    size_t chars = 0;
    while (text[bytes] && chars < wideCapacity - 1) {
        size_t charBytes = 1;
        unsigned char ch = (unsigned char)text[bytes];
        if ((ch & 0x80u) == 0) {
            charBytes = 1;
        } else if ((ch & 0xE0u) == 0xC0u) {
            charBytes = 2;
        } else if ((ch & 0xF0u) == 0xE0u) {
            charBytes = 3;
        } else if ((ch & 0xF8u) == 0xF0u) {
            charBytes = 4;
        } else {
            break;
        }

        for (size_t i = 1; i < charBytes; i++) {
            if (!IsUtf8ContinuationByte((unsigned char)text[bytes + i])) {
                return bytes;
            }
        }

        bytes += charBytes;
        chars++;
    }

    return bytes;
}

/**
 * @brief Show balloon notification
 * @param message UTF-8 message text
 * @note Displays for 3 seconds without title
 */
void ShowTrayNotification(HWND hwnd, const char* message) {
    if (!message) return;
    HWND owner = IsValidTrayMainWindow(hwnd) ? hwnd : GetValidTrayMainWindow();
    if (!owner || !IsTrayIconActiveForWindow(owner)) {
        return;
    }

    NOTIFYICONDATAW nid_notify = {0};
    nid_notify.cbSize = sizeof(NOTIFYICONDATAW);
    nid_notify.hWnd = owner;
    nid_notify.uID = CLOCK_ID_TRAY_APP_ICON;
    nid_notify.uFlags = NIF_INFO;
    nid_notify.dwInfoFlags = NIIF_NONE;
    nid_notify.uTimeout = 3000;

    int converted = MultiByteToWideChar(CP_UTF8, 0, message, -1, nid_notify.szInfo,
                                        (int)_countof(nid_notify.szInfo));
    if (converted <= 0) {
        size_t bytes = FindUtf8PrefixForWideCapacity(message, _countof(nid_notify.szInfo));
        if (bytes == 0 || bytes > (size_t)INT_MAX) {
            return;
        }
        converted = MultiByteToWideChar(CP_UTF8, 0, message, (int)bytes,
                                        nid_notify.szInfo,
                                        (int)_countof(nid_notify.szInfo) - 1);
        if (converted <= 0) {
            return;
        }
        nid_notify.szInfo[converted] = L'\0';
    }
    nid_notify.szInfoTitle[0] = L'\0';

    Shell_NotifyIconW(NIM_MODIFY, &nid_notify);
}

/** @brief Recreate tray icon after taskbar restart */
void RecreateTaskbarIcon(HWND hwnd, HINSTANCE hInstance) {
    KillTimer(hwnd, TRAY_RECREATE_RETRY_TIMER_ID);
    HWND oldTrayHwnd = nid.hWnd;
    BOOL hadActiveTrayItem = g_trayIconActive && oldTrayHwnd;
    g_trayIconActive = FALSE;
    if (oldTrayHwnd) {
