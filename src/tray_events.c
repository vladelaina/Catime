/**
 * @file tray_events.c
 * @brief Implementation of system tray event handling module
 * 
 * This module implements the event handling functionality for the application's system tray,
 * including response to various mouse events on the tray icon, menu display and control,
 * as well as tray operations related to the timer such as pause/resume and restart.
 * It provides core functionality for users to quickly control the application through the tray icon.
 */

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

/**
 * @brief Handle system tray messages
 * @param hwnd Window handle
 * @param uID Tray icon ID
 * @param uMouseMsg Mouse message type
 * 
 * Process mouse events for the system tray, displaying different context menus based on the event type:
 * - Left click: Display main function context menu, including timer control functions
 * - Right click: Display color selection menu for quickly changing display color
 * 
 * All menu item command processing is implemented in the corresponding menu display module.
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    // Set default cursor to prevent wait cursor display
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

/**
 * @brief Pause or resume timer
 * @param hwnd Window handle
 * 
 * Toggle timer pause/resume state based on current status:
 * 1. Check if there is an active timer (countdown or count-up)
 * 2. If timer is active, toggle pause/resume state
 * 3. When paused, record current time point and stop timer
 * 4. When resumed, restart timer
 * 5. Refresh window to reflect new state
 * 
 * Note: Can only operate when displaying timer (not current time) and timer is active
 */
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

/**
 * @brief Restart timer
 * @param hwnd Window handle
 * 
 * Reset timer to initial state and continue running, keeping current timer type unchanged:
 * 1. Read current timeout action settings
 * 2. Reset timer progress based on current mode (countdown/count-up)
 * 3. Reset all related timer state variables
 * 4. Cancel pause state, ensure timer is running
 * 5. Refresh window and ensure window is on top after reset
 * 
 * This operation does not change the timer mode or total duration, it only resets progress to initial state.
 */
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

/**
 * @brief Set startup mode
 * @param hwnd Window handle
 * @param mode Startup mode ("COUNTDOWN"/"COUNT_UP"/"SHOW_TIME"/"NO_DISPLAY")
 * 
 * Set the application's default startup mode and save it to the configuration file:
 * 1. Save the selected mode to the configuration file
 * 2. Update menu item checked state to reflect current setting
 * 3. Refresh window display
 * 
 * The startup mode determines the default behavior of the application at startup, such as whether to display current time or start timing.
 * The setting will take effect the next time the program starts.
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    // Save startup mode to configuration file
    WriteConfigStartupMode(mode);
    
    // Update menu item checked state
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Open user guide webpage
 * 
 * Use ShellExecute to open Catime's user guide webpage,
 * providing detailed software instructions and help documentation for users.
 * URL: https://vladelaina.github.io/Catime/guide
 */
void OpenUserGuide(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/guide", NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Open support page
 * 
 * Use ShellExecute to open Catime's support page,
 * providing channels for users to support the developer.
 * URL: https://vladelaina.github.io/Catime/support
 */
void OpenSupportPage(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support", NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Open feedback page
 * 
 * Open different feedback channels based on current language setting:
 * - Simplified Chinese: Open bilibili private message page
 * - Other languages: Open GitHub Issues page
 */
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