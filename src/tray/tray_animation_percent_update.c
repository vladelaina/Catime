/**
 * @file tray_animation_percent_update.c
 * @brief Dynamic percent and Caps Lock icon refresh.
 */

#include "tray_animation_core_internal.h"

void UpdatePercentIconIfNeededInternal(BOOL hasMetricsSnapshot,
                                              float cpuPercent,
                                              float memPercent) {
    if (!BeginTrayAnimationRuntimeUse()) return;

    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) goto done;
    /* Replacing the icon while Explorer owns a hover tooltip can make the
     * notification area briefly tear down and recreate that tooltip.  A
     * popup menu does not own that tooltip, so dynamic built-in icons may
     * continue refreshing while either tray menu is open. */
    if (IsTrayTooltipActive()) goto done;

    char animationName[MAX_PATH] = {0};
    BOOL previewActive = FALSE;
    if (IsAnimCriticalSectionReady()) {
        EnterCriticalSection(&g_animCriticalSection);
        previewActive = g_isPreviewActive;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
        LeaveCriticalSection(&g_animCriticalSection);
    } else {
        previewActive = g_isPreviewActive;
        CopyStringExactA(previewActive ? g_previewAnimationName : g_animationName,
                         animationName, sizeof(animationName));
    }

    if (!animationName[0]) goto done;

    const BuiltinAnimDef* def = GetBuiltinAnimDef(animationName);
    if (!def) goto done;

    HICON hIcon = NULL;
    int value = -1;
    COLORREF textColor = RGB(0, 0, 0);
    COLORREF bgColor = TRANSPARENT_BG_AUTO;
    int iconCx = 0;
    int iconCy = 0;
    if (!GetPercentIconColorSnapshot(&textColor, &bgColor)) goto done;
    GetGeneratedTrayIconSizeSnapshot(&iconCx, &iconCy);

    /* Handle percent type (CPU, Memory, Battery) */
    if (def->type == ANIM_SOURCE_PERCENT) {
        int p = 0;
        if (hasMetricsSnapshot && _stricmp(animationName, "__cpu__") == 0) {
            p = (int)(cpuPercent + 0.5f);
        } else if (hasMetricsSnapshot && _stricmp(animationName, "__mem__") == 0) {
            p = (int)(memPercent + 0.5f);
        } else if (def->getValue) {
            p = def->getValue();
        }
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        value = p;
        if (ShouldPreserveCachedPercentIconValue(animationName, value)) {
            goto done;
        }
        if (IsBuiltinIconUpdateCacheCurrent(animationName, value,
                                            textColor, bgColor,
                                            iconCx, iconCy)) {
            goto done;
        }
        hIcon = CreatePercentIcon16(p);
    }
    /* Handle Caps Lock indicator */
    else if (def->type == ANIM_SOURCE_CAPSLOCK) {
        value = IsCapsLockOn() ? 1 : 0;
        if (IsBuiltinIconUpdateCacheCurrent(animationName, value,
                                            textColor, bgColor,
                                            iconCx, iconCy)) {
            goto done;
        }
        hIcon = CreateCapsLockIcon(value != 0);
    }
    else {
        goto done;
    }

    if (!hIcon) goto done;

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    if (Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        TrayIconLifetime_Retain(hIcon);
        hIcon = NULL;
        RecordBuiltinIconUpdateCache(animationName, value,
                                     textColor, bgColor,
                                     iconCx, iconCy);
    }

    if (hIcon) DestroyIcon(hIcon);

done:
    EndTrayAnimationRuntimeUse();
}

void TrayAnimation_UpdatePercentIconIfNeeded(void) {
    UpdatePercentIconIfNeededInternal(FALSE, 0.0f, 0.0f);
}

void TrayAnimation_UpdatePercentIconWithMetrics(float cpuPercent, float memPercent) {
    UpdatePercentIconIfNeededInternal(TRUE, cpuPercent, memPercent);
}
