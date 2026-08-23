/**
 * @file tray_animation_percent_update.c
 * @brief Dynamic percent and Caps Lock icon refresh.
 */

#include "tray_animation_core_internal.h"
#include "tray_internal.h"

BOOL UpdatePercentIconIfNeededInternal(
    const SystemMonitorSnapshot* snapshot,
    const wchar_t* synchronizedTooltip) {
    BOOL tooltipApplied = FALSE;
    if (!BeginTrayAnimationRuntimeUse()) return FALSE;

    HWND trayHwnd = GetValidTrayAnimationWindow();
    if (!trayHwnd) goto done;

    TrayPresentationUpdateMode updateMode = TrayUpdatePolicy_Select(
        IsTrayTooltipActive(), synchronizedTooltip != NULL);
    if (updateMode == TRAY_UPDATE_DEFER) goto done;
    if (updateMode == TRAY_UPDATE_TOOLTIP_ONLY) {
        UpdateTrayTooltip(synchronizedTooltip);
        tooltipApplied = TRUE;
        goto done;
    }

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
        if (snapshot && _stricmp(animationName, "__cpu__") == 0) {
            if (!snapshot->cpuAvailable) goto done;
            p = (int)(snapshot->cpuPercent + 0.5f);
        } else if (snapshot && _stricmp(animationName, "__mem__") == 0) {
            if (!snapshot->memoryAvailable) goto done;
            p = (int)(snapshot->memoryPercent + 0.5f);
        } else if (def->getValue) {
            p = def->getValue();
        }
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        value = p;
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

    NOTIFYICONDATAW updateData = {0};
    updateData.cbSize = sizeof(updateData);
    updateData.hWnd = trayHwnd;
    updateData.uID = CLOCK_ID_TRAY_APP_ICON;
    updateData.uFlags = NIF_ICON;
    updateData.hIcon = hIcon;
    if (updateMode == TRAY_UPDATE_ICON_AND_TOOLTIP) {
        updateData.uFlags |= NIF_TIP;
        wcsncpy_s(updateData.szTip, _countof(updateData.szTip),
                  synchronizedTooltip, _TRUNCATE);
    }
    if (Shell_NotifyIconW(NIM_MODIFY, &updateData)) {
        ReportTrayIconModifySuccess(trayHwnd);
        TrayIconLifetime_Retain(hIcon);
        hIcon = NULL;
        RecordBuiltinIconUpdateCache(animationName, value,
                                     textColor, bgColor,
                                     iconCx, iconCy);
        if (updateMode == TRAY_UPDATE_ICON_AND_TOOLTIP) {
            wcscpy_s(g_lastTrayTooltip, _countof(g_lastTrayTooltip),
                     updateData.szTip);
            tooltipApplied = TRUE;
        }
    } else {
        ReportTrayIconModifyFailure(trayHwnd);
    }

    if (hIcon) DestroyIcon(hIcon);

done:
    EndTrayAnimationRuntimeUse();
    return tooltipApplied;
}

BOOL TrayAnimation_UpdatePercentIconWithSnapshot(
    const SystemMonitorSnapshot* snapshot,
    const wchar_t* synchronizedTooltip) {
    return UpdatePercentIconIfNeededInternal(
        snapshot, synchronizedTooltip);
}
