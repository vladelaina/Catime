/**
 * @file config.h
 * @brief Comprehensive configuration management system with atomic updates
 * @version 2.0 - Refactored for better maintainability and thread safety
 * 
 * Advanced INI-based configuration system with:
 * - Thread-safe atomic writes (mutex-protected)
 * - Unicode (UTF-8) path support
 * - System language auto-detection
 * - Enum-to-string mapping for type safety
 * - Recent files MRU (Most Recently Used) list
 * - Hotkey configuration with human-readable strings
 * - Animation speed metrics (memory, CPU, timer-based)
 * - Font license version tracking
 * - First-run detection and setup
 * 
 * Architecture:
 * - Single config.ini file in %APPDATA%\Catime
 * - Atomic updates via temp file + rename
 * - Named mutex for process synchronization
 * - UTF-8 encoding throughout
 * - Resource folder auto-creation
 * 
 * Features:
 * - Live config reloading (config watcher monitors file changes)
 * - Default value fallbacks for missing keys
 * - Validation and sanitization of all inputs
 * - Automatic migration for version upgrades
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <time.h>
#include "../resource/resource.h"
#include "language.h"
#include "font.h"
#include "color.h"
#include "tray.h"
#include "tray_menu.h"
#include "timer.h"
#include "window.h"
#include "startup.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum number of recent files to remember in MRU list */
#define MAX_RECENT_FILES 5

/** @brief INI file section names - use these for all config operations */
#define INI_SECTION_GENERAL       "General"      /**< General app settings */
#define INI_SECTION_DISPLAY       "Display"      /**< Window and visual options */
#define INI_SECTION_TIMER         "Timer"        /**< Timer behavior and defaults */
#define INI_SECTION_POMODORO      "Pomodoro"     /**< Pomodoro technique settings */
#define INI_SECTION_NOTIFICATION  "Notification" /**< Notification appearance and sound */
#define INI_SECTION_HOTKEYS       "Hotkeys"      /**< Global hotkey assignments */
#define INI_SECTION_RECENTFILES   "RecentFiles"  /**< Recent file paths (MRU) */
#define INI_SECTION_COLORS        "Colors"       /**< User color palette */
#define INI_SECTION_OPTIONS       "Options"      /**< Miscellaneous options */

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/**
 * @brief Recent file information structure for MRU list
 */
typedef struct {
    char path[MAX_PATH];  /**< Full file path (UTF-8) */
    char name[MAX_PATH];  /**< Display name (filename only) */
} RecentFile;

/**
 * @brief Notification display types
 */
typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,     /**< Custom Catime notification window */
    NOTIFICATION_TYPE_SYSTEM_MODAL,   /**< System modal MessageBox dialog */
    NOTIFICATION_TYPE_OS              /**< OS native notification (Windows 10+ toast) */
} NotificationType;

/**
 * @brief Animation speed metric selector for tray icon animations
 */
typedef enum {
    ANIMATION_SPEED_MEMORY = 0,  /**< Speed based on memory usage percentage */
    ANIMATION_SPEED_CPU = 1,     /**< Speed based on CPU usage percentage */
    ANIMATION_SPEED_TIMER = 2    /**< Speed based on timer progress (countdown/count-up) */
} AnimationSpeedMetric;

/**
 * @brief Time display format types
 */
typedef enum {
    TIME_FORMAT_DEFAULT = 0,        /**< Default format: 9:59, 9 (no leading zeros) */
    TIME_FORMAT_ZERO_PADDED = 1,    /**< Zero-padded format: 09:59, 09 (minutes padded) */
    TIME_FORMAT_FULL_PADDED = 2     /**< Full zero-padded format: 00:09:59, 00:00:09 (all padded) */
} TimeFormatType;

/* ============================================================================
 * Global state variables
 * ============================================================================ */

/** @brief Array of recently used files (MRU order) */
extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];

/** @brief Count of recent files in list */
extern int CLOCK_RECENT_FILES_COUNT;

/** @brief Default countdown start time in seconds */
extern int CLOCK_DEFAULT_START_TIME;

/** @brief Last configuration file modification time (for live reload) */
extern time_t last_config_time;

