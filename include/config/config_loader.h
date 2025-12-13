/**
 * @file config_loader.h
 * @brief Configuration loading and parsing
 * 
 * Reads configuration from INI files and stores in structured snapshot.
 * Separated from application logic for better testability.
 */

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <windows.h>
#include "config.h"
#include "timer/timer.h"
#include "font.h"
#include "color/color.h"

/* ============================================================================
 * Configuration snapshot structure
 * ============================================================================ */

/**
 * @brief Complete configuration snapshot
 * 
 * @details
 * Intermediate storage between INI file and global variables.
 * Allows validation before application.
 */
typedef struct {
    /* General */
    char language[32];
    BOOL fontLicenseAccepted;
    char fontLicenseVersion[16];
    
    /* Display */
    char textColor[COLOR_HEX_BUFFER];
    int baseFontSize;
    char fontFileName[MAX_PATH];
    char fontInternalName[256];
    int windowPosX;
    int windowPosY;
    float windowScale;
    float pluginScale;
    BOOL windowTopmost;
    int windowOpacity;
    int moveStepSmall;
    int moveStepLarge;
    int opacityStepNormal;
    int opacityStepFast;
    int scaleStepNormal;
    int scaleStepFast;
    int textEffect;  /* TextEffectType enum value */

    /* Timer */
    int defaultStartTime;
    BOOL use24Hour;
    BOOL showSeconds;
    TimeFormatType timeFormat;
    BOOL showMilliseconds;
    char startupMode[20];
    
    /* Timeout action */
    TimeoutActionType timeoutAction;
    char timeoutText[50];
    char timeoutFilePath[MAX_PATH];
    wchar_t timeoutWebsiteUrl[MAX_PATH];
    
    /* Quick countdown presets */
    int timeOptions[MAX_TIME_OPTIONS];
    int timeOptionsCount;
    
    /* Pomodoro */
    int pomodoroTimes[10];
    int pomodoroTimesCount;
    int pomodoroLoopCount;
    
    /* Notifications */
    char timeoutMessage[100];
    int notificationTimeoutMs;
    int notificationMaxOpacity;
    NotificationType notificationType;
    char notificationSoundFile[MAX_PATH];
    int notificationSoundVolume;
    BOOL notificationDisabled;
    int notificationWindowX;
    int notificationWindowY;
    int notificationWindowWidth;
    int notificationWindowHeight;
    
    /* Colors */
    char colorOptions[2048];
    
    /* Hotkeys */
    WORD hotkeyShowTime;
    WORD hotkeyCountUp;
    WORD hotkeyCountdown;
    WORD hotkeyQuickCountdown1;
    WORD hotkeyQuickCountdown2;
    WORD hotkeyQuickCountdown3;
    WORD hotkeyPomodoro;
    WORD hotkeyToggleVisibility;
    WORD hotkeyEditMode;
    WORD hotkeyPauseResume;
    WORD hotkeyRestartTimer;
    WORD hotkeyCustomCountdown;
    WORD hotkeyToggleMilliseconds;
    
    /* Recent files */
    RecentFile recentFiles[MAX_RECENT_FILES];
    int recentFilesCount;
    
} ConfigSnapshot;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Load configuration from INI file into snapshot
 * @param config_path Path to config.ini (UTF-8)
 * @param snapshot Output snapshot structure
 * @return TRUE on success, FALSE on error
 * 
 * @details
 * Reads all configuration items from INI file.
 * Does not apply to global variables (use config_applier for that).
 * Uses defaults from config_defaults for missing values.
 */
BOOL LoadConfigFromFile(const char* config_path, ConfigSnapshot* snapshot);

/**
 * @brief Validate configuration snapshot
 * @param snapshot Snapshot to validate
 * @return TRUE if valid, FALSE if critical errors found
 * 
 * @details
 * Performs range checks, sanitization, and security validation.
 * Automatically corrects out-of-range values.
 * 
 * Security checks:
 * - Filters dangerous timeout actions (SHUTDOWN/RESTART/SLEEP)
 * - Validates opacity range (0-100)
 * - Validates volume range (0-100)
 * - Checks file existence for file paths
 */
BOOL ValidateConfigSnapshot(ConfigSnapshot* snapshot);

/**
 * @brief Initialize snapshot with default values
 * @param snapshot Snapshot to initialize
 * 
 * @details
 * Sets all fields to defaults from config_defaults module.
 * Useful for testing or resetting configuration.
 */
void InitializeDefaultSnapshot(ConfigSnapshot* snapshot);

#endif /* CONFIG_LOADER_H */

