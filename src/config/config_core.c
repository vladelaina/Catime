/**
 * @file config_core.c
 * @brief Configuration coordinator - high-level API for config operations
 * 
 * Simplified coordinator that delegates to specialized modules:
 * - config_defaults: Default values and metadata
 * - config_loader: Reading and parsing INI files
 * - config_applier: Applying config to global variables
 * - config_writer: Collecting and writing config
 */
#include "config.h"
#include "config/config_defaults.h"
#include "config/config_loader.h"
#include "config/config_applier.h"
#include "config/config_writer.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Global configuration - single source of truth
 * ============================================================================ */

AppConfig g_AppConfig = {
    .recent_files = {
        .files = {{{0}}},
        .count = 0
    },
    .pomodoro = {
        .work_time = 0,
        .short_break = 0,
        .long_break = 0,
        .times = {0},
        .times_count = 0,
        .loop_count = 1
    },
    .notification = {
        .messages = {
            .timeout_message = DEFAULT_TIMEOUT_MESSAGE
        },
        .display = {
            .timeout_ms = DEFAULT_NOTIFICATION_TIMEOUT_MS,
            .max_opacity = DEFAULT_NOTIFICATION_MAX_OPACITY,
            .type = NOTIFICATION_TYPE_CATIME,
            .disabled = FALSE,
            .window_x = -1,
            .window_y = -1,
            .window_width = 0,
            .window_height = 0
        },
        .sound = {
            .sound_file = "",
            .volume = DEFAULT_NOTIFICATION_VOLUME
        }
    },
    .font_license = {
        .accepted = FALSE,
        .version_accepted = ""
    },
    .display = {
        .time_format = {
            .format = TIME_FORMAT_DEFAULT,
            .show_milliseconds = FALSE
        },
        .move_step_small = DEFAULT_MOVE_STEP_SMALL,
        .move_step_large = DEFAULT_MOVE_STEP_LARGE,
        .opacity_step_normal = MIN_OPACITY,
        .opacity_step_fast = 5,
        .scale_step_normal = DEFAULT_SCALE_STEP_NORMAL,
        .scale_step_fast = DEFAULT_SCALE_STEP_FAST
    },
    .timer = {
        .default_start_time = DEFAULT_QUICK_COUNTDOWN_3
    },
    .last_config_time = 0
};

/** @brief Global flag to trigger factory reset after window creation */
BOOL g_PerformFactoryReset = FALSE;

/* ============================================================================
 * Public API Implementation - delegates to specialized modules
 * ============================================================================ */

/**
 * @brief Read configuration from INI file (Coordinator)
 * 
 * @details
 * Orchestrates configuration loading through modular components:
 * 1. Load from file (config_loader)
 * 2. Validate snapshot (config_loader)
 * 3. Apply to globals (config_applier)
 */
void ReadConfig() {
    CheckAndCreateResourceFolders();

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    /* Create default config if missing */
    if (!FileExists(config_path)) {
        if (!CreateDefaultConfig(config_path)) {
            LOG_WARNING("Failed to create default configuration file; using in-memory defaults");
        }
    }

    BOOL needsWriteBack = FALSE;

    /* Version check - migrate if version mismatch */
    char version[32] = {0};
    
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "",
                 version, sizeof(version), config_path);

    int versionMatch = strcmp(version, CATIME_VERSION);

    if (versionMatch != 0) {
#if FORCE_CONFIG_RESET_ON_UPDATE
        {
            LOG_WARNING("DEBUG_TRACE: >> DECISION: FORCE RESET (Switch is ON)");
            
            /* Delete the old configuration file */
            wchar_t wConfigPath[MAX_PATH] = {0};
            int conversionResult = MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);
            
            if (conversionResult == 0) {
                 LOG_ERROR("DEBUG_TRACE: Failed to convert config path to WideChar. Win32 Error: %lu", GetLastError());
            } else {
                if (!DeleteFileW(wConfigPath)) {
                    DWORD err = GetLastError();
                    if (err != ERROR_FILE_NOT_FOUND) {
                        LOG_ERROR("DEBUG_TRACE: ERROR: Failed to delete file. Win32 Error Code: %lu", err);
                    }
                }
            }

            /* Create a fresh default configuration */
            if (!CreateDefaultConfig(config_path)) {
                LOG_WARNING("Failed to create default configuration file during forced reset");
            }
            
            /* Mark for full factory reset after window creation */
            g_PerformFactoryReset = TRUE;
            
            /* DIRECTLY initialize and apply default snapshot */
            /* This bypasses LoadConfigFromFile to ensure we use pure default values in memory */
            ConfigSnapshot snapshot;
            InitializeDefaultSnapshot(&snapshot);
            
            const char* detectedLanguage = GetDetectedSystemLanguageConfigKey();
            strncpy(snapshot.language, detectedLanguage,
                    sizeof(snapshot.language) - 1);
            snapshot.language[sizeof(snapshot.language) - 1] = '\0';
            
            ApplyConfigSnapshot(&snapshot);
            
            return; /* EXIT FUNCTION HERE - Do not proceed to LoadConfigFromFile */
        }
