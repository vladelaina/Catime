/**
 * @file startup.c
 * @brief Windows startup integration and application launch mode management
 * @version 3.0 - Comprehensive refactoring for security, modularity and maintainability
 * 
 * Handles automatic startup shortcut creation and startup behavior configuration
 * with data-driven design, improved security, and unified resource management.
 */
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/timer.h"
#include "../include/log.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Startup shortcut filename */
#define STARTUP_LINK_FILENAME L"Catime.lnk"

/** @brief Command line argument for startup detection */
#define STARTUP_CMD_ARG L"--startup"

/** @brief Configuration file key for startup mode */
#define CONFIG_KEY_STARTUP_MODE "STARTUP_MODE="

/** @brief Maximum startup mode name length */
#define STARTUP_MODE_MAX_LEN 20

/** @brief Main timer ID */
#define MAIN_TIMER_ID 1

/** @brief Configuration file line buffer size */
#define CONFIG_LINE_BUFFER_SIZE 256

/* Startup mode identifiers */
#define MODE_NAME_COUNT_UP "COUNT_UP"
#define MODE_NAME_SHOW_TIME "SHOW_TIME"
#define MODE_NAME_NO_DISPLAY "NO_DISPLAY"
#define MODE_NAME_DEFAULT "DEFAULT"

/* ============================================================================
 * External Global Variables
 * ============================================================================ */

extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern int CLOCK_DEFAULT_START_TIME;
extern char CLOCK_STARTUP_MODE[20];

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief COM Shell Link wrapper for RAII-style resource management
 */
typedef struct {
    IShellLinkW* shellLink;       /**< IShellLink interface pointer */
    IPersistFile* persistFile;    /**< IPersistFile interface pointer */
    BOOL initialized;             /**< Whether COM objects are valid */
} ComShellLink;

/**
 * @brief Startup mode configuration
 * Data-driven approach to eliminate if-else chains
 */
typedef struct {
    const char* modeName;         /**< Mode identifier string */
    BOOL showCurrentTime;         /**< Display current system time */
    BOOL countUp;                 /**< Enable count-up timer */
    BOOL enableTimer;             /**< Whether timer should run */
    int totalTime;                /**< Initial timer value (-1 = use default) */
    const char* description;      /**< Human-readable description */
} StartupModeConfig;

/* ============================================================================
 * Startup Mode Configuration Table (Data-Driven)
 * ============================================================================ */

static const StartupModeConfig STARTUP_MODE_CONFIGS[] = {
    { 
        MODE_NAME_COUNT_UP, 
        FALSE, TRUE, TRUE, 0,
        "Count-up timer from zero" 
    },
    { 
        MODE_NAME_SHOW_TIME, 
        TRUE, FALSE, TRUE, 0,
        "Display current system time" 
    },
    { 
        MODE_NAME_NO_DISPLAY, 
        FALSE, FALSE, FALSE, 0,
        "Hidden mode, no timer display" 
    },
    { 
        MODE_NAME_DEFAULT, 
        FALSE, FALSE, TRUE, -1,
        "Standard countdown timer with default time" 
    },
};

static const size_t STARTUP_MODE_COUNT = sizeof(STARTUP_MODE_CONFIGS) / sizeof(STARTUP_MODE_CONFIGS[0]);

/* ============================================================================
 * Helper Functions - Path Operations
 * ============================================================================ */

/**
 * @brief Get startup folder path with shortcut filename appended (safe)
 * @param output Output buffer for complete path
 * @param outputSize Size of output buffer in wchar_t
 * @return TRUE on success
 * 
 * Uses PathCombineW for safe path concatenation
 */
static BOOL GetStartupShortcutPath(wchar_t* output, size_t outputSize) {
    wchar_t startupFolder[MAX_PATH];
    HRESULT hr;
    
    hr = SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupFolder);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get startup folder path, hr=0x%08X", (unsigned int)hr);
        return FALSE;
    }
    
    if (!PathCombineW(output, startupFolder, STARTUP_LINK_FILENAME)) {
        LOG_ERROR("Failed to combine startup path");
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief Get current executable path
 * @param output Output buffer for executable path
 * @param outputSize Size of output buffer in wchar_t
 * @return TRUE on success
 */
static BOOL GetExecutablePath(wchar_t* output, size_t outputSize) {
    DWORD result = GetModuleFileNameW(NULL, output, (DWORD)outputSize);
    if (result == 0 || result >= outputSize) {
        LOG_ERROR("Failed to get executable path");
        return FALSE;
    }
    return TRUE;
}

/* ============================================================================
 * COM Shell Link Management
 * ============================================================================ */

/**
 * @brief Initialize COM Shell Link wrapper
 * @param link Pointer to ComShellLink structure
 * @return TRUE on success
 */
static BOOL InitComShellLink(ComShellLink* link) {
    HRESULT hr;
    
    link->shellLink = NULL;
    link->persistFile = NULL;
    link->initialized = FALSE;
    
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&link->shellLink);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return FALSE;
    }
    
    hr = link->shellLink->lpVtbl->QueryInterface(link->shellLink, 
                                                  &IID_IPersistFile, 
                                                  (void**)&link->persistFile);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
        return FALSE;
    }
    
    link->initialized = TRUE;
    return TRUE;
}

