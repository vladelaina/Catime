/**
 * @file config.h
 * @brief INI-based configuration system with atomic updates
 * 
 * Atomic writes (temp file + rename) prevent corruption during concurrent access.
 * UTF-8 throughout for international path/text support.
 * Mutex synchronization prevents race conditions between processes.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <time.h>
#include "../resource/resource.h"
#include "language.h"
#include "font.h"
#include "color/color.h"
#include "tray/tray.h"
#include "tray/tray_menu.h"
#include "timer/timer.h"
#include "window.h"
#include "startup.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief MRU list capacity */
#define MAX_RECENT_FILES 5

/* ============================================================================
 * Default configuration values
 * ============================================================================ */

/** @brief Default notification messages */
#define DEFAULT_TIMEOUT_MESSAGE        "Ding! Time's up~"

/** @brief Resource path prefixes */
#define LOCALAPPDATA_PREFIX            "%LOCALAPPDATA%\\Catime"
#define FONTS_PATH_PREFIX              "%LOCALAPPDATA%\\Catime\\resources\\fonts\\"
#define AUDIO_PATH_PREFIX              "%LOCALAPPDATA%\\Catime\\resources\\audio\\"
#define ANIMATIONS_PATH_PREFIX         "%LOCALAPPDATA%\\Catime\\resources\\animations\\"

/** @brief Default color values */
#define DEFAULT_TEXT_COLOR             "#648CFF_#64DC78"
#define DEFAULT_WHITE_COLOR            "#FFFFFF"
#define DEFAULT_BLACK_COLOR            "#000000"

/** @brief Default font settings */
#define DEFAULT_FONT_NAME              "Wallpoet Essence.ttf"
#define DEFAULT_FONT_SIZE              20

/** @brief Default window settings
 * - Position 960: Horizontal center on common 1920px wide displays
 * - Position -1: Near top of screen (right-upper area)
 */
#define DEFAULT_WINDOW_SCALE           "1.62"
#define DEFAULT_PLUGIN_SCALE           "1.0"
#define DEFAULT_WINDOW_POS_X           960
#define DEFAULT_WINDOW_POS_Y           -1
#define DEFAULT_MOVE_STEP_SMALL        10
#define DEFAULT_MOVE_STEP_LARGE        50
#define DEFAULT_SCALE_STEP_NORMAL      10
#define DEFAULT_SCALE_STEP_FAST        15

/** @brief Default color palette: 7 solid + 20 gradients, sorted by hue */
#define DEFAULT_COLOR_OPTIONS_INI \
    "#FFFFFF,#E3E3E5,#000000,#FF5F5F,#F6ABB7,#FB7FA4,#22C55E,#8771C6," \
    "#FF9A9E_#FECFEF,#FEA5B7_#FFDE9B,#A8EDEA_#FED6E3,#D299C2_#FEF9D7," \
    "#FF9966_#FF5E62,#ED4264_#FFEDBC,#F6D365_#FDA085,#FFE985_#FA742B,#FF9A56_#56CCBA," \
    "#10BD92_#8CE442,#11998E_#38EF7D,#43E97B_#38F9D7," \
    "#89F7FE_#66A6FF,#00C9FF_#92FE9D,#648CFF_#64DC78,#1F92A9_#EEE0D5," \
    "#8E9EF3_#F774A0,#FF5E96_#56C6FF,#30CFD0_#330867," \
    "#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF"

/** @brief INI section names for logical grouping */
#define INI_SECTION_GENERAL       "General"
#define INI_SECTION_DISPLAY       "Display"
#define INI_SECTION_TIMER         "Timer"
#define INI_SECTION_POMODORO      "Pomodoro"
#define INI_SECTION_NOTIFICATION  "Notification"
#define INI_SECTION_HOTKEYS       "Hotkeys"
#define INI_SECTION_RECENTFILES   "RecentFiles"
#define INI_SECTION_COLORS        "Colors"
#define INI_SECTION_OPTIONS       "Options"

/* ============================================================================
 * Type definitions
 * ============================================================================ */

/**
 * @brief MRU list entry
 */
typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

/**
 * @brief Notification display types
 */
typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,
    NOTIFICATION_TYPE_SYSTEM_MODAL,
    NOTIFICATION_TYPE_OS
} NotificationType;

/**
 * @brief Tray animation speed metric
 */
typedef enum {
    ANIMATION_SPEED_MEMORY = 0,
    ANIMATION_SPEED_CPU = 1,
    ANIMATION_SPEED_TIMER = 2
} AnimationSpeedMetric;

/**
 * @brief Time display format
 */
#ifndef TIME_FORMAT_TYPE_DEFINED
typedef enum {
    TIME_FORMAT_DEFAULT = 0,
    TIME_FORMAT_ZERO_PADDED = 1,
    TIME_FORMAT_FULL_PADDED = 2
} TimeFormatType;
#define TIME_FORMAT_TYPE_DEFINED
#endif

/* ============================================================================
 * Refactored configuration structures
 * ============================================================================ */

/**
 * @brief Recent files state
 */
typedef struct {
    RecentFile files[MAX_RECENT_FILES];
    int count;
} RecentFilesState;

/**
 * @brief Pomodoro configuration
 */
typedef struct {
    int work_time;
    int short_break;
    int long_break;
    int times[10];
    int times_count;
    int loop_count;
} PomodoroConfig;

/**
 * @brief Notification messages
 */
typedef struct {
    char timeout_message[100];
} NotificationMessages;

/**
 * @brief Notification display settings
 */
typedef struct {
    int timeout_ms;
    int max_opacity;
    NotificationType type;
    BOOL disabled;
    int window_x;
    int window_y;
    int window_width;
    int window_height;
} NotificationDisplay;

/**
 * @brief Notification sound settings
 */
typedef struct {
    char sound_file[MAX_PATH];
    int volume;
} NotificationSound;

/**
 * @brief Complete notification configuration
 */
typedef struct {
    NotificationMessages messages;
    NotificationDisplay display;
    NotificationSound sound;
} NotificationConfig;

/**
 * @brief Font license state
 */
typedef struct {
    BOOL accepted;
    char version_accepted[16];
} FontLicenseState;

/**
 * @brief Plugin trust entry
 */
typedef struct {
    char path[MAX_PATH];    /**< Plugin file path */
    char sha256[65];        /**< SHA256 hash of plugin file (hex string) */
} PluginTrustEntry;

/**
 * @brief Plugin trust state
 */
#define MAX_TRUSTED_PLUGINS 32
typedef struct {
    PluginTrustEntry entries[MAX_TRUSTED_PLUGINS];
    int count;
} PluginTrustState;

/**
 * @brief Time display format settings
 */
typedef struct {
    TimeFormatType format;
    BOOL show_milliseconds;
} TimeFormatConfig;

/**
 * @brief Display configuration
 */
typedef struct {
    TimeFormatConfig time_format;
    int move_step_small;
    int move_step_large;
    int opacity_step_normal;
    int opacity_step_fast;
    int scale_step_normal;
    int scale_step_fast;
    BOOL glow_effect;
    BOOL glass_effect;
    BOOL neon_effect;
    BOOL holographic_effect;
} DisplayConfig;

/**
 * @brief Timer state
 */
typedef struct {
    int default_start_time;
} TimerState;

/**
 * @brief Main application configuration
 * 
 * @details
 * Single source of truth for all configuration state.
 * Thread-safe when accessed through config functions.
 */
typedef struct {
    RecentFilesState recent_files;
    PomodoroConfig pomodoro;
    NotificationConfig notification;
    FontLicenseState font_license;
    PluginTrustState plugin_trust;
    DisplayConfig display;
    TimerState timer;
    time_t last_config_time;
} AppConfig;

/* ============================================================================
 * Global state variables
 * ============================================================================ */

/** @brief Global application configuration instance */
extern AppConfig g_AppConfig;

/* ============================================================================
 * Animation speed functions
 * ============================================================================ */

/**
 * @brief Get animation speed metric from config
 * @return Metric type (default: ANIMATION_SPEED_MEMORY)
 *
 * @details Reads [Animation] ANIMATION_SPEED_METRIC
 */
AnimationSpeedMetric GetAnimationSpeedMetric(void);

