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
 * @param uID Icon ID (reserved for future multi-icon)
 * @param uMouseMsg Message type
 * 
 * @details
 * - WM_RBUTTONUP: Color menu (quick theme changes)
 * - WM_LBUTTONUP: Main context menu
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

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
 * @param mode "COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY"
 * 
 * @details Persists to config, updates menu checkmarks
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
 * @brief Open feedback page (language-aware)
 * 
 * @details Chinese → local form, others → GitHub Issues
 */
void OpenFeedbackPage(void);

#endif /* CLOCK_TRAY_EVENTS_H */