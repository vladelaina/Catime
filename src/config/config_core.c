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
 * Global configuration variables - definitions
 * ============================================================================ */

char CLOCK_TIMEOUT_MESSAGE_TEXT[100] = DEFAULT_TIMEOUT_MESSAGE;
char POMODORO_TIMEOUT_MESSAGE_TEXT[100] = DEFAULT_POMODORO_MESSAGE;
char POMODORO_CYCLE_COMPLETE_TEXT[100] = DEFAULT_POMODORO_COMPLETE_MSG;

int NOTIFICATION_TIMEOUT_MS = 3000;
int NOTIFICATION_MAX_OPACITY = 95;
NotificationType NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
BOOL NOTIFICATION_DISABLED = FALSE;

char NOTIFICATION_SOUND_FILE[MAX_PATH] = "";
int NOTIFICATION_SOUND_VOLUME = 100;

BOOL FONT_LICENSE_ACCEPTED = FALSE;
char FONT_LICENSE_VERSION_ACCEPTED[16] = "";

TimeFormatType CLOCK_TIME_FORMAT = TIME_FORMAT_DEFAULT;
BOOL IS_TIME_FORMAT_PREVIEWING = FALSE;
TimeFormatType PREVIEW_TIME_FORMAT = TIME_FORMAT_DEFAULT;
BOOL CLOCK_SHOW_MILLISECONDS = FALSE;
BOOL IS_MILLISECONDS_PREVIEWING = FALSE;
BOOL PREVIEW_SHOW_MILLISECONDS = FALSE;

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