/* Pomodoro technique settings */
/** @brief Pomodoro work phase duration in seconds (default: 1500 = 25 min) */
extern int POMODORO_WORK_TIME;

/** @brief Pomodoro short break duration in seconds (default: 300 = 5 min) */
extern int POMODORO_SHORT_BREAK;

/** @brief Pomodoro long break duration in seconds (default: 900 = 15 min) */
extern int POMODORO_LONG_BREAK;

/* Notification message texts */
/** @brief Timeout notification message text (supports placeholders) */
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];

/** @brief Pomodoro timeout message text (supports placeholders) */
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];

/** @brief Pomodoro cycle completion message text */
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

/* Notification display settings */
/** @brief Notification display timeout in milliseconds (default: 5000) */
extern int NOTIFICATION_TIMEOUT_MS;

/** @brief Current notification type setting (Catime/Modal/OS) */
extern NotificationType NOTIFICATION_TYPE;

/** @brief Notifications globally disabled flag */
extern BOOL NOTIFICATION_DISABLED;

/* Notification sound settings */
/** @brief Notification sound file path (UTF-8, relative or absolute) */
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];

/** @brief Notification sound volume level (0-100, default: 50) */
extern int NOTIFICATION_SOUND_VOLUME;

/* Font license tracking */
/** @brief Font license agreement accepted flag */
extern BOOL FONT_LICENSE_ACCEPTED;

/** @brief Accepted font license version from config (for version upgrades) */
extern char FONT_LICENSE_VERSION_ACCEPTED[16];

/* Time display format settings */
/** @brief Current time format setting (default, zero-padded, full-padded) */
extern TimeFormatType CLOCK_TIME_FORMAT;

/** @brief Time format preview active flag (for live preview in settings) */
extern BOOL IS_TIME_FORMAT_PREVIEWING;

/** @brief Preview time format value (used during live preview) */
extern TimeFormatType PREVIEW_TIME_FORMAT;

/** @brief Centiseconds display setting (2-digit precision after decimal) */
extern BOOL CLOCK_SHOW_MILLISECONDS;

/** @brief Centiseconds preview active flag */
extern BOOL IS_MILLISECONDS_PREVIEWING;

/** @brief Preview centiseconds display value */
extern BOOL PREVIEW_SHOW_MILLISECONDS;

/* ============================================================================
 * Animation speed functions
 * ============================================================================ */

/**
 * @brief Get animation speed metric configured in INI
 * @return Animation speed metric enum value
 * 
 * @details
 * - Reads [Animation] ANIMATION_SPEED_METRIC key
 * - Returns ANIMATION_SPEED_MEMORY by default
 * - Controls whether tray icon animation speed is based on:
 *   * Memory usage (0-100%)
 *   * CPU usage (0-100%)
 *   * Timer progress (0-100%)
 */
AnimationSpeedMetric GetAnimationSpeedMetric(void);

/**
 * @brief Map utilization percent (0-100) to speed scale percent
 * @param percent Utilization percentage (0.0-100.0)
 * @return Speed scale percentage (e.g., 50.0 = half speed, 200.0 = double speed)
 * 
 * @details
 * - Uses fixed-range mappings from [Animation] section
 * - Keys: ANIMATION_SPEED_MAP_LOW-HIGH = SCALE[%]
 * - Example: ANIMATION_SPEED_MAP_0-20 = 50 (0-20% usage → 50% speed)
 * - Returns 100.0 if no mapping matches (normal speed)
 * 
 * @note Call ReloadAnimationSpeedFromConfig() after editing config
 */
double GetAnimationSpeedScaleForPercent(double percent);

/**
 * @brief Reload animation speed metric and mappings from config.ini
 * 
 * @details
 * - Updates internal mapping tables
 * - Parses all ANIMATION_SPEED_MAP_* keys
 * - Should be called after config file changes
 * - Thread-safe (uses internal locking)
 */
void ReloadAnimationSpeedFromConfig(void);

/* ============================================================================
 * Core configuration functions
 * ============================================================================ */

