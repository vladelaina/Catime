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
#include "log.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Global configuration - single source of truth
 * ============================================================================ */

AppConfig g_AppConfig = {
    .recent_files = {
        .files = {{0}},
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
    
    LOG_INFO("Loading configuration from: %s", config_path);

    /* Create default config if missing */
    if (!FileExists(config_path)) {
        LOG_INFO("Configuration file not found, creating default configuration");
        CreateDefaultConfig(config_path);
    }

    BOOL needsWriteBack = FALSE;

    /* Version check - migrate if version mismatch */
    char version[32] = {0};
    
    LOG_DEBUG("DEBUG_TRACE: Attempting to read CONFIG_VERSION from file: %s", config_path);
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "",
                 version, sizeof(version), config_path);

    LOG_INFO("DEBUG_TRACE: Version Check Start --------------------------------");
    LOG_INFO("DEBUG_TRACE: Config Path: '%s'", config_path);
    LOG_INFO("DEBUG_TRACE: Config Version in File: '%s'", version[0] ? version : "<empty/missing>");
    LOG_INFO("DEBUG_TRACE: App Current Version:    '%s'", CATIME_VERSION);
    
    int versionMatch = strcmp(version, CATIME_VERSION);
    LOG_INFO("DEBUG_TRACE: strcmp result: %d (0 means match)", versionMatch);

    if (versionMatch != 0) {
        LOG_INFO("DEBUG_TRACE: >> Version MISMATCH detected. Entering logic block.");
        
        LOG_INFO("DEBUG_TRACE: Checking FORCE_CONFIG_RESET_ON_UPDATE macro value...");
        int forceResetValue = FORCE_CONFIG_RESET_ON_UPDATE;
        LOG_INFO("DEBUG_TRACE: FORCE_CONFIG_RESET_ON_UPDATE evaluates to: %d", forceResetValue);

        if (forceResetValue) {
            LOG_WARNING("DEBUG_TRACE: >> DECISION: FORCE RESET (Switch is ON)");
            LOG_INFO("DEBUG_TRACE: Preparing to delete old configuration file...");
            
            /* Delete the old configuration file */
            wchar_t wConfigPath[MAX_PATH] = {0};
            int conversionResult = MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wConfigPath, MAX_PATH);
            
            if (conversionResult == 0) {
                 LOG_ERROR("DEBUG_TRACE: Failed to convert config path to WideChar. Win32 Error: %lu", GetLastError());
            }

            // Check if file exists before deleting
            DWORD fileAttr = GetFileAttributesW(wConfigPath);
            if (fileAttr == INVALID_FILE_ATTRIBUTES) {
                 LOG_INFO("DEBUG_TRACE: GetFileAttributesW says file does not exist or cannot be accessed before deletion.");
            } else {
                 LOG_INFO("DEBUG_TRACE: File exists. Proceeding to DeleteFileW...");
            }

            if (DeleteFileW(wConfigPath)) {
                LOG_INFO("DEBUG_TRACE: SUCCESS: Old configuration file deleted.");
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_FILE_NOT_FOUND) {
                    LOG_INFO("DEBUG_TRACE: DeleteFileW result: File not found (already gone).");
                } else {
                    LOG_ERROR("DEBUG_TRACE: ERROR: Failed to delete file. Win32 Error Code: %lu", err);
                }
            }

            /* Create a fresh default configuration */
            LOG_INFO("DEBUG_TRACE: Calling CreateDefaultConfig to generate a fresh INI...");
            CreateDefaultConfig(config_path);
            
            LOG_INFO("DEBUG_TRACE: >> RESET PHASE 1 COMPLETE. Scheduling UI reset.");
            
            /* Mark for full factory reset after window creation */
            g_PerformFactoryReset = TRUE;
            
            /* DIRECTLY initialize and apply default snapshot */
            /* This bypasses LoadConfigFromFile to ensure we use pure default values in memory */
            ConfigSnapshot snapshot;
            InitializeDefaultSnapshot(&snapshot);
            
            /* Apply detected language if possible, matching CreateDefaultConfig logic */
            extern int DetectSystemLanguage(void);
            int detectedLang = DetectSystemLanguage();
            
            const char* langNames[] = {
                "Chinese_Simplified",
                "Chinese_Traditional",
                "English",
                "Spanish",
                "French",
                "German",
                "Russian",
                "Portuguese",
                "Japanese",
                "Korean"
            };
            
            /* Assuming enum values correspond to array indices 0-9 */
            if (detectedLang >= 0 && detectedLang < (int)(sizeof(langNames)/sizeof(langNames[0]))) {
                strncpy(snapshot.language, langNames[detectedLang], sizeof(snapshot.language) - 1);
                snapshot.language[sizeof(snapshot.language) - 1] = '\0';
            } else {
                strncpy(snapshot.language, "English", sizeof(snapshot.language) - 1);
                snapshot.language[sizeof(snapshot.language) - 1] = '\0';
            }
            
            LOG_INFO("DEBUG_TRACE: Applying pure default snapshot directly to memory...");
            ApplyConfigSnapshot(&snapshot);
            
            LOG_INFO("DEBUG_TRACE: Configuration reset and applied successfully.");
            return; /* EXIT FUNCTION HERE - Do not proceed to LoadConfigFromFile */
        } else {
            LOG_INFO("DEBUG_TRACE: >> DECISION: STANDARD MIGRATION (Switch is OFF)");
            LOG_INFO("DEBUG_TRACE: Calling MigrateConfig...");
            MigrateConfig(config_path);
            needsWriteBack = TRUE;
            LOG_INFO("DEBUG_TRACE: >> MIGRATION COMPLETE. needsWriteBack set to TRUE.");
        }
    } else {
        LOG_INFO("DEBUG_TRACE: >> Version MATCH detected. Skipping all migration/reset logic.");
        LOG_INFO("DEBUG_TRACE: Logic skipped because config version matches app version.");
    }
    LOG_INFO("DEBUG_TRACE: Version Check End ----------------------------------");

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
        LOG_INFO("Configuration validation found issues and auto-corrected them");
        needsWriteBack = TRUE;
    }

    /* Apply to global variables */
    ApplyConfigSnapshot(&snapshot);

    /* Write back if migration occurred or validation modified values */
    if (needsWriteBack) {
        LOG_INFO("Writing corrected configuration back to file");
        WriteConfig(config_path);
    }
    
    LOG_INFO("Configuration loading completed successfully");
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
void WriteConfigTimeoutAction(const char* action) {
    const char* actual_action = action;
    
    if (strcmp(action, "RESTART") == 0 || 
        strcmp(action, "SHUTDOWN") == 0 || 
        strcmp(action, "SLEEP") == 0) {
        return;
    }
    
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", actual_action);
}

/**
 * @brief Write time options configuration
 */
void WriteConfigTimeOptions(const char* options) {
    UpdateConfigKeyValueAtomic(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", options);
}

/**
 * @brief Write window topmost setting
 */
void WriteConfigTopmost(const char* topmost) {
    UpdateConfigKeyValueAtomic(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", topmost);
}

/**
 * @brief Configure timeout action to open file
 */
void WriteConfigTimeoutFile(const char* filePath) {
    if (!filePath) filePath = "";
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
    strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, MAX_PATH - 1);
    CLOCK_TIMEOUT_FILE_PATH[MAX_PATH - 1] = '\0';
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_FILE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", filePath);
}

/**
 * @brief Configure timeout action to open website
 */
void WriteConfigTimeoutWebsite(const char* url) {
    if (!url) url = "";
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    strncpy(CLOCK_TIMEOUT_WEBSITE_URL, url, MAX_PATH - 1);
    CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH - 1] = '\0';
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_WEBSITE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_WEBSITE", url);
}
