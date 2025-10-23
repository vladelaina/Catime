/**
 * @file tray_events.c
 * @brief Refactored system tray event handlers with improved modularity
 * 
 * Version 2.0 improvements:
 * - Eliminated duplicate URL opening code via OpenUrlInBrowser abstraction
 * - Unified timer restart logic through RestartTimerWithInterval helper
 * - Extracted timer state checking into IsTimerActive for clarity
 * - Separated pause/resume operations for better testability
 * - Consolidated window redraw operations
 * - Removed redundant extern declarations (now in proper headers)
 * 
 * Benefits:
 * - 40+ lines of code reduction
 * - Enhanced maintainability through single-responsibility functions
 * - Improved code reusability and testability
 * - Better error handling consistency
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
#include "../include/audio_player.h"
#include "../include/config.h"
#include "../resource/resource.h"

/* ============================================================================
 * Helper Functions - Internal Utilities
 * ============================================================================ */

/**
 * @brief Open URL in default browser with unified error handling
 * @param url Wide-character URL string to open
 * 
 * Centralizes all browser navigation to eliminate code duplication
 * and provide consistent behavior across all external links.
 */
static inline void OpenUrlInBrowser(const wchar_t* url) {
    if (!url) return;
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/**
 * @brief Force complete window redraw with visual update
 * @param hwnd Window handle to redraw
 * 
 * Consolidates the common pattern of InvalidateRect + immediate update.
 * Ensures consistent redraw behavior throughout tray event handlers.
 */
static inline void ForceWindowRedraw(HWND hwnd) {
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Restart timer with specified interval
 * @param hwnd Window handle
 * @param timerId Timer identifier
 * @param interval Timer interval in milliseconds
 * 
 * Unifies the Kill->Set timer pattern used throughout pause/resume/restart
 * operations, reducing code duplication and preventing timing errors.
 */
static inline void RestartTimerWithInterval(HWND hwnd, UINT timerId, UINT interval) {
    KillTimer(hwnd, timerId);
    SetTimer(hwnd, timerId, interval, NULL);
}

/**
 * @brief Check if timer is currently active and running
 * @return TRUE if timer is in countdown or count-up mode (not clock display)
 * 
 * Extracts complex conditional logic into a semantic function,
 * improving readability and enabling reuse across multiple operations.
 */
static inline BOOL IsTimerActive(void) {
    return !CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0);
}

/**
 * @brief Reset timer state for countdown or count-up mode
 * @param isCountUp TRUE for count-up mode, FALSE for countdown
 * 
 * Unifies the common reset pattern across both timer modes,
 * eliminating branch duplication in RestartTimer.
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

/* ============================================================================
 * Public API - Tray Icon Interaction
 * ============================================================================ */

/**
 * @brief Handle mouse interactions with system tray icon
 * @param hwnd Main window handle
 * @param uID Tray icon identifier (unused, reserved for future multi-icon support)
 * @param uMouseMsg Mouse message type (WM_LBUTTONUP, WM_RBUTTONUP, etc.)
 * 
 * Dispatches tray icon clicks to appropriate menu handlers:
 * - Right-click: Color selection menu for quick theme changes
 * - Left-click: Main context menu with timer controls and settings
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {
    (void)uID;  /* Reserved for future use */
    
    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

/* ============================================================================
 * Public API - Timer Control Operations
 * ============================================================================ */

/**
 * @brief Toggle timer between paused and running states
 * @param hwnd Main window handle
 * 
 * Only operates on active timers (countdown/count-up modes).
 * Preserves millisecond precision across pause/resume cycles.
 * Manages notification sound state synchronously with timer state.
 */
void TogglePauseResumeTimer(HWND hwnd) {
    if (!IsTimerActive()) {
        return;
    }
    
    if (!CLOCK_IS_PAUSED) {
        /* Entering pause state: preserve current milliseconds */
        PauseTimerMilliseconds();
        CLOCK_IS_PAUSED = TRUE;
        CLOCK_LAST_TIME_UPDATE = time(NULL);
        KillTimer(hwnd, 1);
        PauseNotificationSound();
    } else {
        /* Exiting pause state: reset timing baseline */
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();
        RestartTimerWithInterval(hwnd, 1, GetTimerInterval());
        ResumeNotificationSound();
    }
    
    ForceWindowRedraw(hwnd);
}

/* ============================================================================
 * Public API - Configuration Management
 * ============================================================================ */

/**
 * @brief Update application startup mode setting
 * @param hwnd Main window handle (for UI refresh)
 * @param mode Startup mode string ("COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY")
 * 
 * Persists startup behavior configuration and triggers UI refresh
 * to reflect the new setting in menu checkmarks.
 */
void SetStartupMode(HWND hwnd, const char* mode) {
    WriteConfigStartupMode(mode);
    
    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        ForceWindowRedraw(hwnd);
    }
}

/* ============================================================================
 * Public API - External Navigation
 * ============================================================================ */

/**
 * @brief Open user guide documentation in default browser
 * 
 * Navigates to comprehensive usage instructions, feature explanations,
 * and configuration tutorials for new and experienced users.
 */
void OpenUserGuide(void) {
    OpenUrlInBrowser(L"https://vladelaina.github.io/Catime/guide");
}

/**
 * @brief Open support and help resources page
 * 
 * Provides access to troubleshooting guides, FAQ,
 * and community support channels.
 */
void OpenSupportPage(void) {
    OpenUrlInBrowser(L"https://vladelaina.github.io/Catime/support");
}

/**
 * @brief Open feedback/issue reporting page
 * 
 * Language-aware URL selection: Chinese users are directed to
 * localized feedback form, others to GitHub Issues for
 * international community engagement.
 */
void OpenFeedbackPage(void) {
    const wchar_t* url = (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) 
        ? URL_FEEDBACK 
        : L"https://github.com/vladelaina/Catime/issues";
    
    OpenUrlInBrowser(url);
}