/**
 * @brief Get configuration file path with automatic directory creation
 * @param path Buffer to store config file path (UTF-8)
 * @param size Size of path buffer
 * 
 * @details
 * - Returns %APPDATA%\Catime\config.ini
 * - Creates Catime folder if it doesn't exist
 * - Thread-safe
 * - UTF-8 encoded output path
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Read all configuration from file with validation
 * 
 * @details Comprehensive config loading including:
 * - Language detection (system or saved)
 * - All timer, display, and notification settings
 * - Hotkey assignments
 * - Recent files list
 * - Font license status
 * - Animation settings
 * - Creates default config if missing
 * - Validates and sanitizes all values
 * - Updates UI elements after load
 * 
 * @note Should be called during app initialization
 */
void ReadConfig();

/**
 * @brief Check and create audio/fonts/animations resource folders
 * 
 * @details Creates structure:
 * - %APPDATA%\Catime\resources\audio
 * - %APPDATA%\Catime\resources\fonts
 * - %APPDATA%\Catime\resources\animations
 * 
 * @note Idempotent - safe to call multiple times
 */
void CheckAndCreateAudioFolder();

/**
 * @brief Get animations folder path for tray animations
 * @param path Buffer to store animations folder path (UTF-8)
 * @param size Size of path buffer
 * 
 * @details
 * - Returns %APPDATA%\Catime\resources\animations
 * - Creates folder if it doesn't exist
 * - Used for custom frame-by-frame tray animations
 */
void GetAnimationsFolderPath(char* path, size_t size);

/**
 * @brief Write timeout action configuration with security filtering
 * @param action Timeout action enum string (MESSAGE, LOCK, SHUTDOWN, etc.)
 * 
 * @details
 * - Updates [Timer] CLOCK_TIMEOUT_ACTION key
 * - Validates action against allowed enum values
 * - Atomic write (mutex-protected)
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Write time options configuration (quick countdown values)
 * @param options Time options comma-separated string (e.g., "5,10,15")
 * 
 * @details
 * - Updates [Timer] CLOCK_TIME_OPTIONS key
 * - Used for quick countdown menu items
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Load recent files from configuration with validation
 * 
 * @details
 * - Reads [RecentFiles] RecentFile1..RecentFile5 keys
 * - Validates file existence (skips missing files)
 * - Extracts display names from paths
 * - Updates CLOCK_RECENT_FILES global array
 * - Updates tray menu with recent files
 */
void LoadRecentFiles(void);

/**
 * @brief Save file to recent files list with MRU ordering
 * @param filePath Path of file to add to recent list (UTF-8)
 * 
 * @details
 * - Adds to top of list (most recent)
 * - Removes duplicates (case-sensitive)
 * - Maintains MAX_RECENT_FILES limit
 * - Updates config file atomically
 * - Refreshes tray menu
 * 
 * @note File doesn't need to exist to be added
 */
void SaveRecentFile(const char* filePath);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Convert UTF-8 string to ANSI (GB2312) with memory allocation
 * @param utf8Str UTF-8 encoded string
 * @return Newly allocated ANSI string (caller must free with free())
 * 
 * @details
 * - Uses CP_ACP (system ANSI code page)
 * - Returns NULL on allocation failure
 * - Caller responsible for freeing returned memory
 * 
 * @note Primarily for legacy API compatibility
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Create default configuration file with system language detection
 * @param config_path Path where to create config file (UTF-8)
 * 
 * @details Creates config.ini with:
 * - System language auto-detected from Windows locale
 * - Default timer values (25 min countdown)
 * - Default Pomodoro times (25/5/15 min)
 * - Default hotkeys
 * - Empty recent files list
 * - Default color palette (16 colors)
 * - Default notification settings
 * 
 * @note Only creates if file doesn't exist
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Write complete configuration to file (atomic operation)
 * @param config_path Configuration file path (UTF-8)
 * 
 * @details
 * - Writes all current settings to temp file
 * - Atomically renames temp → config.ini
 * - Mutex-protected for thread safety
 * - UTF-8 encoded output
 * - Preserves comments (currently not implemented)
 * 
 * @note Used internally by specific write functions
 */
void WriteConfig(const char* config_path);

