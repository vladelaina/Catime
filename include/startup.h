/**
 * @file startup.h
 * @brief Windows startup integration and launch mode config
 * 
 * Shortcut management uses COM IShellLink for reliability.
 * --startup argument in shortcut enables different launch behavior (e.g., hidden mode).
 * Data-driven mode switching simplifies adding new startup behaviors.
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <windows.h>
#include <shlobj.h>

typedef enum {
    AUTO_START_STATUS_ABSENT = 0,
    AUTO_START_STATUS_ENABLED,
    AUTO_START_STATUS_DISABLED_BY_WINDOWS,
    AUTO_START_STATUS_BROKEN
} AutoStartStatus;

/** @brief Return the effective Windows startup state. */
AutoStartStatus GetAutoStartStatus(void);

/**
 * @brief Check if auto-start enabled
 * @return TRUE only when the shortcut is valid and Windows has not disabled it
 */
BOOL IsAutoStartEnabled(void);

/**
 * @brief Create startup shortcut with --startup argument
 * @return TRUE on success
 * 
 * @details
 * Points to a stable executable path with --startup arg. Scoop installs use
 * the apps\\catime\\current path so the shortcut survives package upgrades.
 * Handles COM initialization internally and preserves a Windows-level opt-out.
 */
BOOL CreateShortcut(void);

/** @brief Explicitly enable startup, clearing a stale Windows disable record. */
BOOL EnableAutoStart(void);

/** @brief Explicitly disable startup and persist the choice before removal. */
BOOL DisableAutoStart(void);

/**
 * @brief Remove startup shortcut (idempotent)
 * @return TRUE on success or if doesn't exist
 * 
 * @note Returns TRUE for ERROR_FILE_NOT_FOUND
 */
BOOL RemoveShortcut(void);

/**
 * @brief Repair an existing startup shortcut before single-instance routing
 * @return TRUE when no repair is needed or the repair succeeded
 *
 * @details This lets a newly launched version replace a stale old-version
 * target even while the old process still owns the single-instance mutex.
 * A legacy install with no shortcut is recorded as disabled; only an explicit
 * enabled preference may recreate a previously absent shortcut here.
 */
BOOL RepairExistingAutoStartShortcut(void);

/**
 * @brief Apply the default/migrated auto-start preference and repair registration
 * @return TRUE when the configured state is valid
 *
 * @details New installs default to enabled. Existing installs adopt the prior
 * shortcut state, preserving an explicit opt-out. Enabled shortcuts are
 * validated against target, arguments, and working directory before reuse.
 */
BOOL EnsureAutoStart(void);

/** @brief Compatibility wrapper for EnsureAutoStart */
BOOL UpdateStartupShortcut(void);

/**
 * @brief Apply configured startup mode from config
 * @param hwnd Window handle
 * 
 * @details Modes:
 * - COUNT_UP: Timer counts up from zero
 * - SHOW_TIME: Display current system time
 * - NO_DISPLAY: Hidden mode
 * - DEFAULT: Countdown with default time
 * 
 * Falls back to DEFAULT if config missing/invalid.
 * 
 * @note Call during init after window creation
 */
void ApplyStartupMode(HWND hwnd);

/** @brief Open the Windows Startup Apps settings page. */
BOOL OpenStartupSettings(void);

/** @brief Compatibility wrapper for OpenStartupSettings. */
BOOL OpenPackagedStartupSettings(void);

#endif /* STARTUP_H */
