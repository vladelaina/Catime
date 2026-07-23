    if (result.value >= 1024.0) { result.value /= 1024.0; result.unit = L"GB/s"; }

    return result;
}

/**
 * @brief Determine animation type from name
 * @return Type category (CUSTOM, LOGO, CPU, MEMORY)
 */
static AnimationType GetAnimationType(const char* animName) {
    if (!animName) return ANIM_TYPE_CUSTOM;
    if (_stricmp(animName, "__logo__") == 0) return ANIM_TYPE_LOGO;
    if (_stricmp(animName, "__cpu__") == 0) return ANIM_TYPE_CPU;
    if (_stricmp(animName, "__mem__") == 0) return ANIM_TYPE_MEMORY;
    if (_stricmp(animName, "__battery__") == 0) return ANIM_TYPE_BATTERY;
    if (_stricmp(animName, "__capslock__") == 0) return ANIM_TYPE_CAPSLOCK;
    if (_stricmp(animName, "__none__") == 0) return ANIM_TYPE_NONE;
    return ANIM_TYPE_CUSTOM;
}

/** @brief Check if type is percent-based (CPU/Memory/Battery) */
static inline BOOL IsPercentIcon(AnimationType type) {
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY || type == ANIM_TYPE_BATTERY;
}

static inline BOOL IsDynamicBuiltinIcon(AnimationType type) {
    return IsPercentIcon(type) || type == ANIM_TYPE_CAPSLOCK;
}

static BOOL CurrentTrayIconNeedsBackgroundRefresh(void) {
    return IsDynamicBuiltinIcon(GetAnimationType(GetCurrentAnimationName()));
}

static BOOL CurrentTrayIconNeedsSystemMonitor(void) {
    AnimationType type = GetAnimationType(GetCurrentAnimationName());
    return type == ANIM_TYPE_CPU || type == ANIM_TYPE_MEMORY;
}

static BOOL CurrentAnimationSpeedNeedsSystemMonitor(void) {
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();
    const char* animName = GetCurrentAnimationName();
    return TrayAnimation_IsRunning() &&
           (metric == ANIMATION_SPEED_CPU || metric == ANIMATION_SPEED_MEMORY) &&
           GetAnimationType(animName) == ANIM_TYPE_CUSTOM &&
           !IsStaticImageFile(animName);
}

static BOOL ShouldKeepSystemMonitorActive(void) {
    HWND hwnd = GetValidTrayMainWindow();
    if (!g_trayBackgroundWorkEnabled ||
        !hwnd ||
        !IsTrayIconActiveForWindow(hwnd)) {
        return FALSE;
    }

    if (IsTrayInteractionSuspended()) {
        return CurrentTrayIconNeedsSystemMonitor() ||
               CurrentAnimationSpeedNeedsSystemMonitor();
    }

    return IsTrayTooltipActive() ||
           CurrentTrayIconNeedsSystemMonitor() ||
           CurrentAnimationSpeedNeedsSystemMonitor();
}

static void EnsureTraySystemMonitorActive(void) {
    if (!g_traySystemMonitorActive) {
        SystemMonitor_Init();
        g_traySystemMonitorActive = TRUE;
    }
}

static void ReleaseIdleTraySystemMonitor(void) {
    if (!ShouldKeepSystemMonitorActive() &&
        (g_traySystemMonitorActive || SystemMonitor_IsInitialized())) {
        SystemMonitor_Shutdown();
        g_traySystemMonitorActive = FALSE;
    }
}

static BOOL ShouldRunTrayBackgroundTimer(HWND hwnd) {
    return g_trayBackgroundWorkEnabled &&
           !IsTrayInteractionSuspended() &&
           IsTrayIconActiveForWindow(hwnd) &&
           (IsTrayTooltipActive() ||
            CurrentTrayIconNeedsBackgroundRefresh() ||
            TrayAnimation_HasDeferredIconUpdate());
}

void RefreshTrayBackgroundWorkState(void) {
    HWND hwnd = GetValidTrayMainWindow();
    BOOL shouldRun = hwnd && ShouldRunTrayBackgroundTimer(hwnd);
    BOOL keepSystemMonitor = ShouldKeepSystemMonitorActive();
    if (keepSystemMonitor) {
        EnsureTraySystemMonitorActive();
    }

    if (shouldRun) {
        if (!g_trayTipTimerActive) {
            if (SetTimer(hwnd, TRAY_TIP_TIMER_ID, TOOLTIP_UPDATE_INTERVAL_MS,
                         (TIMERPROC)TrayTipTimerProc)) {
                g_trayTipTimerActive = TRUE;
            } else {
                LOG_WARNING("Failed to start tray background refresh timer (error=%lu)",
                            GetLastError());
            }
        }
        if (!keepSystemMonitor) {
            ReleaseIdleTraySystemMonitor();
        }
        return;
    }

    if (hwnd) {
        KillTimer(hwnd, TRAY_TIP_TIMER_ID);
    }
    g_trayTipTimerActive = FALSE;
    ReleaseIdleTraySystemMonitor();
}

void SetTrayTooltipActive(BOOL active) {
    HWND hwnd = GetValidTrayMainWindow();
    BOOL normalized = active && hwnd && IsTrayIconActiveForWindow(hwnd);
    BOOL wasActive = IsTrayTooltipActive();

    if (wasActive == normalized) {
        return;
    }

    InterlockedExchange(&g_trayTooltipActive, normalized ? 1L : 0L);
    RefreshTrayBackgroundWorkState();

    if (IsTrayTooltipActive() && hwnd) {
        TrayTipTimerProc(hwnd, WM_TIMER, TRAY_TIP_TIMER_ID, 0);
    } else if (wasActive && hwnd && IsTrayIconActiveForWindow(hwnd) &&
               !IsTrayInteractionSuspended()) {
        /* Static icons have no animation timer to flush a presentation that
         * was deliberately deferred during hover. Refresh once on the real
         * leave transition; animated icons also resume at their latest phase. */
        TrayAnimation_RefreshCurrentIcon();
        RefreshTrayBackgroundWorkState();
    }
}

