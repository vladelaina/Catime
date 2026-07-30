/**
 * @file tray_events.h
 * @brief Tray event dispatcher and external navigation
 * 
 * Right-click opens color menu for quick theming (faster than nested navigation).
 * Left-click opens main context menu with all controls.
 * Language-aware feedback routing (Chinese → local form, others → GitHub Issues).
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

/* ============================================================================
 * Tray Icon Interaction
 * ============================================================================ */

/**
 * @brief Handle tray icon mouse events
 * @param hwnd Window handle
 * @param wParam Raw Shell callback wParam
 * @param lParam Raw Shell callback lParam
 * 
 * @details
 * - Primary activation: Main context menu
 * - Secondary activation: Color/configuration menu
 * - Supports VERSION_4 callbacks and legacy button-up fallback
 */
void HandleTrayIconMessage(HWND hwnd, WPARAM wParam, LPARAM lParam);

/**
 * @brief Run the tray left/right-click action for another tray surface
 * @return TRUE when the mouse message was recognized and handled
 */
BOOL HandleTrayMenuClick(HWND hwnd, UINT mouseMessage);

/**
 * @brief Stop tray hover detection timer
 * @note Called when tray icon is removed for cleanup
 */
void StopTrayHoverDetection(void);

/* ============================================================================
 * Timer Control Operations
 * ============================================================================ */

/**
 * @brief Toggle pause state
 * @param hwnd Window handle
 * 
 * @details
 * Preserves millisecond precision across cycles. Syncs sound state.
 * No-op in clock mode.
 */
void TogglePauseResumeTimer(HWND hwnd);


/* ============================================================================
 * Configuration Management
 * ============================================================================ */

/**
 * @brief Set startup mode and update UI
 * @param hwnd Window handle
 * @param mode "DEFAULT", "COUNT_UP", "SHOW_TIME", "NO_DISPLAY", or "POMODORO"
 * 
 * @details Persists to config and refreshes the UI state
 */
void SetStartupMode(HWND hwnd, const char* mode);

/* ============================================================================
 * External Navigation
 * ============================================================================ */

/**
 * @brief Open user guide in browser
 */
void OpenUserGuide(void);

/**
 * @brief Open support page in browser
 */
void OpenSupportPage(void);

/**
 * @brief Open Vlaina project page in browser
 */
void OpenVlainaPage(void);

/**
 * @brief Open feedback page (language-aware)
 * 
 * @details Chinese → local form, others → GitHub Issues
 */
void OpenFeedbackPage(void);

#endif /* CLOCK_TRAY_EVENTS_H */