/**
 * @brief Set animation speed metric and persist to config
 * @param metric Metric type to set
 *
 * @details Updates [Animation] ANIMATION_SPEED_METRIC in config file
 */
void WriteConfigAnimationSpeedMetric(AnimationSpeedMetric metric);

/**
 * @brief Map utilization percent to animation speed scale
 * @param percent Utilization (0-100)
 * @return Speed scale (50.0=half, 100.0=normal, 200.0=double)
 * 
 * @details
 * Uses range mappings from [Animation] section.
 * Example: ANIMATION_SPEED_MAP_0-20 = 50 (0-20% usage → 50% speed)
 * Returns 100.0 if no mapping matches.
 */
double GetAnimationSpeedScaleForPercent(double percent);

/**
 * @brief Reload animation mappings from config
 * 
 * @details Thread-safe. Call after config changes.
 */
void ReloadAnimationSpeedFromConfig(void);

/**
 * @brief Write animation speed settings to config
 * @param config_path Path to config file
 */
void WriteAnimationSpeedToConfig(const char* config_path);

/* ============================================================================
 * Core configuration functions
 * ============================================================================ */

/**
 * @brief Get config file path (auto-creates directory)
 * @param path Output buffer (UTF-8)
 * @param size Buffer size
 * 
 * @details Returns %APPDATA%\Catime\config.ini
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Check if file exists with UTF-8 path support
 * @param filePath File path (UTF-8)
 * @return TRUE if exists, FALSE otherwise
 */
BOOL FileExists(const char* filePath);

/**
 * @brief Extract filename from full path
 * @param path Full path (UTF-8)
 * @param name Output buffer for filename
 * @param nameSize Size of output buffer
 */
void ExtractFileName(const char* path, char* name, size_t nameSize);

/**
 * @brief Create resource folder structure
 * @details Creates resources, audio, fonts, animations folders
 */
void CheckAndCreateResourceFolders(void);

/**
 * @brief Update config key-value atomically (internal helper)
 */
BOOL UpdateConfigKeyValueAtomic(const char* section, const char* key, const char* value);

/**
 * @brief Update config integer value atomically (internal helper)
 */
BOOL UpdateConfigIntAtomic(const char* section, const char* key, int value);

/**
 * @brief Update config boolean value atomically (internal helper)
 */
BOOL UpdateConfigBoolAtomic(const char* section, const char* key, BOOL value);

/**
 * @brief Load all configuration with validation
 * 
 * @details
 * Loads all settings (language, timers, hotkeys, etc.).
 * Creates default config if missing.
 * Validates and sanitizes all values.
 * 
 * @note Call during app initialization
 */
void ReadConfig();

/**
 * @brief Create resource folders (idempotent)
 * 
 * @details Creates %APPDATA%\Catime\resources\{audio,fonts,animations}
 */
void CheckAndCreateAudioFolder();

/**
 * @brief Get animations folder path (auto-creates)
 * @param path Output buffer (UTF-8)
 * @param size Buffer size
 */
void GetAnimationsFolderPath(char* path, size_t size);

/**
 * @brief Get plugins folder path (auto-creates)
 * @param path Output buffer (UTF-8)
 * @param size Buffer size
 */
void GetPluginsFolderPath(char* path, size_t size);

/**
 * @brief Write timeout action (atomic, validates enum)
 * @param action Action string (MESSAGE, LOCK, SHUTDOWN, etc.)
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Write quick countdown values
 * @param options Comma-separated times (e.g., "5,10,15")
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Load recent files with validation
 * 
 * @details
 * Reads RecentFile1-5, validates existence, updates menu.
 */
void LoadRecentFiles(void);

/**
 * @brief Add file to MRU list (atomic)
 * @param filePath Path to add (UTF-8)
 * 
 * @details
 * Adds to top, removes duplicates, maintains limit, updates menu.
 */
void SaveRecentFile(const char* filePath);

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Convert UTF-8 to ANSI (caller must free)
 * @param utf8Str UTF-8 string
 * @return Allocated ANSI string or NULL on failure
 * 
 * @note For legacy API compatibility
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Create default config with language auto-detection
 * @param config_path Path (UTF-8)
 * 
 * @details
 * Creates default settings (25min timer, 25/5/15 Pomodoro, hotkeys, colors).
 * Only creates if doesn't exist.
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Write complete config atomically
 * @param config_path Path (UTF-8)
 * 
 * @details
 * Writes to temp file, then atomically renames. Mutex-protected.
 */