BOOL IsTrayTooltipActive(void) {
    return InterlockedCompareExchange(&g_trayTooltipActive, 0, 0) != 0;
}

/**
 * @brief Check if filename is static image
 * @return TRUE for .ico, .png, .bmp, .jpg, .tif extensions
 */
static BOOL IsStaticImageFile(const char* filename) {
    if (!filename) return FALSE;

    static const char* staticExtensions[] = {
        ".ico", ".png", ".bmp", ".jpg", ".jpeg", ".tif", ".tiff", NULL
    };

    size_t len = strlen(filename);
    for (int i = 0; staticExtensions[i]; i++) {
        size_t extLen = strlen(staticExtensions[i]);
        if (len >= extLen) {
            const char* fileSuffix = filename + (len - extLen);
            if (_stricmp(fileSuffix, staticExtensions[i]) == 0) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/**
 * @brief Get system metrics with zero-value warm-up
 * @note Forces refresh if percent icon initially shows 0%
 */
static void GetSystemMetricsWithWarmup(float* cpu, float* mem) {
    EnsureTraySystemMonitorActive();
    SystemMonitor_GetUsage(cpu, mem);

    static AnimationType s_lastWarmupType = ANIM_TYPE_CUSTOM;
    static BOOL s_warmupAttempted = FALSE;
    AnimationType type = GetAnimationType(GetCurrentAnimationName());
    if (type != s_lastWarmupType) {
        s_lastWarmupType = type;
        s_warmupAttempted = FALSE;
    }

    if (type != ANIM_TYPE_CPU && type != ANIM_TYPE_MEMORY) {
        return;
    }

    float chosen = (type == ANIM_TYPE_CPU) ? *cpu : *mem;
    if (!s_warmupAttempted) {
        s_warmupAttempted = TRUE;
        if ((int)(chosen + 0.5f) == 0) {
            SystemMonitor_ForceRefresh();
            SystemMonitor_GetUsage(cpu, mem);
        }
    }
}

/**
 * @brief Build a stable tooltip with CPU, memory, and network lines
 *
 * Keep the same layout even before the first network sample is ready so the
 * shell tooltip does not switch between separate CPU/memory and network views.
 */
static void BuildBasicTooltip(wchar_t* tip, size_t tipSize, float cpu, float mem,
                              float upBps, float downBps, BOOL hasNet) {
    const wchar_t* cpuLabel = GetLocalizedString(NULL, L"Tray Tooltip CPU");
    const wchar_t* memoryLabel = GetLocalizedString(NULL, L"Tray Tooltip Memory");
    const wchar_t* uploadLabel = GetLocalizedString(NULL, L"Tray Tooltip Upload");
    const wchar_t* downloadLabel = GetLocalizedString(NULL, L"Tray Tooltip Download");
    if (hasNet) {
        FormattedBytes upload = FormatBytesPerSecond((double)upBps);
        FormattedBytes download = FormatBytesPerSecond((double)downBps);
        _snwprintf_s(tip, tipSize, _TRUNCATE,
                     L"%s %.1f%%\n%s %.1f%%\n%s %.1f %s\n%s %.1f %s",
                     cpuLabel, cpu, memoryLabel, mem,
                     uploadLabel, upload.value, upload.unit,
                     downloadLabel, download.value, download.unit);
    } else {
        _snwprintf_s(tip, tipSize, _TRUNCATE,
                     L"%s %.1f%%\n%s %.1f%%\n%s\n%s",
                     cpuLabel, cpu, memoryLabel, mem,
                     uploadLabel, downloadLabel);
    }
}

/**
 * @brief Check if animation speed should be displayed in tooltip
 * @return FALSE for builtin icons and static images
 */
static BOOL ShouldShowAnimationSpeed(const char* animName) {
    if (!animName) return FALSE;

    if (IsBuiltinAnimationName(animName)) return FALSE;
    if (IsStaticImageFile(animName)) return FALSE;

    return TRUE;
}

/**
 * @brief Calculate speed percentage from metric
 * @return Percentage for CPU/Memory, or countdown progress for Timer
 */
static double CalculateSpeedMetricPercent(AnimationSpeedMetric metric, float cpu, float mem) {
    if (metric == ANIMATION_SPEED_CPU) {
        return (double)cpu;
    }

    if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            return p * 100.0;
        }
        return 0.0;
    }

    if (metric == ANIMATION_SPEED_FIXED) {
        return 0.0;
    }

    return (double)mem;
}

/** @brief Append tray icon animation speed line to tooltip */
static void AppendSpeedLine(wchar_t* tip, size_t tipSize, AnimationSpeedMetric metric,
                           float cpu, float mem) {
    /* Original speed mode: don't show speed line (always 100%, no useful info) */
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return;
    }

    if (metric == ANIMATION_SPEED_FIXED) {
        wchar_t extra[128];
        _snwprintf_s(extra, _countof(extra), _TRUNCATE,
                     L"\n%s · %s %.3gx",
                     GetLocalizedString(NULL, L"Tray Tooltip Animation Speed"),
                     GetLocalizedString(NULL, L"Tray Tooltip Fixed Metric"),
                     GetAnimationFixedSpeedMultiplier());
        wcsncat_s(tip, tipSize, extra, _TRUNCATE);
        return;
    }

    double percent = CalculateSpeedMetricPercent(metric, cpu, mem);
