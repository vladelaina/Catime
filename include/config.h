/**
 * @file config.h
 * @brief Configuration management system
 * 
 * Comprehensive configuration handling for all application settings,
 * including INI file management, recent files, and user preferences
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

/** @brief Maximum number of recent files to remember */
#define MAX_RECENT_FILES 5

/** @brief INI file section names */
#define INI_SECTION_GENERAL       "General"      /**< General settings */
#define INI_SECTION_DISPLAY       "Display"      /**< Display options */
#define INI_SECTION_TIMER         "Timer"        /**< Timer configuration */
#define INI_SECTION_POMODORO      "Pomodoro"     /**< Pomodoro settings */
#define INI_SECTION_NOTIFICATION  "Notification" /**< Notification config */
#define INI_SECTION_HOTKEYS       "Hotkeys"      /**< Hotkey assignments */
#define INI_SECTION_RECENTFILES   "RecentFiles"  /**< Recent file list */
#define INI_SECTION_COLORS        "Colors"       /**< Color options */
#define INI_SECTION_OPTIONS       "Options"      /**< Misc options */

/**
 * @brief Recent file information structure
 */
typedef struct {
    char path[MAX_PATH];  /**< Full file path */
    char name[MAX_PATH];  /**< Display name */
} RecentFile;

/** @brief Array of recently used files */
extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];

/** @brief Count of recent files */
extern int CLOCK_RECENT_FILES_COUNT;

/** @brief Default countdown start time in seconds */
extern int CLOCK_DEFAULT_START_TIME;

/** @brief Last configuration file modification time */
extern time_t last_config_time;

/** @brief Pomodoro work phase duration */
extern int POMODORO_WORK_TIME;

/** @brief Pomodoro short break duration */
extern int POMODORO_SHORT_BREAK;

/** @brief Pomodoro long break duration */
extern int POMODORO_LONG_BREAK;

/** @brief Timeout notification message text */
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];

/** @brief Pomodoro timeout message text */
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];

/** @brief Pomodoro cycle completion message text */
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

/** @brief Notification display timeout in milliseconds */
extern int NOTIFICATION_TIMEOUT_MS;

/**
 * @brief Notification display types
 */
typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,     /**< Custom Catime notification */
    NOTIFICATION_TYPE_SYSTEM_MODAL,   /**< System modal dialog */
    NOTIFICATION_TYPE_OS              /**< OS native notification */
} NotificationType;

/** @brief Current notification type setting */
extern NotificationType NOTIFICATION_TYPE;

/** @brief Notifications disabled flag */
extern BOOL NOTIFICATION_DISABLED;

/** @brief Notification sound file path */
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];

/** @brief Notification sound volume (0-100) */
extern int NOTIFICATION_SOUND_VOLUME;

/** @brief Font license agreement accepted flag */
extern BOOL FONT_LICENSE_ACCEPTED;

/** @brief Accepted font license version from config */
extern char FONT_LICENSE_VERSION_ACCEPTED[16];

/** @brief Time format types */
typedef enum {
    TIME_FORMAT_DEFAULT = 0,        /**< Default format: 9:59, 9 */
    TIME_FORMAT_ZERO_PADDED = 1,    /**< Zero-padded format: 09:59, 09 */
    TIME_FORMAT_FULL_PADDED = 2     /**< Full zero-padded format: 00:09:59, 00:00:09 */
} TimeFormatType;

/** @brief Current time format setting */
extern TimeFormatType CLOCK_TIME_FORMAT;

/** @brief Time format preview variables */
extern BOOL IS_TIME_FORMAT_PREVIEWING;
extern TimeFormatType PREVIEW_TIME_FORMAT;

/** @brief Milliseconds display setting */
extern BOOL CLOCK_SHOW_MILLISECONDS;

/** @brief Milliseconds preview variables */
extern BOOL IS_MILLISECONDS_PREVIEWING;
extern BOOL PREVIEW_SHOW_MILLISECONDS;

/**
 * @brief Get configuration file path
 * @param path Buffer to store config file path
 * @param size Size of path buffer
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Read all configuration from file
 */
