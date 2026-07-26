/**
 * @file tray_animation_update_state.c
 * @brief Shell-update recovery and playback speed snapshots.
 */

#include "tray_animation_core_internal.h"

void NormalizeAnimConfigValue(char* s) {
    if (!s) return;
    char* p = s;
    while (*p && (isspace((unsigned char)*p) || *p == '"' || *p == '\'')) p++;
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '"' || s[len - 1] == '\'')) {
        s[--len] = '\0';
    }
}

/**
 * @brief Record successful update
 */
void RecordSuccessfulUpdate(void) {
    g_consecutiveUpdateFailures = 0;
    g_lastSuccessfulUpdateTime = GetTickCount();
    g_updateFailureReported = FALSE;
    g_lastPreviewShellFailureLogTick = 0;
    InterlockedExchange(&g_shellUpdateBackoffActive, 0);
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, 0);
}

/**
 * @brief Record failed update
 * @return TRUE if repeated failures should trigger recovery handling
 */
BOOL RecordFailedUpdate(void) {
    DWORD currentTime = GetTickCount();
    InterlockedExchange(&g_lastFailedUpdateAttemptTick, (LONG)currentTime);
    if (g_consecutiveUpdateFailures < UINT_MAX) {
        g_consecutiveUpdateFailures++;
    }
    if (g_updateFailureReported) {
        return FALSE;
    }

    if (g_consecutiveUpdateFailures >= MAX_CONSECUTIVE_FAILURES) {
        g_updateFailureReported = TRUE;
        InterlockedExchange(&g_shellUpdateBackoffActive, 1);
        return TRUE;
    }

    if (g_lastSuccessfulUpdateTime > 0) {
        DWORD elapsed = currentTime - g_lastSuccessfulUpdateTime;
        if (elapsed > UPDATE_TIMEOUT_MS) {
            g_updateFailureReported = TRUE;
            InterlockedExchange(&g_shellUpdateBackoffActive, 1);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Keep the selected animation alive across transient Shell failures
 */
void HandleRepeatedTrayUpdateFailure(void) {
    WriteLog(LOG_LEVEL_WARNING,
             "Tray icon update is temporarily unavailable; retaining the selected animation and retrying");
}

void LogPreviewShellFailureThrottled(void) {
    DWORD now = GetTickCount();
    if (g_lastPreviewShellFailureLogTick == 0 ||
        (DWORD)(now - g_lastPreviewShellFailureLogTick) >= UPDATE_TIMEOUT_MS) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Tray preview icon update is temporarily unavailable");
        g_lastPreviewShellFailureLogTick = now ? now : 1u;
    }
}

/**
 * @brief Compute scaled delay based on CPU/memory/timer
 */
double ComputeAnimationSpeedScalePercent(AnimationSpeedMetric metric) {
    if (metric == ANIMATION_SPEED_ORIGINAL) {
        return 100.0;
    }
    if (metric == ANIMATION_SPEED_FIXED) {
        return GetAnimationFixedSpeedMultiplier() * 100.0;
    }

    double percent = 0.0;

    if (metric == ANIMATION_SPEED_CPU) {
        float cpu = 0.0f;
        SystemMonitor_GetCpuUsage(&cpu);
        percent = cpu;
    } else if (metric == ANIMATION_SPEED_TIMER) {
        if (!CLOCK_SHOW_CURRENT_TIME && !CLOCK_COUNT_UP && CLOCK_TOTAL_TIME > 0) {
            double p = (double)countdown_elapsed_time / (double)CLOCK_TOTAL_TIME;
            if (p < 0.0) p = 0.0;
            if (p > 1.0) p = 1.0;
            percent = p * 100.0;
        }
    } else {
        float mem = 0.0f;
        SystemMonitor_GetMemoryUsage(&mem);
        percent = mem;
    }

    BOOL applyScaling = TRUE;
    if (metric == ANIMATION_SPEED_TIMER) {
        if (CLOCK_SHOW_CURRENT_TIME || CLOCK_COUNT_UP || CLOCK_TOTAL_TIME <= 0 || percent >= 100.0) {
            applyScaling = FALSE;
        }
    }

    double scalePercent = applyScaling ? GetAnimationSpeedScaleForPercent(percent) :
                                        GetAnimationSpeedScaleForPercent(0.0);
    return (scalePercent > 0.0) ? scalePercent : 100.0;
}

BOOL IsSpeedScaleCacheFresh(DWORD now, LONG invalidationSerial) {
    return g_speedScaleCache.valid &&
           g_speedScaleCache.invalidationSerial == invalidationSerial &&
           (DWORD)(now - g_speedScaleCache.lastRefreshTick) < SPEED_SCALE_CACHE_TTL_MS;
}

void RefreshSpeedScaleCache(DWORD now, LONG invalidationSerial) {
    AnimationSpeedMetric metric = GetAnimationSpeedMetric();

    g_speedScaleCache.scalePercent = ComputeAnimationSpeedScalePercent(metric);
    g_speedScaleCache.minIntervalMs = GetUserMinIntervalMs();
    g_speedScaleCache.lastRefreshTick = now;
    g_speedScaleCache.invalidationSerial = invalidationSerial;
    g_speedScaleCache.valid = TRUE;
}

void GetPlaybackSpeedSnapshot(double* speedMultiplier,
                                     UINT* minimumFrameIntervalMs) {
    if (!speedMultiplier || !minimumFrameIntervalMs) return;
    DWORD now = GetTickCount();
    LONG invalidationSerial = InterlockedCompareExchange(&g_speedScaleCacheInvalidation, 0, 0);
    if (!IsSpeedScaleCacheFresh(now, invalidationSerial)) {
        RefreshSpeedScaleCache(now, invalidationSerial);
    }

    *speedMultiplier = g_speedScaleCache.scalePercent / 100.0;
    if (!DoubleIsFiniteStrict(*speedMultiplier) || *speedMultiplier < 0.1) {
        *speedMultiplier = 0.1;
    }
    *minimumFrameIntervalMs = g_speedScaleCache.minIntervalMs;
}