void WriteConfig(const char* config_path);

/* ============================================================================
 * Specific configuration writers (Pomodoro, Timer, Window)
 * ============================================================================ */

/**
 * @brief Write Pomodoro times atomically
 * @param work Work duration (seconds)
 * @param short_break Short break (seconds)
 * @param long_break Long break (seconds)
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

/**
 * @brief Write Pomodoro settings (4-parameter version)
 */
void WriteConfigPomodoroSettings(int work, int short_break, int long_break, int long_break2);

/**
 * @brief Write Pomodoro loop count (1-99)
 * @param loop_count Cycles before long break
 */
void WriteConfigPomodoroLoopCount(int loop_count);

/**
 * @brief Set timeout action to open file
 * @param filePath File path (UTF-8)
 */
void WriteConfigTimeoutFile(const char* filePath);

/**
 * @brief Write always-on-top setting (applies immediately)
 * @param topmost "TRUE" or "FALSE"
 */
void WriteConfigTopmost(const char* topmost);

/**
 * @brief Set timeout action to open URL
 * @param url URL (UTF-8, no validation)
 */
void WriteConfigTimeoutWebsite(const char* url);

/**
 * @brief Write custom Pomodoro quick times
 * @param times Time array (seconds)
 * @param count Array size
 */
void WriteConfigPomodoroTimeOptions(int* times, int count);

/* ============================================================================
 * Notification configuration functions
 * ============================================================================ */

/**
 * @brief Read notification message texts (with fallbacks)
 */
void ReadNotificationMessagesConfig(void);

/**
 * @brief Write notification timeout
 * @param timeout_ms Duration (recommended: 3000-10000)
 */
void WriteConfigNotificationTimeout(int timeout_ms);

/**
 * @brief Read notification timeout (default: 5000ms)
 */
void ReadNotificationTimeoutConfig(void);

/**
 * @brief Read notification opacity (default: 100, range: 0-100)
 */
void ReadNotificationOpacityConfig(void);

/**
 * @brief Write notification opacity (auto-clamped to 1-100)
 * @param opacity Opacity (100 = fully opaque)
 */
void WriteConfigNotificationOpacity(int opacity);

/**
 * @brief Write notification messages atomically (placeholder support)
 * @param timeout_msg Timeout message (UTF-8)
 */
void WriteConfigNotificationMessages(const char* timeout_msg);

/**
 * @brief Read notification type (default: CATIME)
 */
void ReadNotificationTypeConfig(void);

/**
 * @brief Write notification type (enum to string)
 * @param type Notification type
 */
void WriteConfigNotificationType(NotificationType type);

/**
 * @brief Read notification disabled flag (default: FALSE)
 */
void ReadNotificationDisabledConfig(void);

/**
 * @brief Write notification disabled setting
 * @param disabled TRUE to suppress all notifications
 */
void WriteConfigNotificationDisabled(BOOL disabled);

/**
 * @brief Write language setting (triggers UI reload)
 * @param language AppLanguage enum ID
 */
void WriteConfigLanguage(int language);

/**
 * @brief Get audio folder path (auto-creates)
 * @param path Output buffer (UTF-8)
 * @param size Buffer size
 */
void GetAudioFolderPath(char* path, size_t size);

/**
 * @brief Read notification sound path
 * 
 * @details
 * Supports absolute/relative paths, "SYSTEM_BEEP", or empty (no sound).
 */
void ReadNotificationSoundConfig(void);

/**
 * @brief Write notification sound path (no validation)
 * @param sound_file Path, "SYSTEM_BEEP", or empty (UTF-8)
 */
void WriteConfigNotificationSound(const char* sound_file);

/**
 * @brief Read notification volume (default: 50, range: 0-100)
 */
void ReadNotificationVolumeConfig(void);

/**
 * @brief Write notification volume (auto-clamped to 0-100)
 * @param volume Volume (0=mute, 100=max)
 */
void WriteConfigNotificationVolume(int volume);

