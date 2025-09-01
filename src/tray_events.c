/**
 * @file tray_events.c
 * @brief System tray icon event handlers and user interaction processing
 * Manages mouse clicks, timer controls, and external page navigation
 */
#include <windows.h>
#include <shellapi.h>
#include "../include/tray_events.h"
#include "../include/tray_menu.h"
#include "../include/color.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/window_events.h"
#include "../include/timer_events.h"
#include "../include/drawing.h"
#include "../resource/resource.h"

extern void ReadTimeoutActionFromConfig(void);
extern UINT GetTimerInterval(void);

/**
 * @brief Handle mouse interactions with system tray icon
 * @param hwnd Main window handle
 * @param uID Tray icon identifier (unused)
 * @param uMouseMsg Mouse message type
 * Right-click shows color menu, left-click shows context menu
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

/**
 * @brief Toggle timer pause/resume state with sound and visual updates
 * @param hwnd Main window handle for timer management
 * Only operates on active timers (not system clock mode)
 */
void PauseResumeTimer(HWND hwnd) {
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        if (!CLOCK_IS_PAUSED) {
            /** About to pause: save current milliseconds first */
            PauseTimerMilliseconds();
        }
        
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {
            /** Record pause timestamp and stop updates */
            CLOCK_LAST_TIME_UPDATE = time(NULL);
            KillTimer(hwnd, 1);
            
            extern BOOL PauseNotificationSound(void);
            PauseNotificationSound();
        } else {
            /** Resume timer updates and notification sounds */
            ResetMillisecondAccumulator();  /** Reset millisecond timing on resume */
            SetTimer(hwnd, 1, GetTimerInterval(), NULL);
            
            extern BOOL ResumeNotificationSound(void);
            ResumeNotificationSound();
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Reset timer to initial state and restart countdown/countup
 * @param hwnd Main window handle for timer operations
 * Stops all sounds, clears elapsed time, and restarts timer updates
 */
void RestartTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    if (!CLOCK_COUNT_UP) {
        /** Reset countdown timer if valid duration is set */
        if (CLOCK_TOTAL_TIME > 0) {
            countdown_elapsed_time = 0;
            countdown_message_shown = FALSE;
            CLOCK_IS_PAUSED = FALSE;
            ResetMillisecondAccumulator();  /** Reset millisecond timing on restart */
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, GetTimerInterval(), NULL);
        }
    } else {
        /** Reset countup timer (always valid) */
        countup_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on restart */
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, GetTimerInterval(), NULL);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
    
    HandleWindowReset(hwnd);
}

/**
 * @brief Update startup mode configuration setting
 * @param hwnd Main window handle (for menu updates)
 * @param mode Startup mode string to save in configuration
 * Persists setting and triggers menu refresh if available
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    WriteConfigStartupMode(mode);
    
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Open user guide webpage in default browser
 * Navigates to project documentation for application usage instructions
 */
void OpenUserGuide(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/guide", NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Open support page in default browser
 * Provides access to help resources and troubleshooting information
 */
void OpenSupportPage(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support", NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Open feedback/issues page based on user language preference
 * Chinese users get localized feedback URL, others get GitHub issues
 */
void OpenFeedbackPage(void) {
    extern AppLanguage CURRENT_LANGUAGE;
    
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) {
        ShellExecuteW(NULL, L"open", URL_FEEDBACK, NULL, NULL, SW_SHOWNORMAL);
    } else {
        ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime/issues", NULL, NULL, SW_SHOWNORMAL);
    }
}