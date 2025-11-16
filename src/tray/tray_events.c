/**
 * @file tray_events.c
 * @brief System tray event handlers and timer control
 */
#include <windows.h>
#include <shellapi.h>
#include "tray/tray_events.h"
#include "tray/tray_menu.h"
#include "color/color.h"
#include "timer/timer.h"
#include "language.h"
#include "window_procedure/window_events.h"
#include "timer/timer_events.h"
#include "drawing.h"
#include "audio_player.h"
#include "config.h"
#include "../resource/resource.h"

/**
 * @brief Open URL in default browser
 * @param url Wide-character URL string
 */
static inline void OpenUrlInBrowser(const wchar_t* url) {
    if (!url) return;
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Invalidate window to trigger redraw
 * @param hwnd Window handle
 */
static inline void ForceWindowRedraw(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Restart timer with new interval
 * @param hwnd Window handle
 * @param timerId Timer identifier
 * @param interval Interval in milliseconds
 */
static inline void RestartTimerWithInterval(HWND hwnd, UINT timerId, UINT interval) {
    KillTimer(hwnd, timerId);
    SetTimer(hwnd, timerId, interval, NULL);
}

/**
 * @brief Check if timer is active (countdown or count-up)
 * @return TRUE if timer is running (not clock display mode)
 */
static inline BOOL IsTimerActive(void) {
    return !CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0);
}

/**
 * @brief Reset timer state and clear pause flag
 * @param isCountUp TRUE for count-up mode, FALSE for countdown
 */
static void ResetTimerState(BOOL isCountUp) {
    if (isCountUp) {
        countup_elapsed_time = 0;
    } else {
        if (CLOCK_TOTAL_TIME > 0) {
            countdown_elapsed_time = 0;
            countdown_message_shown = FALSE;
        }
    }
    CLOCK_IS_PAUSED = FALSE;
}

/**
 * @brief Handle tray icon mouse events
 * @param hwnd Main window handle
 * @param uID Tray icon identifier
 * @param uMouseMsg Mouse message (WM_LBUTTONUP, WM_RBUTTONUP, WM_MOUSEWHEEL, etc.)
 * @note Right-click: color menu; Left-click: main context menu; Mouse wheel: adjust window opacity
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    (void)uID;

    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));

    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

/**
 * @brief Toggle timer pause/resume state
 * @param hwnd Main window handle for timer operations
 * @note Preserves milliseconds and manages notification sound state
 */
void TogglePauseResumeTimer(HWND hwnd) {
    if (!IsTimerActive()) {
        return;
    }
    
    if (!CLOCK_IS_PAUSED) {
        PauseTimerMilliseconds();
        CLOCK_IS_PAUSED = TRUE;
        CLOCK_LAST_TIME_UPDATE = time(NULL);
        KillTimer(hwnd, 1);
        PauseNotificationSound();
    } else {
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();
        RestartTimerWithInterval(hwnd, 1, GetTimerInterval());
        ResumeNotificationSound();
    }
    
    ForceWindowRedraw(hwnd);
}

/**
 * @brief Persist startup mode and refresh UI
 * @param hwnd Window handle for redraw
 * @param mode Startup mode ("COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY")
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    WriteConfigStartupMode(mode);
    
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        ForceWindowRedraw(hwnd);
    }
}

/** @brief Open user guide in browser */
void OpenUserGuide(void) {
    OpenUrlInBrowser(L"https://vladelaina.github.io/Catime/guide");
}

/** @brief Open support page in browser */
void OpenSupportPage(void) {
    OpenUrlInBrowser(L"https://vladelaina.github.io/Catime/support");
}

/**
 * @brief Open feedback page (language-aware)
 * @note Chinese users → localized form; others → GitHub Issues
 */
void OpenFeedbackPage(void) {
    const wchar_t* url = (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) 
        ? URL_FEEDBACK 
        : L"https://github.com/vladelaina/Catime/issues";
    
    OpenUrlInBrowser(url);
}