/* ============================================================================
 * Specific configuration writers (Pomodoro, Timer, Window)
 * ============================================================================ */

/**
 * @brief Write Pomodoro timing configuration (atomic)
 * @param work Work session duration in seconds
 * @param short_break Short break duration in seconds
 * @param long_break Long break duration in seconds
 * 
 * @details
 * - Updates [Pomodoro] POMODORO_WORK_TIME, POMODORO_SHORT_BREAK, POMODORO_LONG_BREAK
 * - Atomic write (all three values written together)
 * - Updates global variables immediately
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

/**
 * @brief Alias for WriteConfigPomodoroTimes
 * @param work_time Work session duration in seconds
 * @param short_break Short break duration in seconds
 * @param long_break Long break duration in seconds
 */
void WriteConfigPomodoroSettings(int work_time, int short_break, int long_break);

/**
 * @brief Write Pomodoro loop count setting
 * @param loop_count Number of cycles before long break (1-99)
 * 
 * @details
 * - Updates [Pomodoro] POMODORO_LOOP_COUNT key
 * - Used to determine when to take long break vs short break
 */
void WriteConfigPomodoroLoopCount(int loop_count);

/**
 * @brief Configure timeout action to open file
 * @param filePath File path to open on timeout (UTF-8)
 * 
 * @details
 * - Updates [Timer] CLOCK_TIMEOUT_FILE key
 * - Sets CLOCK_TIMEOUT_ACTION to OPEN_FILE
 * - Updates global CLOCK_TIMEOUT_FILE_PATH variable
 */
void WriteConfigTimeoutFile(const char* filePath);

/**
 * @brief Write window always-on-top setting
 * @param topmost "TRUE" or "FALSE" string
 * 
 * @details
 * - Updates [Display] WINDOW_TOPMOST key
 * - Changes window style immediately (if window exists)
 */
void WriteConfigTopmost(const char* topmost);

/**
 * @brief Configure timeout action to open website URL
 * @param url Website URL to open on timeout (UTF-8)
 * 
 * @details
 * - Updates [Timer] CLOCK_TIMEOUT_WEBSITE key
 * - Sets CLOCK_TIMEOUT_ACTION to OPEN_WEBSITE
 * - No URL validation (allows flexible use)
 */
void WriteConfigTimeoutWebsite(const char* url);

/**
 * @brief Write custom Pomodoro time intervals to config
 * @param times Array of time intervals in seconds
 * @param count Number of time values in array
 * 
 * @details
 * - Updates [Pomodoro] POMODORO_TIME_OPTIONS key
 * - Used for quick Pomodoro menu items (p1, p2, p3, etc.)
 * - Comma-separated format in config
 */
void WriteConfigPomodoroTimeOptions(int* times, int count);

/* ============================================================================
 * Notification configuration functions
 * ============================================================================ */

/**
 * @brief Read notification message texts from configuration
 * 
 * @details
 * - Reads [Notification] CLOCK_TIMEOUT_MESSAGE_TEXT
 * - Reads [Notification] POMODORO_TIMEOUT_MESSAGE_TEXT
 * - Reads [Notification] POMODORO_CYCLE_COMPLETE_TEXT
 * - Updates global message text variables
 * - Uses fallback messages if not found
 */
void ReadNotificationMessagesConfig(void);

/**
 * @brief Write notification display timeout setting
 * @param timeout_ms Timeout in milliseconds (recommended: 3000-10000)
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_TIMEOUT_MS key
 * - Affects how long notification stays visible
 */
void WriteConfigNotificationTimeout(int timeout_ms);

/**
 * @brief Read notification timeout from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_TIMEOUT_MS key
 * - Updates global NOTIFICATION_TIMEOUT_MS variable
 * - Default: 5000ms if not found
 */
void ReadNotificationTimeoutConfig(void);

/**
 * @brief Read notification opacity from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_MAX_OPACITY key
 * - Updates global NOTIFICATION_MAX_OPACITY variable
 * - Validates range (1-100)
 * - Default: 100 (fully opaque) if not found
 */
void ReadNotificationOpacityConfig(void);

