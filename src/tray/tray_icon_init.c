/**
 * @file tray_icon_init.c
 * @brief Initial tray icon selection, registration, and setup.
 */

#include "tray_internal.h"
#include "language.h"
#include "config.h"
#include "config/config_defaults.h"
#include "system_monitor.h"
#include "tray/tray_animation_core.h"
#include "tray/tray_animation_loader.h"
#include "tray/tray_animation_percent.h"
#include "tray/tray_icon_lifetime.h"
#include "log.h"
#include "../../resource/resource.h"
#include <shellapi.h>
#include <string.h>

extern void ReadPercentIconColorsConfig(void);

void RegisterTaskbarCreatedMessage(void) {
    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
}

static HICON GetInitialPercentIcon(AnimationType type) {
    if (!IsPercentIcon(type)) {
        return NULL;
    }

    int percent = 0;
    if (type == ANIM_TYPE_BATTERY) {
        int batteryPercent = 0;
        if (SystemMonitor_GetBatteryPercent(&batteryPercent)) {
            percent = batteryPercent;
        }
    } else {
        SystemMonitorSnapshot snapshot = {0};
        EnsureTraySystemMonitorActive();
        if (!SystemMonitor_GetSnapshot(
                SYSTEM_MONITOR_SNAPSHOT_CPU_MEMORY, &snapshot)) {
            return NULL;
        }
        if (type == ANIM_TYPE_CPU) {
            if (!snapshot.cpuAvailable) return NULL;
            percent = (int)(snapshot.cpuPercent + 0.5f);
        } else {
            if (!snapshot.memoryAvailable) return NULL;
            percent = (int)(snapshot.memoryPercent + 0.5f);
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

void InitTrayIconInternal(HWND hwnd, HINSTANCE hInstance,
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
        hInitial = GetInitialDynamicBuiltinIcon(GetAnimationType(animName));
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
    g_trayCallbackVersion = 0;
    nid.cbSize = sizeof(nid);
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    nid.hIcon = hInitial ? hInitial
                         : LoadIconW(hInstance,
                                     MAKEINTRESOURCEW(IDI_CATIME));
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
        nid.uVersion = NOTIFYICON_VERSION_4;
        if (Shell_NotifyIconW(NIM_SETVERSION, &nid)) {
            g_trayCallbackVersion = NOTIFYICON_VERSION_4;
        } else {
            nid.uVersion = 0;
            LOG_WARNING("Tray callback version 4 unavailable; using legacy events");
        }
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
        nid.uFlags = NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    }

    if (WM_TASKBARCREATED == 0) {
        RegisterTaskbarCreatedMessage();
    }
    RefreshTrayBackgroundWorkState();
}

void InitTrayIcon(HWND hwnd, HINSTANCE hInstance) {
    g_trayShuttingDown = FALSE;
    InitTrayIconInternal(hwnd, hInstance, TRUE, TRUE, TRUE);
}
