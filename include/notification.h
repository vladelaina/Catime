/**
 * @file notification.h
 * @brief Multi-modal notification system with animations and fallback mechanisms
 * @version 2.0 - Enhanced with better type safety and modular design
 * 
 * Provides three notification display modes:
 * 1. Toast notifications - Custom animated overlays with transparency
 * 2. Modal notifications - System message boxes in background threads
 * 3. OS tray notifications - System tray balloon tips
 * 
 * Features:
 * - Automatic fallback chain for failure resilience
 * - Smooth fade-in/fade-out animations for toast notifications
 * - Click-to-dismiss support
 * - Dynamic width calculation based on message content
 * - Type-safe window data management
 * - Comprehensive resource cleanup
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <windows.h>
#include "config.h"

/* ============================================================================
 * Animation States
 * ============================================================================ */

/**
 * @brief Animation states for toast notification fade effects
 * Controls the lifecycle of notification window transparency animations
 */
typedef enum {
    ANIM_FADE_IN,    /**< Fading in from transparent to visible */
    ANIM_VISIBLE,    /**< Fully visible and stable (no animation) */
    ANIM_FADE_OUT,   /**< Fading out from visible to transparent */
} AnimationState;

/* ============================================================================
 * External Configuration Variables
 * ============================================================================ */

/** @brief Notification display timeout in milliseconds */
extern int NOTIFICATION_TIMEOUT_MS;

/* ============================================================================
 * Public API Functions
 * ============================================================================ */

/**
 * @brief Show notification using configured display method
 * @param hwnd Parent window handle
 * @param message Wide-char message text to display
 * 
 * Automatically selects notification type based on user configuration:
 * - NOTIFICATION_TYPE_CATIME: Toast notification (default)
 * - NOTIFICATION_TYPE_SYSTEM_MODAL: Modal message box
 * - NOTIFICATION_TYPE_OS: System tray notification
 * 
 * Respects NOTIFICATION_DISABLED and NOTIFICATION_TIMEOUT_MS settings.
 * Falls back to tray notification on any failure.
 * 
 * @note This is the primary entry point for showing notifications
 * @see ShowToastNotification, ShowModalNotification
 */
void ShowNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show toast-style notification with animation
 * @param hwnd Parent window handle
 * @param message Wide-char message text to display
 * 
 * Creates a custom animated notification window with:
 * - Dynamic width based on message length
 * - Smooth fade-in/fade-out transitions
 * - Auto-dismiss after configured timeout
 * - Click-to-dismiss support
 * - Bottom-right screen positioning
 * 
 * Falls back to tray notification if window creation fails.
 * 
 * @note Uses layered windows for transparency effects
 * @note Execution time: ~3-5 seconds (including animation and display)
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Show modal notification dialog in background thread
 * @param hwnd Parent window handle
 * @param message Wide-char message text to display
 * 
 * Creates a standard Windows message box in a separate thread to avoid
 * blocking the main UI. Falls back to tray notification if thread creation
 * or memory allocation fails.
 * 
 * @note Non-blocking on main thread
 * @note Thread is detached and cleans up automatically
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message);

/**
 * @brief Force close all active toast notifications
 * 
 * Enumerates all notification windows and triggers fade-out animations.
 * Windows that are already fading out are not affected.
 * 
 * Used when:
 * - Application is shutting down
 * - User explicitly closes all notifications
 * - Configuration changes require notification refresh
 * 
 * @note Only affects toast notifications, not modal or tray notifications
 * @note Windows will fade out gracefully rather than closing immediately
 */
void CloseAllNotifications(void);

#endif /* NOTIFICATION_H */