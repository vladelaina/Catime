/**
 * @file config.h
 * @brief Configuration management module header file
 *
 * This file defines the application configuration interface,
 * including reading, writing, and managing settings related to windows, fonts, colors, and other customizable options.
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

// Define maximum of 5 recent files
#define MAX_RECENT_FILES 5

// INI configuration file section definitions
#define INI_SECTION_GENERAL       "General"        // General settings (including version, language, etc.)
#define INI_SECTION_DISPLAY       "Display"        // Display settings (font, color, window position, etc.)
#define INI_SECTION_TIMER         "Timer"          // Timer settings (default time, etc.)
#define INI_SECTION_POMODORO      "Pomodoro"       // Pomodoro settings
#define INI_SECTION_NOTIFICATION  "Notification"   // Notification settings
#define INI_SECTION_HOTKEYS       "Hotkeys"        // Hotkey settings
#define INI_SECTION_RECENTFILES   "RecentFiles"    // Recently used files
#define INI_SECTION_COLORS        "Colors"         // Color options
#define INI_SECTION_OPTIONS       "Options"        // Other options

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
extern int CLOCK_RECENT_FILES_COUNT;
extern int CLOCK_DEFAULT_START_TIME;
extern time_t last_config_time;
extern int POMODORO_WORK_TIME;      // Work time (minutes)
extern int POMODORO_SHORT_BREAK;    // Short break time (minutes) 
extern int POMODORO_LONG_BREAK;     // Long break time (minutes)

// New: Variables for storing custom notification messages
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];       ///< Notification message when countdown ends
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];    ///< Notification message when pomodoro time segment ends
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];     ///< Notification message when all pomodoro cycles complete

// New: Variable for storing notification display time
extern int NOTIFICATION_TIMEOUT_MS;  ///< Notification display duration (milliseconds)

// New: Notification type enumeration
typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,      // Catime notification window
    NOTIFICATION_TYPE_SYSTEM_MODAL,    // System modal window
    NOTIFICATION_TYPE_OS               // Operating system notification
} NotificationType;

// Notification type global variable declaration
extern NotificationType NOTIFICATION_TYPE;

// New: Whether to disable notification window
extern BOOL NOTIFICATION_DISABLED;  ///< Whether notification window is disabled

// New: Notification audio related configuration
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];  ///< Notification audio file path

// New: Notification audio volume
extern int NOTIFICATION_SOUND_VOLUME;  ///< Notification audio volume (0-100)

/// @name Configuration related function declarations
/// @{

/**
 * @brief Get configuration file path
 * @param path Buffer to store configuration file path
 * @param size Buffer size
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Read configuration from file
 */
void ReadConfig();

/**
 * @brief Check and create audio folder
 * 
 * Check if the audio folder exists in the same directory as the configuration file, create it if it doesn't exist
 */
void CheckAndCreateAudioFolder();

/**
 * @brief Write timeout action to configuration file
 * @param action Timeout action to write
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Write time options to configuration file
 * @param options Time options to write
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Load recently used files from configuration
 */
void LoadRecentFiles(void);

/**
 * @brief Save recently used file to configuration
 * @param filePath File path to save
 */
void SaveRecentFile(const char* filePath);

/**
 * @brief Convert UTF-8 string to ANSI encoding
 * @param utf8Str UTF-8 string to convert
 * @return Converted ANSI string
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Create default configuration file
 * @param config_path Path of configuration file
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Write all configuration settings to file
 * @param config_path Path of configuration file
 */
void WriteConfig(const char* config_path);

/**
 * @brief Write pomodoro times to configuration file
 * @param work Work time (minutes)
 * @param short_break Short break time (minutes)
 * @param long_break Long break time (minutes)
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

/**
 * @brief Write pomodoro time settings
 * @param work_time Work time (seconds)
 * @param short_break Short break time (seconds)
 * @param long_break Long break time (seconds)
 * 
 * Update pomodoro time settings in configuration file, including work, short break and long break times
 */
