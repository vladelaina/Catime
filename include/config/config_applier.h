/**
 * @file config_applier.h
 * @brief Apply configuration snapshot to global variables
 * 
 * Separates configuration application from loading logic.
 * Allows independent testing of configuration effects.
 */

#ifndef CONFIG_APPLIER_H
#define CONFIG_APPLIER_H

#include <windows.h>
#include "config_loader.h"

/* ============================================================================
 * Global flags
 * ============================================================================ */

/**
 * @brief Force apply all config values, bypassing position preservation logic
 * 
 * When TRUE, ApplyDisplaySettings will apply window position from config
 * even if current window position differs significantly.
 * Used during reset operations to ensure defaults are applied.
 * Automatically reset to FALSE after ApplyConfigSnapshot completes.
 */
extern BOOL g_ForceApplyConfig;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Apply complete configuration snapshot to global variables
 * @param snapshot Configuration snapshot to apply
 * 
 * @details
 * Updates all global configuration variables with values from snapshot.
 * Triggers side effects like language switching, window repositioning, etc.
 * 
 * Order of application:
 * 1. Display settings (window position, scale, colors)
 * 2. Timer settings
 * 3. Notification settings
 * 4. Language (triggers UI update)
 * 5. Animation settings
 */
void ApplyConfigSnapshot(const ConfigSnapshot* snapshot);

/**
 * @brief Apply general settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: language, font license flags
 */
void ApplyGeneralSettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply display settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: colors, fonts, window position/scale, topmost flag
 * Repositions window if handle is available
 */
void ApplyDisplaySettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply timer settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: default start time, time format, milliseconds display,
 * timeout actions, quick countdown presets, startup mode
 */
void ApplyTimerSettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply Pomodoro settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: work time, break times, loop count, time intervals array
 */
void ApplyPomodoroSettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply notification settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: notification messages, timeout, opacity, type, sound settings
 */
void ApplyNotificationSettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply color palette settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: COLOR_OPTIONS global array
 * Frees and reallocates color options
 */
void ApplyColorSettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply hotkey settings only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: all 12 hotkey global variables
 * Note: Does not register/unregister system hotkeys (handled by hotkey module)
 */
void ApplyHotkeySettings(const ConfigSnapshot* snapshot);

/**
 * @brief Apply recent files list only
 * @param snapshot Configuration snapshot
 * 
 * @details
 * Updates: CLOCK_RECENT_FILES array and count
 * Updates tray menu if window handle is available
 */
void ApplyRecentFilesSettings(const ConfigSnapshot* snapshot);

#endif /* CONFIG_APPLIER_H */