void ReadConfig();

/**
 * @brief Check and create audio folder if needed
 */
void CheckAndCreateAudioFolder();

/**
 * @brief Write timeout action configuration
 * @param action Timeout action string
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Write time options configuration
 * @param options Time options string
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Load recent files from configuration
 */
void LoadRecentFiles(void);

/**
 * @brief Save file to recent files list
 * @param filePath Path of file to add to recent list
 */
void SaveRecentFile(const char* filePath);

/**
 * @brief Convert UTF-8 string to ANSI
 * @param utf8Str UTF-8 encoded string
 * @return Newly allocated ANSI string (caller must free)
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Create default configuration file
 * @param config_path Path where to create config file
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Write current configuration to file
 * @param config_path Configuration file path
 */
void WriteConfig(const char* config_path);

/**
 * @brief Write Pomodoro time settings
 * @param work Work time in seconds
 * @param short_break Short break time in seconds
 * @param long_break Long break time in seconds
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

/**
 * @brief Write Pomodoro settings (alias for WriteConfigPomodoroTimes)
 * @param work_time Work time in seconds
 * @param short_break Short break time in seconds
 * @param long_break Long break time in seconds
 */
void WriteConfigPomodoroSettings(int work_time, int short_break, int long_break);

/**
 * @brief Write Pomodoro loop count setting
 * @param loop_count Number of cycles before long break
 */
void WriteConfigPomodoroLoopCount(int loop_count);

/**
 * @brief Write timeout file action path
 * @param filePath File path for timeout action
 */
void WriteConfigTimeoutFile(const char* filePath);

/**
 * @brief Write window topmost setting
 * @param topmost "TRUE" or "FALSE" string
 */
void WriteConfigTopmost(const char* topmost);

/**
 * @brief Write timeout website URL
 * @param url Website URL for timeout action
 */
void WriteConfigTimeoutWebsite(const char* url);

/**
 * @brief Write Pomodoro time options array
 * @param times Array of time values in seconds
 * @param count Number of time values
 */
void WriteConfigPomodoroTimeOptions(int* times, int count);

/**
 * @brief Read notification messages from configuration
 */
void ReadNotificationMessagesConfig(void);

/**
 * @brief Write notification timeout setting
 * @param timeout_ms Timeout in milliseconds
 */
void WriteConfigNotificationTimeout(int timeout_ms);

/**
 * @brief Read notification timeout from configuration
 */
void ReadNotificationTimeoutConfig(void);

/**
 * @brief Read notification opacity from configuration
 */
void ReadNotificationOpacityConfig(void);

/**
 * @brief Write notification opacity setting
 * @param opacity Opacity value (0-255)
 */
void WriteConfigNotificationOpacity(int opacity);

/**
 * @brief Write notification message texts
 * @param timeout_msg Timeout notification message
 * @param pomodoro_msg Pomodoro notification message
 * @param cycle_complete_msg Cycle completion message
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg);

/**
 * @brief Read notification type from configuration
 */
void ReadNotificationTypeConfig(void);

/**
 * @brief Write notification type setting
 * @param type Notification type enumeration value
 */
void WriteConfigNotificationType(NotificationType type);

/**
 * @brief Read notification disabled flag from configuration
 */
void ReadNotificationDisabledConfig(void);

/**
 * @brief Write notification disabled setting
 * @param disabled TRUE to disable notifications
 */
void WriteConfigNotificationDisabled(BOOL disabled);

/**
 * @brief Write language setting
 * @param language Language enumeration value
 */
void WriteConfigLanguage(int language);

/**
 * @brief Get audio folder path for notification sounds
 * @param path Buffer to store audio folder path
 * @param size Size of path buffer
 */
void GetAudioFolderPath(char* path, size_t size);

/**
 * @brief Read notification sound file from configuration
 */
void ReadNotificationSoundConfig(void);

/**
 * @brief Write notification sound file setting
 * @param sound_file Path to sound file
 */
void WriteConfigNotificationSound(const char* sound_file);