void WriteConfigPomodoroSettings(int work_time, int short_break, int long_break);

/**
 * @brief Write pomodoro loop count setting
 * @param loop_count Loop count
 * 
 * Update pomodoro loop count setting in configuration file
 */
void WriteConfigPomodoroLoopCount(int loop_count);

/**
 * @brief Write timeout open file path
 * @param filePath File path
 * 
 * Update timeout open file path in configuration file, also set timeout action to open file
 */
void WriteConfigTimeoutFile(const char* filePath);

/**
 * @brief Write window topmost status to configuration file
 * @param topmost Topmost status string ("TRUE"/"FALSE")
 */
void WriteConfigTopmost(const char* topmost);

/**
 * @brief Write timeout open website URL
 * @param url Website URL
 * 
 * Update timeout open website URL in configuration file, also set timeout action to open website
 */
void WriteConfigTimeoutWebsite(const char* url);

/**
 * @brief Write pomodoro time options
 * @param times Time array (seconds)
 * @param count Length of time array
 * 
 * Write pomodoro time options to configuration file
 */
void WriteConfigPomodoroTimeOptions(int* times, int count);

/**
 * @brief Read notification message text from configuration file
 * 
 * Specifically read CLOCK_TIMEOUT_MESSAGE_TEXT, POMODORO_TIMEOUT_MESSAGE_TEXT and POMODORO_CYCLE_COMPLETE_TEXT
 * and update corresponding global variables.
 */
void ReadNotificationMessagesConfig(void);

/**
 * @brief Write notification display time configuration
 * @param timeout_ms Notification display time (milliseconds)
 * 
 * Update notification display time in configuration file, and update global variable.
 */
void WriteConfigNotificationTimeout(int timeout_ms);

/**
 * @brief Read notification display time from configuration file
 * 
 * Specifically read NOTIFICATION_TIMEOUT_MS configuration item and update corresponding global variable.
 */
void ReadNotificationTimeoutConfig(void);

/**
 * @brief Read notification maximum opacity from configuration file
 * 
 * Specifically read NOTIFICATION_MAX_OPACITY configuration item
 * and update corresponding global variable. If configuration doesn't exist, keep default value unchanged.
 */
void ReadNotificationOpacityConfig(void);

/**
 * @brief Write notification maximum opacity configuration
 * @param opacity Opacity percentage value (1-100)
 * 
 * Update notification maximum opacity setting in configuration file,
 * using temporary file method to ensure configuration update safety.
 */
void WriteConfigNotificationOpacity(int opacity);

/**
 * @brief Write notification message configuration
 * @param timeout_msg Countdown timeout prompt text
 * @param pomodoro_msg Pomodoro timeout prompt text
 * @param cycle_complete_msg Pomodoro cycle completion prompt text
 * 
 * Update notification message settings in configuration file,
 * using temporary file method to ensure configuration update safety.
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg);

/**
 * @brief Read notification type from configuration file
 * 
 * Specifically read NOTIFICATION_TYPE configuration item and update corresponding global variable.
 */
void ReadNotificationTypeConfig(void);

/**
 * @brief Write notification type configuration
 * @param type Notification type enumeration value
 * 
 * Update notification type setting in configuration file,
 * using temporary file method to ensure configuration update safety.
 */
void WriteConfigNotificationType(NotificationType type);

/**
 * @brief Read notification disable setting from configuration file
 * 
 * Specifically read NOTIFICATION_DISABLED configuration item and update corresponding global variable.
 */
void ReadNotificationDisabledConfig(void);

/**
 * @brief Write notification disable configuration
 * @param disabled Whether to disable notifications (TRUE/FALSE)
 * 
 * Update notification disable setting in configuration file,
 * using temporary file method to ensure configuration update safety.
 */
void WriteConfigNotificationDisabled(BOOL disabled);

/**
 * @brief Write language setting
 * @param language Language ID
 * 
 * Write current language setting to configuration file
 */
void WriteConfigLanguage(int language);