/**
 * @brief Write notification opacity setting
 * @param opacity Opacity value (1-100, where 100 = fully opaque)
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_MAX_OPACITY key
 * - Clamped to 1-100 range automatically
 */
void WriteConfigNotificationOpacity(int opacity);

/**
 * @brief Write notification message texts (atomic)
 * @param timeout_msg Timeout notification message (UTF-8)
 * @param pomodoro_msg Pomodoro notification message (UTF-8)
 * @param cycle_complete_msg Cycle completion message (UTF-8)
 * 
 * @details
 * - Updates all three message keys atomically
 * - Supports placeholder text (e.g., "%time%", "%date%")
 * - Empty strings use default localized messages
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg);

/**
 * @brief Read notification type from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_TYPE key
 * - Updates global NOTIFICATION_TYPE variable
 * - Values: CATIME, SYSTEM_MODAL, OS
 * - Default: CATIME if not found
 */
void ReadNotificationTypeConfig(void);

/**
 * @brief Write notification type setting with validation
 * @param type Notification type enum value
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_TYPE key
 * - Converts enum to string (CATIME/SYSTEM_MODAL/OS)
 */
void WriteConfigNotificationType(NotificationType type);

/**
 * @brief Read notification globally disabled flag from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_DISABLED key
 * - Updates global NOTIFICATION_DISABLED variable
 * - Default: FALSE (enabled)
 */
void ReadNotificationDisabledConfig(void);

/**
 * @brief Write notification disabled setting
 * @param disabled TRUE to disable all notifications
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_DISABLED key
 * - When TRUE, suppresses all notification types
 */
void WriteConfigNotificationDisabled(BOOL disabled);

/**
 * @brief Write language setting to config file
 * @param language Language ID from AppLanguage enum
 * 
 * @details
 * - Updates [General] LANGUAGE key
 * - Converts enum to string (English, Chinese_Simplified, etc.)
 * - Triggers UI language reload
 */
void WriteConfigLanguage(int language);

/**
 * @brief Get audio resources folder path with auto-creation
 * @param path Buffer to store audio folder path (UTF-8)
 * @param size Size of path buffer
 * 
 * @details
 * - Returns %APPDATA%\Catime\resources\audio
 * - Creates folder if it doesn't exist
 * - Used for notification sound files
 */
void GetAudioFolderPath(char* path, size_t size);

/**
 * @brief Read notification sound file path from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_SOUND_FILE key
 * - Updates global NOTIFICATION_SOUND_FILE variable
 * - Supports:
 *   * Absolute paths
 *   * Relative paths (relative to audio folder)
 *   * Special value "SYSTEM_BEEP" for system beep
 * - Empty string = no sound
 */
void ReadNotificationSoundConfig(void);

/**
 * @brief Write notification sound file setting with path sanitization
 * @param sound_file Path to sound file (UTF-8)
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_SOUND_FILE key
 * - Accepts:
 *   * Absolute paths
 *   * Relative paths (stored as-is)
 *   * "SYSTEM_BEEP" keyword
 *   * Empty string to disable sound
 * - No file existence validation (allows future files)
 */
void WriteConfigNotificationSound(const char* sound_file);

/**
 * @brief Read notification volume from configuration
 * 
 * @details
 * - Reads [Notification] NOTIFICATION_SOUND_VOLUME key
 * - Updates global NOTIFICATION_SOUND_VOLUME variable
 * - Validates range (0-100)
 * - Default: 50 if not found
 */
void ReadNotificationVolumeConfig(void);

/**
 * @brief Write notification volume setting
 * @param volume Volume level (0-100, where 0 = mute, 100 = max)
 * 
 * @details
 * - Updates [Notification] NOTIFICATION_SOUND_VOLUME key
 * - Clamped to 0-100 range automatically
 * - Applies to miniaudio playback (not system beep)
 */
void WriteConfigNotificationVolume(int volume);

/* ============================================================================
 * Hotkey configuration functions
 * ============================================================================ */

