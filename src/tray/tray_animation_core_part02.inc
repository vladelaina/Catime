        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetValidTrayAnimationWindow(void) {
    HWND hwnd = g_trayHwnd;
    return IsValidTrayAnimationWindow(hwnd) && IsTrayIconActive(hwnd) ? hwnd : NULL;
}

static BOOL BuildAnimationConfigPath(const char* name, char* animPath, size_t animPathSize) {
    if (!name || !animPath || animPathSize == 0) return FALSE;

    int pathLen = 0;
    if (IsBuiltinAnimationName(name)) {
        pathLen = snprintf(animPath, animPathSize, "%s", name);
    } else {
        if (!IsSafeAnimationRelativePath(name)) return FALSE;

        pathLen = snprintf(animPath, animPathSize,
                           "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", name);
    }

    if (pathLen < 0 || (size_t)pathLen >= animPathSize) {
        animPath[0] = '\0';
        LOG_WARNING("Animation config path too long: %s", name);
        return FALSE;
    }

    return TRUE;
}

static BOOL WriteAnimationConfigPathIfChanged(const char* configPath, const char* animPath) {
    if (!configPath || !animPath) return FALSE;

    char currentPath[MAX_PATH] = {0};
    BOOL hasCompleteCurrentPath = ReadIniStringExact(
        "Animation", "ANIMATION_PATH", "", currentPath, sizeof(currentPath), configPath);
    if (hasCompleteCurrentPath && strcmp(currentPath, animPath) == 0) {
        return TRUE;
    }
    if (!hasCompleteCurrentPath) {
        LOG_WARNING("Replacing ANIMATION_PATH because the existing config value is too long");
    }

    return WriteIniString("Animation", "ANIMATION_PATH", animPath, configPath);
}

static BOOL WriteAnimationNameToConfigIfChanged(const char* name) {
    char configPath[MAX_PATH] = {0};
    char animPath[MAX_PATH] = {0};

    GetConfigPath(configPath, sizeof(configPath));
    if (!BuildAnimationConfigPath(name, animPath, sizeof(animPath))) {
        return FALSE;
    }

    return WriteAnimationConfigPathIfChanged(configPath, animPath);
}

static void ReadAnimationNameFromConfig(char* name, size_t nameSize, const char* configPath) {
    if (!name || nameSize == 0) return;

    char nameBuf[MAX_PATH] = {0};
    if (!ReadIniStringExact("Animation", "ANIMATION_PATH", "__logo__",
                            nameBuf, sizeof(nameBuf), configPath)) {
        LOG_WARNING("Ignoring ANIMATION_PATH because the config value is too long");
        return;
    }
    NormalizeAnimConfigValue(nameBuf);

    if (nameBuf[0] == '\0') {
        return;
    }

    const char* prefix = ANIMATIONS_PATH_PREFIX;
    if (IsBuiltinAnimationName(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    } else if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
        const char* rel = nameBuf + strlen(prefix);
        if (IsSafeAnimationRelativePath(rel)) {
            CopyStringExactA(rel, name, nameSize);
        }
    } else if (IsSafeAnimationRelativePath(nameBuf)) {
        CopyStringExactA(nameBuf, name, nameSize);
    }
}
static DWORD WINAPI PreviewWorkerThread(LPVOID param);

static BOOL ShouldRunTrayAnimationTimer(void) {
    const LoadedAnimation* currentAnim = g_isPreviewActive ? &g_previewAnimation : &g_mainAnimation;
    return currentAnim->isAnimated && currentAnim->count > 1;
}

static void EnsureTrayAnimationTimerState(void) {
    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) {
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
        g_trayFrameDirty = FALSE;
        ClearPendingTrayUpdate();
        return;
    }

    BOOL shouldRun = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        shouldRun = ShouldRunTrayAnimationTimer();
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        shouldRun = ShouldRunTrayAnimationTimer();
    }

    if (shouldRun) {
        if (!IsAnimationTimerActive()) {
            FrameRateController_Init(&g_frameRateCtrl, TRAY_UPDATE_INTERVAL_MS);
            if (!InitializeAnimationTimer(trayHwnd, INTERNAL_TICK_INTERVAL_MS,
                                          TrayAnimationTimerCallback, NULL)) {
                LOG_WARNING("Failed to start tray animation timer (interval=%u)",
                            (unsigned)INTERNAL_TICK_INTERVAL_MS);
                ClearPendingTrayUpdate();
            }
        }
    } else {
        g_trayFrameDirty = FALSE;
        if (IsAnimationTimerActive()) {
            if (!CleanupAnimationTimer()) {
                LOG_WARNING("Animation timer cleanup timed out; retaining runtime resources");
            }
        }
    }
}

