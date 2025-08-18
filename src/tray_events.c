#include <windows.h>
#include <shellapi.h>
#include "../include/tray_events.h"
#include "../include/tray_menu.h"
#include "../include/color.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/window_events.h"
#include "../resource/resource.h"

// Declaration of function to read timeout action from configuration file
extern void ReadTimeoutActionFromConfig(void);

void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    // Set default cursor to prevent wait cursor display
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

void PauseResumeTimer(HWND hwnd) {
    // Check if there is an active timer
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        
        // Toggle pause/resume state
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {
            // If paused, record current time point
            CLOCK_LAST_TIME_UPDATE = time(NULL);
            // Stop timer
            KillTimer(hwnd, 1);
            
            // Pause playing notification audio (new addition)
            extern BOOL PauseNotificationSound(void);
            PauseNotificationSound();
        } else {
            // If resumed, restart timer
            SetTimer(hwnd, 1, 1000, NULL);
            
            // Resume notification audio (new addition)
            extern BOOL ResumeNotificationSound(void);
            ResumeNotificationSound();
        }
        
        // Update window to reflect new state
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void RestartTimer(HWND hwnd) {
    // Stop any notification audio that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Determine operation based on current mode
    if (!CLOCK_COUNT_UP) {
        // Countdown mode
        if (CLOCK_TOTAL_TIME > 0) {
            countdown_elapsed_time = 0;
            countdown_message_shown = FALSE;
            CLOCK_IS_PAUSED = FALSE;
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
        }
    } else {
        // Count-up mode
        countup_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, 1000, NULL);
    }
    
    // Update window
    InvalidateRect(hwnd, NULL, TRUE);
    
    // Ensure window is on top and visible
    HandleWindowReset(hwnd);
}

void SetStartupMode(HWND hwnd, const char* mode) {
    // Save startup mode to configuration file
    WriteConfigStartupMode(mode);
    
    // Update menu item checked state
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void OpenUserGuide(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/guide", NULL, NULL, SW_SHOWNORMAL);
}

void OpenSupportPage(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support", NULL, NULL, SW_SHOWNORMAL);
}

void OpenFeedbackPage(void) {
    extern AppLanguage CURRENT_LANGUAGE; // Declare external variable
    
    // Choose different feedback links based on language
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) {
        // Simplified Chinese users open bilibili private message
        ShellExecuteW(NULL, L"open", URL_FEEDBACK, NULL, NULL, SW_SHOWNORMAL);
    } else {
        // Users of other languages open GitHub Issues
        ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime/issues", NULL, NULL, SW_SHOWNORMAL);
    }
}