/**
 * @brief Convert hotkey WORD to human-readable string
 * @param hotkey Hotkey value to convert (LOWORD=VK, HIWORD=modifiers)
 * @param buffer Buffer to store string representation
 * @param bufferSize Size of buffer
 * 
 * @details
 * - Converts Windows hotkey code to string like "Ctrl+Shift+A"
 * - Handles modifiers: Ctrl, Alt, Shift, Win
 * - Maps virtual key codes to names (F1-F12, A-Z, 0-9, etc.)
 * - Returns "None" for hotkey value 0
 * 
 * Example: 0x0341 (Ctrl+A) → "Ctrl+A"
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

/**
 * @brief Parse human-readable hotkey string to Windows hotkey code
 * @param str String representation like "Ctrl+Shift+A" or "None"
 * @return Hotkey WORD value (LOWORD=VK, HIWORD=modifiers)
 * 
 * @details
 * - Parses modifier keys (case-insensitive): Ctrl, Alt, Shift, Win
 * - Parses virtual key names: F1-F12, A-Z, 0-9, Space, Enter, etc.
 * - "None" or empty string returns 0
 * - Tolerates extra whitespace and various separators (+, -, space)
 * 
 * Example: "Ctrl+A" → 0x0341
 */
WORD StringToHotkey(const char* str);

/**
 * @brief Read all hotkey assignments from configuration
 * @param showTimeHotkey Output: Show time toggle hotkey
 * @param countUpHotkey Output: Count-up timer hotkey
 * @param countdownHotkey Output: Countdown timer hotkey
 * @param quickCountdown1Hotkey Output: Quick countdown 1 hotkey
 * @param quickCountdown2Hotkey Output: Quick countdown 2 hotkey
 * @param quickCountdown3Hotkey Output: Quick countdown 3 hotkey
 * @param pomodoroHotkey Output: Pomodoro timer hotkey
 * @param toggleVisibilityHotkey Output: Visibility toggle hotkey
 * @param editModeHotkey Output: Edit mode toggle hotkey
 * @param pauseResumeHotkey Output: Pause/resume hotkey
 * @param restartTimerHotkey Output: Restart timer hotkey
 * 
 * @details
 * - Reads all hotkeys from [Hotkeys] section
 * - Parses string format to WORD values
 * - Uses default hotkeys if not found in config
 * - Validates hotkey values (no duplicates check)
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey);

/**
 * @brief Read custom countdown hotkey from configuration
 * @param hotkey Output buffer for hotkey value
 * 
 * @details
 * - Reads [Hotkeys] CUSTOM_COUNTDOWN_HOTKEY key
 * - Used for dialog-based custom countdown input
 */
void ReadCustomCountdownHotkey(WORD* hotkey);

/**
 * @brief Write all hotkey assignments to configuration (atomic)
 * @param showTimeHotkey Show time toggle hotkey
 * @param countUpHotkey Count-up timer hotkey
 * @param countdownHotkey Countdown timer hotkey
 * @param quickCountdown1Hotkey Quick countdown 1 hotkey
 * @param quickCountdown2Hotkey Quick countdown 2 hotkey
 * @param quickCountdown3Hotkey Quick countdown 3 hotkey
 * @param pomodoroHotkey Pomodoro timer hotkey
 * @param toggleVisibilityHotkey Visibility toggle hotkey
 * @param editModeHotkey Edit mode toggle hotkey
 * @param pauseResumeHotkey Pause/resume hotkey
 * @param restartTimerHotkey Restart timer hotkey
 * 
 * @details
 * - Writes all 11 hotkeys atomically to [Hotkeys] section
 * - Converts WORD values to human-readable strings
 * - 0 value writes as "None" (no hotkey assigned)
 * - Updates config file immediately
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey);

/**
 * @brief Write arbitrary key-value pair to appropriate config section
 * @param key Configuration key name
 * @param value Configuration value
 * 
 * @details
 * - Auto-determines section based on key name prefix
 * - For keys without standard section, uses [General]
 * - Atomic write operation
 * - Useful for custom/advanced settings
 * 
 * @note Prefer specific Write* functions when available
 */
void WriteConfigKeyValue(const char* key, const char* value);

/**
 * @brief Check if desktop shortcut verification has been completed
 * @return TRUE if shortcut check was done, FALSE otherwise
 * 
 * @details
 * - Reads [Options] SHORTCUT_CHECK_DONE key
 * - Used to show "Create Desktop Shortcut" dialog only once
 */