/**
 * @brief Read notification window position and size
 */
void ReadNotificationWindowConfig(void);

/**
 * @brief Write notification window position and size
 * @param x X position (-1 for auto)
 * @param y Y position (-1 for auto)
 * @param width Width (0 for auto)
 * @param height Height (0 for auto)
 */
void WriteConfigNotificationWindow(int x, int y, int width, int height);

/* ============================================================================
 * Hotkey configuration functions
 * ============================================================================ */

/**
 * @brief Convert hotkey to string (e.g., "Ctrl+A")
 * @param hotkey Hotkey value (LOWORD=VK, HIWORD=modifiers)
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * 
 * @details Returns "None" for 0.
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

/**
 * @brief Parse hotkey string to WORD
 * @param str String like "Ctrl+Shift+A" or "None"
 * @return Hotkey value (LOWORD=VK, HIWORD=modifiers)
 * 
 * @details
 * Case-insensitive, tolerates whitespace and various separators.
 * Returns 0 for "None" or empty.
 */
WORD StringToHotkey(const char* str);

/**
 * @brief Read all 12 hotkeys from config (uses defaults if missing)
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey, WORD* toggleMillisecondsHotkey);

/**
 * @brief Read custom countdown hotkey
 * @param hotkey Output
 */
void ReadCustomCountdownHotkey(WORD* hotkey);

/**
 * @brief Write all 12 hotkeys atomically (0 = "None")
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey, WORD toggleMillisecondsHotkey);

/**
 * @brief Write key-value pair (auto-determines section, atomic)
 * @param key Key name
 * @param value Value
 * 
 * @note Prefer specific Write* functions when available
 */
void WriteConfigKeyValue(const char* key, const char* value);

/**
 * @brief Check if shortcut prompt shown (one-time dialog)
 * @return TRUE if done
 */
BOOL IsShortcutCheckDone(void);

/**
 * @brief Mark shortcut prompt as done
 * @param done TRUE to prevent future prompts
 */
void SetShortcutCheckDone(BOOL done);

/* ============================================================================
 * Low-level INI file I/O functions (UTF-8 support)
 * ============================================================================ */

/**
 * @brief Read INI string with UTF-8 support
 * @return Characters copied (excluding null)
 * 
 * @note Thread-safe but not process-safe without mutex
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

/**
 * @brief Write INI string with UTF-8 support (NOT atomic)
 * @return TRUE on success
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

/**
 * @brief Read INI integer (returns default on parse failure)
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

/**
 * @brief Read INI boolean (accepts TRUE/FALSE, 1/0, yes/no, case-insensitive)
 */
BOOL ReadIniBool(const char* section, const char* key, BOOL defaultValue, 
               const char* filePath);

/**
 * @brief Write INI integer (NOT atomic)
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

/**
 * @brief Key-value pair for batch INI updates
 */
typedef struct {
    const char* section;
    const char* key;
    const char* value;
} IniKeyValue;

/**
 * @brief Batch write multiple INI values atomically
 * 
 * @param filePath INI file path
 * @param updates Array of key-value pairs to update
 * @param count Number of updates
 * @return TRUE on success, FALSE on failure
 * 
 * @details
 * Single atomic operation for multiple key-value pairs.
 * Uses temp file + atomic rename. Mutex-protected.
 * Performance: ~300x faster than individual writes during drag operations.
 */
BOOL WriteIniMultipleAtomic(const char* filePath, const IniKeyValue* updates, size_t count);

/* ============================================================================
 * First-run and font license tracking
 * ============================================================================ */

/**
 * @brief Check first run status
 * @return TRUE if config missing or FIRST_RUN=TRUE
 */
BOOL IsFirstRun(void);

/**
 * @brief Mark first run complete
 */
void SetFirstRunCompleted(void);

/**
 * @brief Set font license acceptance
 * @param accepted TRUE if user accepted
 */
void SetFontLicenseAccepted(BOOL accepted);

/**
 * @brief Set accepted license version (for upgrade detection)
 * @param version Version string (e.g., "1.0")
 */
void SetFontLicenseVersionAccepted(const char* version);

/**
 * @brief Check if license needs re-acceptance
 * @return TRUE if version changed or no acceptance
 */
