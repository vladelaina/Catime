    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER) {
        if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0 || percent >= 100.0) {
            applyScaling = FALSE;
        }
    }

    double scalePercent = GetAnimationSpeedScaleForPercent(applyScaling ? percent : 0.0);
    if (scalePercent <= 0.0) scalePercent = 100.0;

    const wchar_t* metricLabel =
        GetLocalizedString(NULL, L"Tray Tooltip Memory Metric");
    if (metric == ANIMATION_SPEED_CPU) {
        metricLabel = GetLocalizedString(NULL, L"Tray Tooltip CPU Metric");
    } else if (metric == ANIMATION_SPEED_TIMER) {
        metricLabel = GetLocalizedString(NULL, L"Tray Tooltip Timer Metric");
    }

    wchar_t extra[128];
    _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                 L"\n%s · %s: %.0f%%",
                 GetLocalizedString(NULL, L"Tray Tooltip Animation Speed"),
                 metricLabel, scalePercent);
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

static void AppendUptimeLine(wchar_t* tip, size_t tipSize) {
    ULONGLONG totalMinutes = GetTickCount64() / 60000ULL;
    ULONGLONG days = totalMinutes / (24ULL * 60ULL);
    ULONGLONG hours = (totalMinutes / 60ULL) % 24ULL;
    ULONGLONG minutes = totalMinutes % 60ULL;

    wchar_t extra[64];
    if (days > 0ULL) {
        const wchar_t* format = GetLocalizedString(
            NULL, L"Tray Tooltip Uptime Days");
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     format, days, hours, minutes);
    } else if (hours > 0ULL) {
        const wchar_t* format = GetLocalizedString(
            NULL, L"Tray Tooltip Uptime Hours");
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     format, hours, minutes);
    } else {
        const wchar_t* format = GetLocalizedString(
            NULL, L"Tray Tooltip Uptime Minutes");
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     format, minutes);
    }
    wcsncat_s(tip, tipSize, extra, _TRUNCATE);
}

/** @brief Update tray icon tooltip */
void UpdateTrayTooltip(const wchar_t* tip) {
    HWND owner = GetValidTrayMainWindow();
    if (!tip || !owner || !IsTrayIconActiveForWindow(owner)) {
        return;
    }

    NOTIFYICONDATAW n = {0};
    n.cbSize = sizeof(n);
    n.hWnd = owner;
    n.uID = CLOCK_ID_TRAY_APP_ICON;
    n.uFlags = NIF_TIP;
    wcsncpy_s(n.szTip, _countof(n.szTip), tip, _TRUNCATE);
    if (wcscmp(g_lastTrayTooltip, n.szTip) == 0) {
        return;
    }
    if (Shell_NotifyIconW(NIM_MODIFY, &n)) {
        wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip), n.szTip);
    }
}

static void ClearPendingTrayOpacitySave(void) {
    g_pendingOpacityToSave = -1;
    g_opacityRollbackValue = -1;
    g_pendingOpacitySaveRetryCount = 0;
}

static void RollBackPendingTrayOpacitySave(HWND hwnd) {
    if (g_opacityRollbackValue >= 0) {
        CLOCK_WINDOW_OPACITY = g_opacityRollbackValue;
        if (IsValidTrayMainWindow(hwnd)) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    ClearPendingTrayOpacitySave();
}

static void FlushPendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave >= 0) {
        char configPath[MAX_PATH];
        GetConfigPath(configPath, sizeof(configPath));
        if (!WriteIniInt(INI_SECTION_DISPLAY, "WINDOW_OPACITY",
                         g_pendingOpacityToSave, configPath)) {
            g_pendingOpacitySaveRetryCount++;
            if (g_pendingOpacitySaveRetryCount >= TRAY_OPACITY_SAVE_MAX_RETRIES) {
                LOG_WARNING("Failed to save tray opacity after %d attempts; dropping pending value: %d",
                            g_pendingOpacitySaveRetryCount, g_pendingOpacityToSave);
                RollBackPendingTrayOpacitySave(hwnd);
                return;
            }
            LOG_WARNING("Failed to save tray opacity: %d (attempt %d/%d)",
                        g_pendingOpacityToSave,
                        g_pendingOpacitySaveRetryCount,
                        TRAY_OPACITY_SAVE_MAX_RETRIES);
            return;
        }
        ClearPendingTrayOpacitySave();
    }
}

static void DiscardPendingTrayOpacitySave(void) {
    ClearPendingTrayOpacitySave();
}

static void EndTrayOpacityPreview(HWND hwnd) {
    RestoreWindowVisibility(hwnd);
}

static void ReschedulePendingTrayOpacitySave(HWND hwnd) {
    if (g_pendingOpacityToSave < 0) {
        return;
    }

    if (!IsValidTrayMainWindow(hwnd)) {
        return;
    }

    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS, TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule pending tray opacity save (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

static void CompleteTrayOpacityFeedback(HWND hwnd, BOOL refreshTooltip) {
    FlushPendingTrayOpacitySave(hwnd);

    if (g_showingOpacityTip) {
        g_showingOpacityTip = FALSE;
        EndTrayOpacityPreview(hwnd);
        if (refreshTooltip && IsTrayTooltipActive() && hwnd && nid.hWnd) {
            TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
        }
        RefreshTrayBackgroundWorkState();
    }
}

static void CALLBACK TrayOpacitySaveTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg;
    (void)time;

    if (id != TRAY_OPACITY_SAVE_TIMER_ID || !IsValidTrayMainWindow(hwnd)) return;

    KillTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID);
    CompleteTrayOpacityFeedback(hwnd, TRUE);
    if (g_pendingOpacityToSave >= 0 &&
        !SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID,
                  TRAY_OPACITY_SAVE_DELAY_MS, TrayOpacitySaveTimerProc)) {
        LOG_WARNING("Failed to reschedule tray opacity save retry (error=%lu)",
                    GetLastError());
        RollBackPendingTrayOpacitySave(hwnd);
    }
}

