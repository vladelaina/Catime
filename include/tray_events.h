/**
 * @file tray_events.h
 * @brief System tray event handling interface
 * 
 * This file defines the application's system tray event handling function interfaces,
 * including handling of tray icon click events and other functionalities.
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

/**
 * @brief Handle system tray messages
 * @param hwnd Window handle
 * @param uID Tray icon ID
 * @param uMouseMsg Mouse message type
 * 
 * Process system tray mouse events, including:
 * - Left click: Display context menu
 * - Right click: Display color menu
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

/**
 * @brief Pause or resume timer
 * @param hwnd Window handle
 * 
 * Pause or resume the timer based on current state, and update related status variables
 */
void PauseResumeTimer(HWND hwnd);

/**
 * @brief Restart timer
 * @param hwnd Window handle
 * 
 * Reset the timer to initial state and continue running
 */
void RestartTimer(HWND hwnd);

/**
 * @brief Set startup mode
 * @param hwnd Window handle
 * @param mode Startup mode string
 * 
 * Set the startup mode according to the given mode, and update related status variables
 */
void SetStartupMode(HWND hwnd, const char* mode);

/**
 * @brief Open user guide
 * 
 * Open the user guide, providing application usage instructions and help
 */
void OpenUserGuide(void);

/**
 * @brief Open support page
 * 
 * Open the support page, providing channels to support the developer
 */
void OpenSupportPage(void);

/**
 * @brief Open feedback page
 * 
 * Open different feedback channels based on current language settings:
 * - Simplified Chinese: Open bilibili private message page
 * - Other languages: Open GitHub Issues page
 */
void OpenFeedbackPage(void);

#endif // CLOCK_TRAY_EVENTS_H