/**
 * @brief Read notification volume from configuration
 */
void ReadNotificationVolumeConfig(void);

/**
 * @brief Write notification volume setting
 * @param volume Volume level (0-100)
 */
void WriteConfigNotificationVolume(int volume);

/**
 * @brief Convert hotkey WORD to string representation
 * @param hotkey Hotkey value to convert
 * @param buffer Buffer to store string representation
 * @param bufferSize Size of buffer
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

/**
 * @brief Convert string representation to hotkey WORD
 * @param str String representation of hotkey
 * @return Hotkey WORD value
 */
WORD StringToHotkey(const char* str);

/**
 * @brief Read all hotkey assignments from configuration
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
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey);

/**
 * @brief Read custom countdown hotkey from configuration
 * @param hotkey Output buffer for hotkey value
 */
void ReadCustomCountdownHotkey(WORD* hotkey);

/**
 * @brief Write all hotkey assignments to configuration
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
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey);

/**
 * @brief Write generic key-value pair to configuration
 * @param key Configuration key name
 * @param value Configuration value
 */
void WriteConfigKeyValue(const char* key, const char* value);

/**
 * @brief Check if desktop shortcut creation check is done
 * @return true if check completed, false otherwise
 */
bool IsShortcutCheckDone(void);

/**
 * @brief Set desktop shortcut creation check status
 * @param done true if check completed
 */
void SetShortcutCheckDone(bool done);

/**
 * @brief Read string value from INI file
 * @param section INI section name
 * @param key INI key name
 * @param defaultValue Default value if key not found
 * @param returnValue Buffer to store retrieved value
 * @param returnSize Size of return buffer
 * @param filePath Path to INI file
 * @return Number of characters copied to buffer
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

/**
 * @brief Write string value to INI file
 * @param section INI section name
 * @param key INI key name
 * @param value Value to write
 * @param filePath Path to INI file
 * @return TRUE on success, FALSE on failure
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

/**
 * @brief Read integer value from INI file
 * @param section INI section name
 * @param key INI key name
 * @param defaultValue Default value if key not found
 * @param filePath Path to INI file
 * @return Retrieved integer value
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

/**
 * @brief Write integer value to INI file
 * @param section INI section name
 * @param key INI key name
 * @param value Value to write
 * @param filePath Path to INI file
 * @return TRUE on success, FALSE on failure
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

/**
 * @brief Check if this is the first run of the application
 * @return TRUE if first run, FALSE otherwise
 */
BOOL IsFirstRun(void);

/**
 * @brief Set first run flag to FALSE
 */
void SetFirstRunCompleted(void);

/**
 * @brief Set font license agreement acceptance status
 * @param accepted TRUE if user accepted the license agreement, FALSE otherwise
 */
void SetFontLicenseAccepted(BOOL accepted);

/**
 * @brief Set font license version acceptance status
 * @param version Version string that was accepted
 */
void SetFontLicenseVersionAccepted(const char* version);

/**
 * @brief Check if font license version needs acceptance
 * @return TRUE if current version needs user acceptance, FALSE otherwise
 */
BOOL NeedsFontLicenseVersionAcceptance(void);

/**
 * @brief Get current font license version
 * @return Current font license version string
 */
const char* GetCurrentFontLicenseVersion(void);

/**
 * @brief Write time format setting to config file
 * @param format Time format type to set
 */
void WriteConfigTimeFormat(TimeFormatType format);

/**
 * @brief Write milliseconds display setting to config file
 * @param showMilliseconds TRUE to show milliseconds, FALSE to hide
 */
void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/**
 * @brief Get appropriate timer interval based on milliseconds display setting
 * @return Timer interval in milliseconds (1ms if showing milliseconds, 1000ms otherwise)
 */
UINT GetTimerInterval(void);

/**
 * @brief Reset timer with appropriate interval based on milliseconds display setting
 * @param hwnd Window handle
 */
void ResetTimerWithInterval(HWND hwnd);

/**
 * @brief Force flush configuration changes to disk immediately
 */
void FlushConfigToDisk(void);

#endif