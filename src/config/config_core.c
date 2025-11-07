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
            .timeout_message = DEFAULT_TIMEOUT_MESSAGE,
            .pomodoro_message = DEFAULT_POMODORO_MESSAGE,
            .cycle_complete_message = DEFAULT_POMODORO_COMPLETE_MSG
        },
        .display = {
            .timeout_ms = 3000,  /* 3s provides sufficient time to read without being intrusive */
            .max_opacity = 95,   /* 95% prevents complete occlusion while maintaining visibility */
            .type = NOTIFICATION_TYPE_CATIME,
            .disabled = FALSE
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
        .preview = {
            .is_format_previewing = FALSE,
            .preview_format = TIME_FORMAT_DEFAULT,
            .is_milliseconds_previewing = FALSE,
            .preview_show_milliseconds = FALSE
        }
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
    
    /* Version check - recreate if version mismatch */
    char version[32] = {0};
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "", 
                 version, sizeof(version), config_path);
    
    if (strcmp(version, CATIME_VERSION) != 0) {
        CreateDefaultConfig(config_path);
    }
    
    /* Load configuration into snapshot */
    ConfigSnapshot snapshot;
    if (!LoadConfigFromFile(config_path, &snapshot)) {
        /* Fallback to defaults on load failure */
        InitializeDefaultSnapshot(&snapshot);
    }
    
    /* Validate and sanitize */
    ValidateConfigSnapshot(&snapshot);
    
    /* Apply to global variables */
    ApplyConfigSnapshot(&snapshot);
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
 */
void WriteConfigTimeoutAction(const char* action) {
    const char* actual_action = action;
    /* Security: Filter dangerous actions */
    if (strcmp(action, "RESTART") == 0 || 
        strcmp(action, "SHUTDOWN") == 0 || 
        strcmp(action, "SLEEP") == 0) {
        actual_action = "MESSAGE";
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
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_FILE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", filePath);
}

/**
 * @brief Configure timeout action to open website
 */
void WriteConfigTimeoutWebsite(const char* url) {
    if (!url) url = "";
    WriteConfigKeyValue("CLOCK_TIMEOUT_ACTION", "OPEN_WEBSITE");
    WriteConfigKeyValue("CLOCK_TIMEOUT_WEBSITE", url);
}
