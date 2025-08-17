/**
 * @file notification.h
 * @brief Application notification system interface
 * 
 * This file defines the application's notification system interface, including custom styled popup notifications and system tray notifications.
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"  // Import notification type enumeration

// Global variable: notification display duration (milliseconds)
extern int NOTIFICATION_TIMEOUT_MS;  // New: notification display time variable

/**
 * @brief Show notification (based on configured notification type)
 * @param hwnd Parent window handle, used to get application instance and calculate position
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays different styles of notifications based on the configured notification type:
 * - NOTIFICATION_TYPE_CATIME: Uses Catime custom notification window
 * - NOTIFICATION_TYPE_SYSTEM_MODAL: Uses system modal dialog
 * - NOTIFICATION_TYPE_OS: Uses system tray notification
 */
void ShowNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Display custom styled toast notification
 * @param hwnd Parent window handle, used to get application instance and calculate position
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays a custom notification window with animation effects in the bottom right corner of the screen
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Display system modal dialog notification
 * @param hwnd Parent window handle
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays a blocking system message box notification
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Close all currently displayed Catime notification windows
 * 
 * Find and close all notification windows created by Catime, ignoring their current display time settings,
 * directly start fade-out animation. Usually called when switching timer modes to ensure notifications don't continue to display.
 */
void CloseAllNotifications(void);

// System tray notification is already defined in tray.h: void ShowTrayNotification(HWND hwnd, const char* message);

#endif // NOTIFICATION_H