#else
        {
            MigrateConfig(config_path);
            needsWriteBack = TRUE;
        }
#endif
    }

    /* Load configuration into snapshot */
    /* NOTE: If we performed a FORCE RESET, we have already returned from the function above. */
    ConfigSnapshot snapshot;
    if (!LoadConfigFromFile(config_path, &snapshot)) {
        /* Fallback to defaults on load failure */
        LOG_WARNING("Failed to load configuration file, using default values");
        InitializeDefaultSnapshot(&snapshot);
    }

    /* Validate and sanitize - returns TRUE if any values were modified */
    if (ValidateConfigSnapshot(&snapshot)) {
        needsWriteBack = TRUE;
    }

    /* Apply to global variables */
    ApplyConfigSnapshot(&snapshot);

    /* Write back if migration occurred or validation modified values */
    if (needsWriteBack) {
        if (!WriteConfig(config_path)) {
            LOG_ERROR("Failed to write migrated or sanitized config: %s", config_path);
        }
    }
}

/* ============================================================================
 * Utility functions
 * ============================================================================ */

/**
 * @brief Write timeout action configuration
 * 
 * @note One-time actions (SHUTDOWN/RESTART/SLEEP) are not persisted to config.
 * They only affect the current session and will reset on next launch.
 */
static BOOL IsOneTimeTimeoutAction(TimeoutActionType action) {
    return action == TIMEOUT_ACTION_SHUTDOWN ||
           action == TIMEOUT_ACTION_RESTART ||
           action == TIMEOUT_ACTION_SLEEP;
}

static BOOL TimerConfigValueEqualsInFile(const char* configPath,
                                         const char* key,
                                         const char* expectedValue,
                                         const char* defaultValue) {
    char currentValue[MAX_PATH];

    if (!ReadIniStringExact(INI_SECTION_TIMER, key, defaultValue ? defaultValue : "",
                            currentValue, sizeof(currentValue), configPath)) {
        return FALSE;
    }

    return strcmp(currentValue, expectedValue ? expectedValue : "") == 0;
}

static void CopyTimeoutString(char* dest, size_t destSize, const char* value) {
    if (!dest || destSize == 0) {
        return;
    }

    if (!value) {
        value = "";
    }

    strncpy(dest, value, destSize - 1);
    dest[destSize - 1] = '\0';
}

BOOL WriteConfigTimeoutAction(const char* action) {
    TimeoutActionType newAction = TimeoutActionType_FromStr(action ? action : "MESSAGE");
    const char* configAction = TimeoutActionType_ToStr(newAction);
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    if (IsOneTimeTimeoutAction(newAction)) {
        BOOL configMatches = TimerConfigValueEqualsInFile(config_path,
                                                          "CLOCK_TIMEOUT_ACTION",
                                                          "MESSAGE",
                                                          "MESSAGE");
        if (!configMatches &&
            !UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE")) {
            return FALSE;
        }

        CLOCK_TIMEOUT_ACTION = newAction;
        Timer_ClearTimeoutSystemActionArm();
        return TRUE;
    }

    BOOL runtimeMatches = CLOCK_TIMEOUT_ACTION == newAction;
    BOOL configMatches = TimerConfigValueEqualsInFile(config_path,
                                                      "CLOCK_TIMEOUT_ACTION",
                                                      configAction,
                                                      "MESSAGE");

    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    if (!configMatches &&
        !UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", configAction)) {
        return FALSE;
    }

    CLOCK_TIMEOUT_ACTION = newAction;
    Timer_ClearTimeoutSystemActionArm();
    return TRUE;
}