/**
 * @brief Cleanup COM Shell Link wrapper
 * @param link Pointer to ComShellLink structure
 */
static void CleanupComShellLink(ComShellLink* link) {
    if (link->persistFile) {
        link->persistFile->lpVtbl->Release(link->persistFile);
        link->persistFile = NULL;
    }
    if (link->shellLink) {
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
    }
    link->initialized = FALSE;
}

/* ============================================================================
 * Timer Management Helper
 * ============================================================================ */

/**
 * @brief Restart timer with specified interval
 * @param hwnd Window handle
 * @param interval Timer interval in milliseconds
 * 
 * Centralizes timer reset logic to avoid repetition
 */
static void RestartTimer(HWND hwnd, UINT interval) {
    KillTimer(hwnd, MAIN_TIMER_ID);
    SetTimer(hwnd, MAIN_TIMER_ID, interval, NULL);
}

/**
 * @brief Stop timer
 * @param hwnd Window handle
 */
static void StopTimer(HWND hwnd) {
    KillTimer(hwnd, MAIN_TIMER_ID);
}

/* ============================================================================
 * Configuration File Operations
 * ============================================================================ */

/**
 * @brief Read startup mode from configuration file
 * @param modeName Output buffer for mode name
 * @param modeNameSize Size of output buffer
 * @return TRUE if mode was read successfully
 */
static BOOL ReadStartupModeConfig(char* modeName, size_t modeNameSize) {
    char configPath[MAX_PATH];
    wchar_t wconfigPath[MAX_PATH];
    FILE* configFile;
    char line[CONFIG_LINE_BUFFER_SIZE];
    BOOL found = FALSE;
    
    GetConfigPath(configPath, MAX_PATH);
    
    MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH);
    
    configFile = _wfopen(wconfigPath, L"r");
    if (!configFile) {
        LOG_WARNING("Failed to open config file for reading startup mode");
        return FALSE;
    }
    
    while (fgets(line, sizeof(line), configFile)) {
        if (strncmp(line, CONFIG_KEY_STARTUP_MODE, strlen(CONFIG_KEY_STARTUP_MODE)) == 0) {
            if (sscanf(line, "STARTUP_MODE=%19s", modeName) == 1) {
                found = TRUE;
                LOG_INFO("Read startup mode from config: %s", modeName);
                break;
            }
        }
    }
    
    fclose(configFile);
    return found;
}

/* ============================================================================
 * Startup Mode Application (Data-Driven)
 * ============================================================================ */

/**
 * @brief Apply startup mode configuration to application state
 * @param hwnd Window handle for timer operations
 * @param config Startup mode configuration to apply
 * 
 * Unified mode application logic using data-driven configuration
 */
static void ApplyModeConfig(HWND hwnd, const StartupModeConfig* config) {
    LOG_INFO("Applying startup mode: %s - %s", config->modeName, config->description);
    
    // Set global state flags
    CLOCK_SHOW_CURRENT_TIME = config->showCurrentTime;
    CLOCK_COUNT_UP = config->countUp;
    
    // Set timer value
    if (config->totalTime == -1) {
        // Use default time
        CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
    } else {
        CLOCK_TOTAL_TIME = config->totalTime;
    }
    
    // Reset elapsed time counters
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    
    // Manage timer state
    if (config->enableTimer) {
        RestartTimer(hwnd, GetTimerInterval());
    } else {
        StopTimer(hwnd);
    }
}

/**
 * @brief Find startup mode configuration by name
 * @param modeName Mode name to search for
 * @return Pointer to configuration or NULL if not found
 */
static const StartupModeConfig* FindModeConfig(const char* modeName) {
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(modeName, STARTUP_MODE_CONFIGS[i].modeName) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    return NULL;
}

/**
 * @brief Get default startup mode configuration
 * @return Pointer to default configuration
 */