BOOL NeedsFontLicenseVersionAcceptance(void);

/**
 * @brief Get current license version
 * @return Hardcoded version string (do not free)
 */
const char* GetCurrentFontLicenseVersion(void);

/* ============================================================================
 * Time display and timer configuration
 * ============================================================================ */

/**
 * @brief Convert string to TimeFormatType enum
 */
TimeFormatType TimeFormatType_FromStr(const char* str);

/**
 * @brief Convert TimeFormatType enum to string
 */
const char* TimeFormatType_ToStr(TimeFormatType val);

/**
 * @brief Convert string to TimeoutActionType enum
 */
TimeoutActionType TimeoutActionType_FromStr(const char* str);

/**
 * @brief Convert TimeoutActionType enum to string
 */
const char* TimeoutActionType_ToStr(TimeoutActionType val);

/**
 * @brief Write time format (updates UI immediately)
 * @param format Format type
 */
void WriteConfigTimeFormat(TimeFormatType format);

/**
 * @brief Write centiseconds display setting (affects timer interval)
 * @param showMilliseconds TRUE for 10ms updates, FALSE for 1s
 * 
 * @details Changes timer frequency for performance (10ms vs 1000ms)
 */
void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/**
 * @brief Get timer interval based on centiseconds setting
 * @return 10ms if showing centiseconds, 1000ms otherwise
 * 
 * @details Performance optimization: only update 100x/sec when needed
 */
UINT GetTimerInterval(void);

/**
 * @brief Reset timer with correct interval (call after changing centiseconds)
 * @param hwnd Window handle
 */
void ResetTimerWithInterval(HWND hwnd);

/**
 * @brief Write startup mode (validates against known modes)
 * @param mode "COUNTDOWN", "COUNTUP", "SHOW_TIME", "NO_DISPLAY"
 */
void WriteConfigStartupMode(const char* mode);

/**
 * @brief Write window opacity setting
 * @param opacity Opacity value (0-100)
 */
void WriteConfigWindowOpacity(int opacity);

/**
 * @brief Write move step settings
 * @param small_step Small step size (pixels)
 * @param large_step Large step size (pixels)
 */
void WriteConfigMoveSteps(int small_step, int large_step);

/**
 * @brief Read opacity adjustment step for normal scroll (default: 1)
 * @return Step size (1-100)
 */
int ReadConfigOpacityStepNormal(void);

/**
 * @brief Read opacity adjustment step for Ctrl+scroll (default: 5)
 * @return Step size (1-100)
 */
int ReadConfigOpacityStepFast(void);

/**
 * @brief Write opacity adjustment steps
 * @param normal_step Normal scroll step (1-100)
 * @param fast_step Ctrl+scroll step (1-100)
 */
void WriteConfigOpacitySteps(int normal_step, int fast_step);

/**
 * @brief Read scale adjustment step for normal scroll (default: 10)
 * @return Step percentage (1-100)
 */
int ReadConfigScaleStepNormal(void);

/**
 * @brief Read scale adjustment step for Ctrl+scroll (default: 15)
 * @return Step percentage (1-100)
 */
int ReadConfigScaleStepFast(void);

/**
 * @brief Write scale adjustment steps
 * @param normal_step Normal scroll step percentage (1-100)
 * @param fast_step Ctrl+scroll step percentage (1-100)
 */
void WriteConfigScaleSteps(int normal_step, int fast_step);

/**
 * @brief Force flush pending writes (use sparingly)
 *
 * @details Performance impact. Use before shutdown or critical operations.
 */
void FlushConfigToDisk(void);

/* ============================================================================
 * Tray icon animation color configuration
 * ============================================================================ */

/**
 * @brief Read percent icon colors (hex or RGB, defaults: black/white)
 */
void ReadPercentIconColorsConfig(void);

/**
 * @brief Get percent icon text color
 * @return Foreground COLORREF
 * 
 * @note Call ReadPercentIconColorsConfig() first
 */
COLORREF GetPercentIconTextColor(void);

/**
 * @brief Get percent icon background color
 * @return Background COLORREF
 * 
 * @note Call ReadPercentIconColorsConfig() first
 */
COLORREF GetPercentIconBgColor(void);

#endif