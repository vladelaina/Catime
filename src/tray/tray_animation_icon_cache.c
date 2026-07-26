/**
 * @file tray_animation_icon_cache.c
 * @brief Builtin-icon and playback cache bookkeeping.
 */

#include "tray_animation_core_internal.h"

void ResetBuiltinIconUpdateCache(void) {
    g_lastBuiltinIconName[0] = '\0';
    g_lastBuiltinIconValue = -1;
    g_lastBuiltinIconTextColor = CLR_INVALID;
    g_lastBuiltinIconBgColor = CLR_INVALID;
    g_lastBuiltinIconCx = 0;
    g_lastBuiltinIconCy = 0;
}

BOOL IsBuiltinIconUpdateCacheCurrent(const char* name,
                                            int value,
                                            COLORREF textColor,
                                            COLORREF bgColor,
                                            int iconCx,
                                            int iconCy) {
    return name &&
           _stricmp(g_lastBuiltinIconName, name) == 0 &&
           g_lastBuiltinIconValue == value &&
           g_lastBuiltinIconTextColor == textColor &&
           g_lastBuiltinIconBgColor == bgColor &&
           g_lastBuiltinIconCx == iconCx &&
           g_lastBuiltinIconCy == iconCy;
}

BOOL TryGetCachedBuiltinIconValue(const char* name, int* value) {
    if (!name || !value) {
        return FALSE;
    }

    if (g_lastBuiltinIconValue >= 0 &&
        _stricmp(g_lastBuiltinIconName, name) == 0) {
        *value = g_lastBuiltinIconValue;
        return TRUE;
    }

    if (g_lastStablePercentIconValue >= 0 &&
        _stricmp(g_lastStablePercentIconName, name) == 0) {
        *value = g_lastStablePercentIconValue;
        return TRUE;
    }

    return FALSE;
}

BOOL IsTransientZeroPronePercentIcon(const char* name) {
    return name &&
           (_stricmp(name, "__cpu__") == 0 ||
            _stricmp(name, "__mem__") == 0);
}

BOOL ShouldPreserveCachedPercentIconValue(const char* name, int sampledValue) {
    int cachedValue = -1;
    return sampledValue == 0 &&
           IsTransientZeroPronePercentIcon(name) &&
           (!TryGetCachedBuiltinIconValue(name, &cachedValue) || cachedValue > 0);
}

void RecordBuiltinIconUpdateCache(const char* name,
                                         int value,
                                         COLORREF textColor,
                                         COLORREF bgColor,
                                         int iconCx,
                                         int iconCy) {
    if (!name) return;

    strncpy(g_lastBuiltinIconName, name, sizeof(g_lastBuiltinIconName) - 1);
    g_lastBuiltinIconName[sizeof(g_lastBuiltinIconName) - 1] = '\0';
    g_lastBuiltinIconValue = value;
    g_lastBuiltinIconTextColor = textColor;
    g_lastBuiltinIconBgColor = bgColor;
    g_lastBuiltinIconCx = iconCx;
    g_lastBuiltinIconCy = iconCy;

    if (value > 0 && IsTransientZeroPronePercentIcon(name)) {
        strncpy(g_lastStablePercentIconName, name, sizeof(g_lastStablePercentIconName) - 1);
        g_lastStablePercentIconName[sizeof(g_lastStablePercentIconName) - 1] = '\0';
        g_lastStablePercentIconValue = value;
    }
}

BOOL CopyStringExactA(const char* src, char* out, size_t outSize) {
    if (!out || outSize == 0) return FALSE;
    out[0] = '\0';
    if (!src) return FALSE;

    size_t srcLen = strlen(src);
    if (srcLen >= outSize) return FALSE;

    memcpy(out, src, srcLen + 1);
    return TRUE;
}

void InvalidateSpeedScaleCache(void) {
    InterlockedIncrement(&g_speedScaleCacheInvalidation);
}

void ResetFramePlaybackState(void) {
    AnimationPlayback_Reset(&g_playbackState);
    g_lastAnimationTickMs = 0;
    g_playbackGeneration++;
    g_framePresentationSerial = 0;
    g_lastShellPresentationSerial = 0;
    g_lastShellFrameIndex = -1;
    g_lastShellFrameCount = 0;
    g_lastShellAnimatedFrameValid = FALSE;
    g_lastShellAnimationName[0] = '\0';
    g_frameRateCtrl.trayAccumulatorMs = 0.0;
    g_trayFrameDirty = FALSE;
}

/* Error recovery:
 * - 5000ms timeout: Reasonable duration before declaring icon update as failed
 */