static void SwapLoadedAnimation(LoadedAnimation* target, const LoadedAnimation* source) {
    if (!target || !source) return;
    *target = *source;
}

static void CleanupCompletedPreviewWorkerLocked(void) {
    if (!g_previewWorkerThread) {
        return;
    }

    DWORD waitResult = WaitForSingleObject(g_previewWorkerThread, 0);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_FAILED) {
            WriteLog(LOG_LEVEL_WARNING, "Preview worker status check failed (error=%lu)", GetLastError());
        }
        return;
    }

    CloseHandle(g_previewWorkerThread);
    g_previewWorkerThread = NULL;
    g_previewWorkerRetiring = FALSE;

    if (g_previewRequestEvent) {
        CloseHandle(g_previewRequestEvent);
        g_previewRequestEvent = NULL;
    }

    if (g_previewStopEvent) {
        CloseHandle(g_previewStopEvent);
        g_previewStopEvent = NULL;
    }

    if (g_previewCancelEvent) {
        CloseHandle(g_previewCancelEvent);
        g_previewCancelEvent = NULL;
    }
}

static void CleanupRetiredPreviewWorkerOnExit(void) {
    AcquireSRWLockExclusive(&g_previewWorkerLock);
    if (g_previewWorkerRetiring) {
        if (g_previewWorkerThread) {
            CloseHandle(g_previewWorkerThread);
            g_previewWorkerThread = NULL;
        }
        g_previewWorkerRetiring = FALSE;

        if (g_previewRequestEvent) {
            CloseHandle(g_previewRequestEvent);
            g_previewRequestEvent = NULL;
        }

        if (g_previewStopEvent) {
            CloseHandle(g_previewStopEvent);
            g_previewStopEvent = NULL;
        }

        if (g_previewCancelEvent) {
            CloseHandle(g_previewCancelEvent);
            g_previewCancelEvent = NULL;
        }
    }
    ReleaseSRWLockExclusive(&g_previewWorkerLock);
}

static void SignalPreviewDecodeCancelLocked(void) {
    if (g_previewCancelEvent) {
        SetEvent(g_previewCancelEvent);
    }
}

static void WakePreviewWorkerLocked(void) {
    if (g_previewRequestEvent) {
        SetEvent(g_previewRequestEvent);
    }
}

static BOOL IsPreviewWorkerStartFailureCoolingDown(DWORD now) {
    return g_previewWorkerStartFailureCooldownUntil != 0 &&
           (LONG)(g_previewWorkerStartFailureCooldownUntil - now) > 0;
}

static void MarkPreviewWorkerStartFailure(DWORD now) {
    DWORD cooldownUntil = now + PREVIEW_WORKER_START_RETRY_COOLDOWN_MS;
    g_previewWorkerStartFailureCooldownUntil = cooldownUntil ? cooldownUntil : 1;
}

static BOOL EnsurePreviewWorkerStartedLocked(void) {
    CleanupCompletedPreviewWorkerLocked();

    if (g_previewWorkerThread) {
        if (g_previewWorkerRetiring) {
            return FALSE;
        }
        return TRUE;
    }

    DWORD now = GetTickCount();
    if (IsPreviewWorkerStartFailureCoolingDown(now)) {
        return FALSE;
    }

    if (!g_previewRequestEvent) {
        g_previewRequestEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!g_previewRequestEvent) {
            WriteLog(LOG_LEVEL_ERROR, "Failed to create preview request event");
            MarkPreviewWorkerStartFailure(now);
            return FALSE;
        }
    }

    if (!g_previewStopEvent) {