/**
 * @brief Get audio folder path
 * @param path Buffer to store path
 * @param size Buffer size
 */
void GetAudioFolderPath(char* path, size_t size);

/**
 * @brief Read notification audio setting from configuration file
 */
void ReadNotificationSoundConfig(void);

/**
 * @brief Write notification audio configuration
 * @param sound_file Audio file path
 */
void WriteConfigNotificationSound(const char* sound_file);

/**
 * @brief Read notification audio volume from configuration file
 */
void ReadNotificationVolumeConfig(void);

/**
 * @brief Write notification audio volume configuration
 * @param volume Volume percentage (0-100)
 */
void WriteConfigNotificationVolume(int volume);

/**
 * @brief Convert hotkey value to readable string
 * @param hotkey Hotkey value
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

/**
 * @brief Convert string to hotkey value
 * @param str Hotkey string
 * @return WORD Hotkey value
 */
WORD StringToHotkey(const char* str);

/**
 * @brief Read hotkey settings from configuration file
 * @param showTimeHotkey Pointer to store show time hotkey
 * @param countUpHotkey Pointer to store count up hotkey
 * @param countdownHotkey Pointer to store countdown hotkey
 * @param quickCountdown1Hotkey Pointer to store quick countdown 1 hotkey
 * @param quickCountdown2Hotkey Pointer to store quick countdown 2 hotkey
 * @param quickCountdown3Hotkey Pointer to store quick countdown 3 hotkey
 * @param pomodoroHotkey Pointer to store pomodoro hotkey
 * @param toggleVisibilityHotkey Pointer to store hide/show hotkey
 * @param editModeHotkey Pointer to store edit mode hotkey
 * @param pauseResumeHotkey Pointer to store pause/resume hotkey
 * @param restartTimerHotkey Pointer to store restart hotkey
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey);

/**
 * @brief Read custom countdown hotkey from configuration file
 * @param hotkey Pointer to store hotkey
 */
void ReadCustomCountdownHotkey(WORD* hotkey);

/**
 * @brief Write hotkey configuration
 * @param showTimeHotkey Show time hotkey value
 * @param countUpHotkey Count up hotkey value
 * @param countdownHotkey Countdown hotkey value
 * @param quickCountdown1Hotkey Quick countdown 1 hotkey value
 * @param quickCountdown2Hotkey Quick countdown 2 hotkey value
 * @param quickCountdown3Hotkey Quick countdown 3 hotkey value
 * @param pomodoroHotkey Pomodoro hotkey value
 * @param toggleVisibilityHotkey Hide/show hotkey value
 * @param editModeHotkey Edit mode hotkey value
 * @param pauseResumeHotkey Pause/resume hotkey value
 * @param restartTimerHotkey Restart hotkey value
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey);

/**
 * @brief Write configuration item
 * @param key Configuration item key name
 * @param value Configuration item value
 */
void WriteConfigKeyValue(const char* key, const char* value);

/**
 * @brief Check if shortcut check has been performed
 * @return bool true means already checked, false means not checked yet
 */
bool IsShortcutCheckDone(void);

/**
 * @brief Set shortcut check status
 * @param done Whether check is completed
 */
void SetShortcutCheckDone(bool done);

/**
 * @brief Read string value from INI file
 * @param section Section name
 * @param key Key name
 * @param defaultValue Default value
 * @param returnValue Return value buffer
 * @param returnSize Buffer size
 * @param filePath File path
 * @return Actual number of characters read
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

/**
 * @brief Write string value to INI file
 * @param section Section name
 * @param key Key name
 * @param value Value
 * @param filePath File path
 * @return Whether successful
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

/**
 * @brief Read integer value from INI file
 * @param section Section name
 * @param key Key name
 * @param defaultValue Default value
 * @param filePath File path
 * @return Integer value read
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

/**
 * @brief Write integer value to INI file
 * @param section Section name
 * @param key Key name
 * @param value Value
 * @param filePath File path
 * @return Whether successful
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

/// @}

#endif // CONFIG_H
