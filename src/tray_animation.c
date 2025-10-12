/**
 * @file tray_animation.c
 * @brief RunCat-like tray icon animation implementation
 */

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "../include/tray.h"
#include "../include/config.h"

/**
 * @brief Timer ID for tray animation
 */
#define TRAY_ANIM_TIMER_ID 42420

/** @brief Max frames supported */
#define MAX_TRAY_FRAMES 64

/** @brief Loaded icon frames and state */
static HICON g_trayIcons[MAX_TRAY_FRAMES];
static int g_trayIconCount = 0;
static int g_trayIconIndex = 0;
static UINT g_trayInterval = 0;
static HWND g_trayHwnd = NULL;

/** @brief Build cat animation folder path: %LOCALAPPDATA%\Catime\resources\animations\cat */
static void GetCatAnimationFolder(char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%scat", base);
    } else {
        snprintf(path, size, "%s\\cat", base);
    }
}

/** @brief Free all loaded icon frames */
static void FreeTrayIcons(void) {
    for (int i = 0; i < g_trayIconCount; ++i) {
        if (g_trayIcons[i]) {
            DestroyIcon(g_trayIcons[i]);
            g_trayIcons[i] = NULL;
        }
    }
    g_trayIconCount = 0;
    g_trayIconIndex = 0;
}

/** @brief Load sequential .ico files starting from 1.ico upward */
static void LoadTrayIcons(void) {
    FreeTrayIcons();

    char folder[MAX_PATH] = {0};
    GetCatAnimationFolder(folder, sizeof(folder));

    for (int i = 1; i <= MAX_TRAY_FRAMES; ++i) {
        char icoPath[MAX_PATH] = {0};
        snprintf(icoPath, sizeof(icoPath), "%s\\%d.ico", folder, i);

        wchar_t wPath[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, icoPath, -1, wPath, MAX_PATH);

        HICON hIcon = (HICON)LoadImageW(NULL, wPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        if (!hIcon) {
            /** Stop at first missing index to allow 1..N contiguous frames */
            break;
        }
        g_trayIcons[g_trayIconCount++] = hIcon;
    }
}

/** @brief Advance to next icon frame and apply to tray */
static void AdvanceTrayFrame(void) {
    if (!g_trayHwnd || g_trayIconCount <= 0) return;
    if (g_trayIconIndex >= g_trayIconCount) g_trayIconIndex = 0;

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_trayIcons[g_trayIconIndex];

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    g_trayIconIndex = (g_trayIconIndex + 1) % g_trayIconCount;
}

/** @brief Window-proc level timer callback shim */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;
    AdvanceTrayFrame();
}

void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_trayInterval = intervalMs > 0 ? intervalMs : 150; /** default ~6-7 fps */

    LoadTrayIcons();

    if (g_trayIconCount > 0) {
        AdvanceTrayFrame();
        SetTimer(hwnd, TRAY_ANIM_TIMER_ID, g_trayInterval, (TIMERPROC)TrayAnimTimerProc);
    }
}

void StopTrayAnimation(HWND hwnd) {
    KillTimer(hwnd, TRAY_ANIM_TIMER_ID);
    FreeTrayIcons();
    g_trayHwnd = NULL;
}


