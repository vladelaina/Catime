/**
 * @file config_core.c
 * @brief Configuration coordinator - high-level API for config operations
 * 
 * Simplified coordinator that delegates to specialized modules:
 * - config_defaults: Default values and metadata
 * - config_loader: Reading and parsing INI files
 * - config_applier: Applying config to global variables
 * - config_writer: Collecting and writing config
 * 
 * This file maintains backward compatibility with the original API.
 */
#include "config.h"
#include "config/config_defaults.h"
#include "config/config_loader.h"
#include "config/config_applier.h"
#include "config/config_writer.h"
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
            .timeout_ms = 3000,  /* 3s provides sufficient time to read without being intrusive */
            .max_opacity = 95,   /* 95% prevents complete occlusion while maintaining visibility */
            .type = NOTIFICATION_TYPE_CATIME,
            .disabled = FALSE,
            .window_x = -1,      /* -1 means auto-calculate position */
            .window_y = -1,
            .window_width = 0,   /* 0 means auto-calculate based on text */
            .window_height = 0
        },
        .sound = {
            .sound_file = "",
            .volume = 100
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
        .opacity_step_normal = 1,
        .opacity_step_fast = 5
    },
    .timer = {
        .default_start_time = 300  /* 5 minutes is a common Pomodoro short timer duration */
    },
    .last_config_time = 0
};

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
        CreateDefaultConfig(config_path);
    }

    BOOL needsWriteBack = FALSE;

    /* Version check - migrate if version mismatch */
    char version[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "",
                 version, sizeof(version), config_path);

    if (strcmp(version, CATIME_VERSION) != 0) {
        MigrateConfig(config_path);
        needsWriteBack = TRUE;
    }

    /* Load configuration into snapshot */
    ConfigSnapshot snapshot;
    if (!LoadConfigFromFile(config_path, &snapshot)) {
        /* Fallback to defaults on load failure */
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
        WriteConfig(config_path);
    }
}

/**
 * @brief Write complete configuration to file (Coordinator)
 * 
 * @details
 * Delegates to config_writer module.
 */
/* WriteConfig is now in config_writer module */
/* Re-exported here for backward compatibility */

/**
 * @brief Create default configuration (Re-exported)
 * 
 * @details
 * Implemented in config_defaults module.
 * Re-exported here for backward compatibility.
 */
/* CreateDefaultConfig is in config_defaults module */

/* ============================================================================
 * Utility functions kept for backward compatibility
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
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_FILE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", filePath);
}

/**
 * @brief Configure timeout action to open website
 */
void WriteConfigTimeoutWebsite(const char* url) {
    if (!url) url = "";
    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_WEBSITE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_WEBSITE", url);
}