bool IsShortcutCheckDone(void);

/**
 * @brief Set desktop shortcut creation check status
 * @param done TRUE if check completed
 * 
 * @details
 * - Writes [Options] SHORTCUT_CHECK_DONE key
 * - Prevents duplicate shortcut creation prompts
 */
void SetShortcutCheckDone(bool done);

/* ============================================================================
 * Low-level INI file I/O functions (UTF-8 support)
 * ============================================================================ */

/**
 * @brief Read string value from INI file with Unicode support
 * @param section INI section name (UTF-8)
 * @param key INI key name (UTF-8)
 * @param defaultValue Default value if key not found (UTF-8)
 * @param returnValue Buffer to store retrieved value (UTF-8)
 * @param returnSize Size of return buffer in bytes
 * @param filePath Path to INI file (UTF-8)
 * @return Number of characters copied to buffer (excluding null terminator)
 * 
 * @details
 * - Wraps GetPrivateProfileString with UTF-8 conversion
 * - Thread-safe (but not process-safe without external locking)
 * - Returns defaultValue if section/key not found
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

/**
 * @brief Write string value to INI file with Unicode support
 * @param section INI section name (UTF-8)
 * @param key INI key name (UTF-8)
 * @param value Value to write (UTF-8)
 * @param filePath Path to INI file (UTF-8)
 * @return TRUE on success, FALSE on failure
 * 
 * @details
 * - Wraps WritePrivateProfileString with UTF-8 conversion
 * - Creates section/key if doesn't exist
 * - Updates existing value if exists
 * - NOT atomic (use higher-level Write* functions for atomicity)
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

/**
 * @brief Read integer value from INI file with Unicode support
 * @param section INI section name (UTF-8)
 * @param key INI key name (UTF-8)
 * @param defaultValue Default value if key not found
 * @param filePath Path to INI file (UTF-8)
 * @return Retrieved integer value or defaultValue
 * 
 * @details
 * - Parses decimal integers from INI values
 * - Returns defaultValue if key not found or parse fails
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

/**
 * @brief Read boolean value from INI file with Unicode support
 * @param section INI section name (UTF-8)
 * @param key INI key name (UTF-8)
 * @param defaultValue Default boolean if key not found
 * @param filePath Path to INI file (UTF-8)
 * @return TRUE or FALSE
 * 
 * @details
 * - Accepts: TRUE/FALSE, true/false, 1/0, yes/no (case-insensitive)
 * - Returns defaultValue if key not found or unrecognized value
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath);

/**
 * @brief Write integer value to INI file with Unicode support
 * @param section INI section name (UTF-8)
 * @param key INI key name (UTF-8)
 * @param value Value to write
 * @param filePath Path to INI file (UTF-8)
 * @return TRUE on success, FALSE on failure
 * 
 * @details
 * - Converts integer to decimal string
 * - NOT atomic (use higher-level Write* functions for atomicity)
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

/* ============================================================================
 * First-run and font license tracking
 * ============================================================================ */

/**
 * @brief Check if this is the first run of the application
 * @return TRUE if first run, FALSE otherwise
 * 
 * @details
 * - Reads [General] FIRST_RUN key from config
 * - TRUE if config doesn't exist or key is "TRUE"
 * - Used to show welcome screens, setup wizards, etc.
 */
BOOL IsFirstRun(void);

/**
 * @brief Set first run flag to FALSE
 * 
 * @details
 * - Writes [General] FIRST_RUN = FALSE to config
 * - Should be called after initial setup complete
 */
void SetFirstRunCompleted(void);

/**
 * @brief Set font license agreement acceptance status
 * @param accepted TRUE if user accepted the license agreement
 * 
 * @details
 * - Writes [General] FONT_LICENSE_ACCEPTED key
 * - Required before using bundled fonts
 */
void SetFontLicenseAccepted(BOOL accepted);

/**
 * @brief Set font license version acceptance status
 * @param version Version string that was accepted (e.g., "1.0")
 * 
 * @details
 * - Writes [General] FONT_LICENSE_VERSION_ACCEPTED key
 * - Used to re-prompt users on license updates
 */