static const StartupModeConfig* GetDefaultModeConfig(void) {
    // Return the DEFAULT mode (should be last in array)
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(STARTUP_MODE_CONFIGS[i].modeName, MODE_NAME_DEFAULT) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    // Fallback to first config if DEFAULT not found
    return &STARTUP_MODE_CONFIGS[0];
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Check if application is configured to start automatically with Windows
 * @return TRUE if startup shortcut exists in user's Startup folder
 */
BOOL IsAutoStartEnabled(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (!GetStartupShortcutPath(startupPath, MAX_PATH)) {
        return FALSE;
    }
    
    BOOL exists = (GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES);
    LOG_INFO("Startup shortcut %s", exists ? "exists" : "does not exist");
    
    return exists;
}

/**
 * @brief Create startup shortcut in Windows Startup folder
 * @return TRUE if shortcut creation succeeded
 * 
 * Creates shortcut with --startup argument for startup behavior detection
 */
BOOL CreateShortcut(void) {
    ComShellLink link;
    wchar_t startupPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    HRESULT hr;
    BOOL success = FALSE;
    
    LOG_INFO("Creating startup shortcut");
    
    if (!GetExecutablePath(exePath, MAX_PATH)) {
        return FALSE;
    }
    
    if (!GetStartupShortcutPath(startupPath, MAX_PATH)) {
        return FALSE;
    }
    
    if (!InitComShellLink(&link)) {
        return FALSE;
    }
    
    // Set executable path
    hr = link.shellLink->lpVtbl->SetPath(link.shellLink, exePath);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut path, hr=0x%08X", (unsigned int)hr);
        CleanupComShellLink(&link);
        return FALSE;
    }
    
    // Add startup argument
    hr = link.shellLink->lpVtbl->SetArguments(link.shellLink, STARTUP_CMD_ARG);
    if (FAILED(hr)) {
        LOG_WARNING("Failed to set shortcut arguments, hr=0x%08X", (unsigned int)hr);
    }
    
    // Save shortcut
    hr = link.persistFile->lpVtbl->Save(link.persistFile, startupPath, TRUE);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Startup shortcut created successfully: %ls", startupPath);
        success = TRUE;
    } else {
        LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
    }
    
    CleanupComShellLink(&link);
    return success;
}

/**
 * @brief Remove startup shortcut from Windows Startup folder
 * @return TRUE if shortcut removal succeeded or shortcut didn't exist
 */
BOOL RemoveShortcut(void) {
    wchar_t startupPath[MAX_PATH];
    
    LOG_INFO("Removing startup shortcut");
    
    if (!GetStartupShortcutPath(startupPath, MAX_PATH)) {
        return FALSE;
    }
    
    if (DeleteFileW(startupPath)) {
        LOG_INFO("Startup shortcut removed successfully");
        return TRUE;
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            LOG_INFO("Startup shortcut does not exist, nothing to remove");
            return TRUE;
        }
        LOG_ERROR("Failed to delete startup shortcut, error=%lu", error);
        return FALSE;
    }
}

/**
 * @brief Update existing startup shortcut to current executable location
 * @return TRUE if update succeeded or no shortcut exists
 * 
 * Recreates shortcut if one exists to ensure it points to current executable
 */
BOOL UpdateStartupShortcut(void) {
    LOG_INFO("Updating startup shortcut if exists");
    
    if (IsAutoStartEnabled()) {
        if (!RemoveShortcut()) {
            LOG_ERROR("Failed to remove old startup shortcut");
            return FALSE;
        }
        
        if (!CreateShortcut()) {
            LOG_ERROR("Failed to recreate startup shortcut");
            return FALSE;
        }
        
        LOG_INFO("Startup shortcut updated successfully");
        return TRUE;
    }
    
    LOG_INFO("No startup shortcut to update");
    return TRUE;
}

/**
 * @brief Apply configured startup mode behavior to timer application
 * @param hwnd Main window handle for timer operations
 * 
 * Reads STARTUP_MODE from config and applies corresponding configuration
 * using data-driven mode handlers. Falls back to DEFAULT mode if not found.
 */
void ApplyStartupMode(HWND hwnd) {
    char modeName[STARTUP_MODE_MAX_LEN] = {0};
    const StartupModeConfig* config = NULL;
    
    LOG_INFO("Applying startup mode configuration");
    
    if (ReadStartupModeConfig(modeName, sizeof(modeName))) {
        config = FindModeConfig(modeName);
        if (!config) {
            LOG_WARNING("Unknown startup mode '%s', using default", modeName);
            config = GetDefaultModeConfig();
        }
    } else {
        LOG_INFO("No startup mode configured, using default");
        config = GetDefaultModeConfig();
    }
    
    ApplyModeConfig(hwnd, config);
    
    // Trigger window repaint to reflect mode changes
    InvalidateRect(hwnd, NULL, TRUE);
}
