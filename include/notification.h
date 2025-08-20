/**
 * @file notification.h
 * @brief Notification display system
 * 
 * Functions for showing different types of notifications to users
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"

/** @brief Notification display timeout in milliseconds */
extern int NOTIFICATION_TIMEOUT_MS;

/**
 * @brief Show notification using configured display method
 * @param hwnd Parent window handle
 * @param message Message text to display
 */
void ShowNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show toast-style notification
 * @param hwnd Parent window handle
 * @param message Message text to display
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show modal notification dialog
 * @param hwnd Parent window handle
 * @param message Message text to display
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Close all active notifications
 */
void CloseAllNotifications(void);

#endif