void SetFontLicenseVersionAccepted(const char* version);

/**
 * @brief Check if font license version needs user acceptance
 * @return TRUE if current version needs acceptance, FALSE if already accepted
 * 
 * @details
 * - Compares current font license version with accepted version
 * - Returns TRUE if versions differ or no acceptance recorded
 * - Triggers license agreement dialog if TRUE
 */
BOOL NeedsFontLicenseVersionAcceptance(void);

/**
 * @brief Get current font license version
 * @return Current font license version string (const, do not free)
 * 
 * @details
 * - Returns hardcoded version string from code
 * - Compared against saved version in config
 */
const char* GetCurrentFontLicenseVersion(void);

/* ============================================================================
 * Time display and timer configuration
 * ============================================================================ */

/**
 * @brief Write time format setting to config file
 * @param format Time format type to set
 * 
 * @details
 * - Writes [Display] CLOCK_TIME_FORMAT key
 * - Values: DEFAULT, ZERO_PADDED, FULL_PADDED
 * - Updates global CLOCK_TIME_FORMAT variable
 * - Refreshes UI immediately
 */
void WriteConfigTimeFormat(TimeFormatType format);

/**
 * @brief Write milliseconds/centiseconds display setting to config file
 * @param showMilliseconds TRUE to show 2-digit precision, FALSE to hide
 * 
 * @details
 * - Writes [Display] CLOCK_SHOW_MILLISECONDS key
 * - Affects timer update frequency (10ms vs 1000ms)
 * - Updates global CLOCK_SHOW_MILLISECONDS variable
 * - Resets timer with appropriate interval
 */
void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/**
 * @brief Get appropriate timer interval based on milliseconds display setting
 * @return Timer interval in milliseconds (10ms if showing centiseconds, 1000ms otherwise)
 * 
 * @details
 * - Used for SetTimer() calls
 * - 10ms for centisecond precision
 * - 1000ms for second precision (performance optimization)
 */
UINT GetTimerInterval(void);

/**
 * @brief Reset timer with appropriate interval based on milliseconds display
 * @param hwnd Window handle for timer
 * 
 * @details
 * - Kills existing timer
 * - Creates new timer with correct interval (10ms or 1000ms)
 * - Should be called after changing centiseconds setting
 */
void ResetTimerWithInterval(HWND hwnd);

/**
 * @brief Write startup mode to configuration
 * @param mode Startup mode string ("COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY")
 * 
 * @details
 * - Persists to [Timer] CLOCK_STARTUP_MODE key
 * - Determines timer behavior on application launch
 * - Validates against known modes
 */
void WriteConfigStartupMode(const char* mode);

/**
 * @brief Force flush configuration changes to disk immediately
 * 
 * @details
 * - Calls WritePrivateProfileString(NULL, NULL, NULL, ...)
 * - Ensures all pending writes are committed
 * - Use sparingly (performance impact)
 * - Useful before app shutdown or critical operations
 */
void FlushConfigToDisk(void);

/* ============================================================================
 * Tray icon animation color configuration
 * ============================================================================ */

/**
 * @brief Read percent tray icon colors from config
 * 
 * @details Reads:
 * - [Animation] PERCENT_ICON_TEXT_COLOR (foreground)
 * - [Animation] PERCENT_ICON_BG_COLOR (background)
 * 
 * Accepts formats:
 * - Hex: "#RRGGBB" or "RRGGBB"
 * - RGB: "R,G,B" (0-255)
 * 
 * Defaults:
 * - Text: black (#000000)
 * - Background: white (#FFFFFF)
 */
void ReadPercentIconColorsConfig(void);

/**
 * @brief Get percent tray icon text color as COLORREF
 * @return Foreground color for percent display
 * 
 * @note Call ReadPercentIconColorsConfig() first
 */
COLORREF GetPercentIconTextColor(void);

/**
 * @brief Get percent tray icon background color as COLORREF
 * @return Background color for percent display
 * 
 * @note Call ReadPercentIconColorsConfig() first
 */
COLORREF GetPercentIconBgColor(void);

#endif