void HandleTrayOpacityWheel(HWND hwnd, int wheelDirection, BOOL ctrlPressed) {
    if (!IsValidTrayMainWindow(hwnd)) return;

    extern int ReadConfigOpacityStepNormal(void);
    extern int ReadConfigOpacityStepFast(void);

    int step = ctrlPressed ? ReadConfigOpacityStepFast() : ReadConfigOpacityStepNormal();
    if (step <= 0) step = 1;

    int oldOpacity = CLOCK_WINDOW_OPACITY;
    CLOCK_WINDOW_OPACITY += (wheelDirection > 0) ? step : -step;
    if (CLOCK_WINDOW_OPACITY < MIN_VISIBLE_OPACITY) CLOCK_WINDOW_OPACITY = MIN_VISIBLE_OPACITY;
    if (CLOCK_WINDOW_OPACITY > MAX_OPACITY) CLOCK_WINDOW_OPACITY = MAX_OPACITY;

    ShowWindowForPreview(hwnd);

    g_showingOpacityTip = TRUE;
    wchar_t opacityTip[64];
    _snwprintf_s(opacityTip, _countof(opacityTip), _TRUNCATE,
                 L"%s: %d%%",
                 GetLocalizedString(NULL, L"Tray Tooltip Opacity"),
                 CLOCK_WINDOW_OPACITY);
    UpdateTrayTooltip(opacityTip);

    if (CLOCK_WINDOW_OPACITY != oldOpacity) {
        InvalidateRect(hwnd, NULL, FALSE);
        if (g_pendingOpacityToSave < 0) {
            g_opacityRollbackValue = oldOpacity;
        }
        g_pendingOpacityToSave = CLOCK_WINDOW_OPACITY;
        g_pendingOpacitySaveRetryCount = 0;
    }

    if (!SetTimer(hwnd, TRAY_OPACITY_SAVE_TIMER_ID, TRAY_OPACITY_SAVE_DELAY_MS,
                  TrayOpacitySaveTimerProc)) {
        CompleteTrayOpacityFeedback(hwnd, TRUE);
        if (g_pendingOpacityToSave >= 0) {
            LOG_WARNING("Dropping pending tray opacity save after timer start failure");
            RollBackPendingTrayOpacitySave(hwnd);
        }
    }
}

/**
 * @brief On-demand tray background refresh callback
 * @note Updates tooltip only while hovered; otherwise keeps dynamic icons fresh.
 */
void CALLBACK TrayTipTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)time;

    if (msg != WM_TIMER || id != TRAY_TIP_TIMER_ID) {
        return;
    }

    if (!IsTrayIconActiveForWindow(hwnd)) {
        if (hwnd) {
            KillTimer(hwnd, TRAY_TIP_TIMER_ID);
        }
        g_trayTipTimerActive = FALSE;
        return;
    }

    BOOL interactionSuspended = IsTrayInteractionSuspended();
    BOOL dynamicIcon = CurrentTrayIconNeedsBackgroundRefresh();

    if (interactionSuspended) {
        if (dynamicIcon) {
            if (CurrentTrayIconNeedsSystemMonitor()) {
                EnsureTraySystemMonitorActive();
            }
            TrayAnimation_UpdatePercentIconIfNeeded();
        }
        RefreshTrayBackgroundWorkState();
        return;
    }

    /* Keep percent icons fresh while preserving the temporary opacity tooltip. */
    if (g_showingOpacityTip) {
        if (dynamicIcon) {
            if (CurrentTrayIconNeedsSystemMonitor()) {
                EnsureTraySystemMonitorActive();
            }
            TrayAnimation_UpdatePercentIconIfNeeded();
        }
        RefreshTrayBackgroundWorkState();
        return;
    }

    if (!IsTrayTooltipActive()) {
        BOOL flushedDeferredIcon = TrayAnimation_HasDeferredIconUpdate();
        if (flushedDeferredIcon) {
            TrayAnimation_RefreshCurrentIcon();
        }
        if (dynamicIcon && !flushedDeferredIcon) {
            if (CurrentTrayIconNeedsSystemMonitor()) {
                EnsureTraySystemMonitorActive();
            }
            TrayAnimation_UpdatePercentIconIfNeeded();
        }
        RefreshTrayBackgroundWorkState();
        return;
    }

    float cpu, mem, upBps, downBps;
    GetSystemMetricsWithWarmup(&cpu, &mem);
    BOOL hasNet = SystemMonitor_GetNetSpeed(&upBps, &downBps);

    wchar_t tip[256] = {0};
    BuildBasicTooltip(tip, _countof(tip), cpu, mem, upBps, downBps, hasNet);

    const char* animName = GetCurrentAnimationName();
    AnimationSpeedMetric metric = ANIMATION_SPEED_ORIGINAL;
    BOOL showAnimationSpeed = FALSE;
    if (ShouldShowAnimationSpeed(animName)) {
        metric = GetAnimationSpeedMetric();
        showAnimationSpeed = (metric != ANIMATION_SPEED_ORIGINAL);
    }
    AppendUptimeLine(tip, _countof(tip));
    if (showAnimationSpeed) {
        AppendSpeedLine(tip, _countof(tip), metric, cpu, mem);
    }

    UpdateTrayTooltip(tip);
    if (dynamicIcon) {
        if (CurrentTrayIconNeedsSystemMonitor()) {
            EnsureTraySystemMonitorActive();
        }
        TrayAnimation_UpdatePercentIconWithMetrics(cpu, mem);
    }
    RefreshTrayBackgroundWorkState();
}
