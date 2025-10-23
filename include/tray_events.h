/**
 * @file tray_events.h
 * @brief Refactored system tray event handling with improved modularity
 * 
 * Version 2.0 - Enhanced API design:
 * - Clearer function naming (TogglePauseResumeTimer)
 * - Comprehensive documentation with usage context
 * - Reduced coupling through proper header organization
 * - Better semantic clarity in function responsibilities
 * 
 * Provides centralized handling for:
 * - Tray icon mouse interactions (left/right click)
 * - Timer control operations (pause/resume)
 * - Configuration management (startup mode)
 * - External navigation (documentation, support, feedback)
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

/* ============================================================================
 * Tray Icon Interaction
 * ============================================================================ */

/**
 * @brief Handle mouse interactions with system tray icon
 * @param hwnd Main window handle
 * @param uID Tray icon identifier (reserved for future multi-icon support)
 * @param uMouseMsg Mouse message type (WM_LBUTTONUP, WM_RBUTTONUP, etc.)
 * 
 * @details Dispatches tray icon clicks to appropriate menu handlers:
 * - WM_RBUTTONUP: Opens color selection menu for quick theme changes
 * - WM_LBUTTONUP: Opens main context menu with timer controls and settings
 * 
 * @note Currently uses single icon; uID reserved for future expansion
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

/* ============================================================================
 * Timer Control Operations
 * ============================================================================ */

/**
 * @brief Toggle timer between paused and running states
 * @param hwnd Main window handle
 * 
 * @details Only affects active timers (countdown/count-up modes):
 * - Preserves millisecond precision across pause/resume cycles
 * - Synchronizes notification sound state with timer state
 * - Maintains visual consistency with automatic window redraw
 * - No-op when in clock display mode
 * 
 * @note Function renamed from PauseResumeTimer for clarity
 */
void TogglePauseResumeTimer(HWND hwnd);


/* ============================================================================
 * Configuration Management
 * ============================================================================ */

/**
 * @brief Update application startup mode configuration
 * @param hwnd Main window handle (for UI refresh)
 * @param mode Startup mode string (e.g., "COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY")
 * 
 * @details Persists startup behavior to configuration file and triggers
 * UI refresh to update menu checkmarks. Mode determines initial timer
 * state when application launches.
 */
void SetStartupMode(HWND hwnd, const char* mode);

/* ============================================================================
 * External Navigation
 * ============================================================================ */

/**
 * @brief Open user guide documentation in default browser
 * 
 * @details Navigates to comprehensive online documentation including:
 * - Feature usage instructions
 * - Configuration tutorials
 * - Keyboard shortcuts reference
 * - Tips and best practices
 */
void OpenUserGuide(void);

/**
 * @brief Open support and help resources page
 * 
 * @details Provides access to:
 * - Troubleshooting guides
 * - Frequently asked questions (FAQ)
 * - Community support channels
 * - Known issues and workarounds
 */
void OpenSupportPage(void);

/**
 * @brief Open feedback/issue reporting page
 * 
 * @details Language-aware navigation:
 * - Chinese users: Directed to localized feedback form
 * - Other users: Directed to GitHub Issues for international community
 * 
 * Enables users to report bugs, request features, and provide suggestions.
 */
void OpenFeedbackPage(void);

#endif /* CLOCK_TRAY_EVENTS_H */