/**
 * @brief Write time options configuration
 */
BOOL WriteConfigTimeOptions(const char* options) {
    if (!options) {
        return FALSE;
    }

    return UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", options);
}

BOOL WriteConfigDefaultCountdownStartup(int seconds) {
    if (seconds <= 0) {
        return FALSE;
    }

    char secondsStr[32];
    if (snprintf(secondsStr, sizeof(secondsStr), "%d", seconds) < 0) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentSeconds[32] = {0};
    char currentMode[sizeof(CLOCK_STARTUP_MODE)] = {0};
    ReadIniString(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", "",
                  currentSeconds, sizeof(currentSeconds), config_path);
    ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", "SHOW_TIME",
                  currentMode, sizeof(currentMode), config_path);

    BOOL runtimeMatches = g_AppConfig.timer.default_start_time == seconds &&
                          strcmp(CLOCK_STARTUP_MODE, "DEFAULT") == 0;
    BOOL configMatches = strcmp(currentSeconds, secondsStr) == 0 &&
                         strcmp(currentMode, "DEFAULT") == 0;
    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", secondsStr},
        {INI_SECTION_TIMER, "STARTUP_MODE", "DEFAULT"},
    };
    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return FALSE;
    }

    g_AppConfig.timer.default_start_time = seconds;
    strncpy(CLOCK_STARTUP_MODE, "DEFAULT", sizeof(CLOCK_STARTUP_MODE) - 1);
    CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';
    return TRUE;
}

/**
 * @brief Write window topmost setting
 */
BOOL WriteConfigTopmost(const char* topmost) {
    if (!topmost) {
        return FALSE;
    }

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char currentValue[16] = {0};
    ReadIniString(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", "",
                  currentValue, sizeof(currentValue), config_path);
    if (strcmp(currentValue, topmost) == 0) {
        return TRUE;
    }

    return UpdateConfigKeyValueAtomic(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", topmost);
}

/**
 * @brief Configure timeout action to open file
 */
BOOL WriteConfigTimeoutFile(const char* filePath) {
    char normalizedPath[MAX_PATH];
    CopyTimeoutString(normalizedPath, sizeof(normalizedPath), filePath);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE &&
                          strcmp(CLOCK_TIMEOUT_FILE_PATH, normalizedPath) == 0;
    BOOL configMatches = TimerConfigValueEqualsInFile(config_path,
                                                      "CLOCK_TIMEOUT_ACTION",
                                                      "OPEN_FILE",
                                                      "MESSAGE") &&
                         TimerConfigValueEqualsInFile(config_path,
                                                      "CLOCK_TIMEOUT_FILE",
                                                      normalizedPath,
                                                      "");

    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "OPEN_FILE"},
        {INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", normalizedPath}
    };

    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return FALSE;
    }

    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    CopyTimeoutString(CLOCK_TIMEOUT_FILE_PATH, MAX_PATH, normalizedPath);
    return TRUE;
}

/**
 * @brief Configure timeout action to open website
 */
BOOL WriteConfigTimeoutWebsite(const char* url) {
    char normalizedUrl[MAX_PATH];
    CopyTimeoutString(normalizedUrl, sizeof(normalizedUrl), url);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    BOOL runtimeMatches = CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_WEBSITE &&
                          strcmp(CLOCK_TIMEOUT_WEBSITE_URL, normalizedUrl) == 0;
    BOOL configMatches = TimerConfigValueEqualsInFile(config_path,
                                                      "CLOCK_TIMEOUT_ACTION",
                                                      "OPEN_WEBSITE",
                                                      "MESSAGE") &&
                         TimerConfigValueEqualsInFile(config_path,
                                                      "CLOCK_TIMEOUT_WEBSITE",
                                                      normalizedUrl,
                                                      "");

    if (runtimeMatches && configMatches) {
        return TRUE;
    }

    const IniKeyValue updates[] = {
        {INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "OPEN_WEBSITE"},
        {INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", normalizedUrl}
    };

    if (!configMatches &&
        !WriteIniMultipleAtomic(config_path, updates, sizeof(updates) / sizeof(updates[0]))) {
        return FALSE;
    }

    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    CopyTimeoutString(CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH, normalizedUrl);
    return TRUE;
}
