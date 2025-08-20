/**
 * @file tray_events.h
 * @brief System tray event handling
 * 
 * Functions for processing tray icon interactions and related actions
 */

#ifndef CLOCK_TRAY_EVENTS_H
#define CLOCK_TRAY_EVENTS_H

#include <windows.h>

/**
 * @brief Handle tray icon mouse messages
 * @param hwnd Main window handle
 * @param uID Tray icon identifier
 * @param uMouseMsg Mouse message type
 */
void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg);

/**
 * @brief Toggle timer pause/resume state
 * @param hwnd Main window handle
 */
void PauseResumeTimer(HWND hwnd);

/**
 * @brief Restart current timer from beginning
 * @param hwnd Main window handle
 */
void RestartTimer(HWND hwnd);

/**
 * @brief Set application startup mode
 * @param hwnd Main window handle
 * @param mode Startup mode string
 */
void SetStartupMode(HWND hwnd, const char* mode);

/**
 * @brief Open user guide in default browser
 */
void OpenUserGuide(void);

/**
 * @brief Open support page in default browser
 */
void OpenSupportPage(void);

/**
 * @brief Open feedback page in default browser
 */
void OpenFeedbackPage(void);

#endif