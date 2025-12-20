/**
 * @file startup.c
 * @brief Windows startup integration with table-driven mode configs
 */
#include "startup.h"
#include "config.h"
#include "timer/timer.h"
#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <string.h>

#define STARTUP_LINK_FILENAME L"Catime.lnk"
#define STARTUP_CMD_ARG L"--startup"
#define CONFIG_KEY_STARTUP_MODE "STARTUP_MODE="
#define STARTUP_MODE_MAX_LEN 20
#define CONFIG_LINE_BUFFER_SIZE 256
#define MODE_NAME_COUNT_UP "COUNT_UP"
#define MODE_NAME_SHOW_TIME "SHOW_TIME"
#define MODE_NAME_NO_DISPLAY "NO_DISPLAY"
#define MODE_NAME_DEFAULT "DEFAULT"

typedef struct {
    IShellLinkW* shellLink;
    IPersistFile* persistFile;
    BOOL initialized;
} ComShellLink;

/** Table-driven design eliminates if-else chains */
typedef struct {
    const char* modeName;
    BOOL showCurrentTime;
    BOOL countUp;
    BOOL enableTimer;
    int totalTime;
    const char* description;
} StartupModeConfig;

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

/** PathCombineW prevents buffer overflows vs sprintf */
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

static BOOL GetExecutablePath(wchar_t* output, size_t outputSize) {
    DWORD result = GetModuleFileNameW(NULL, output, (DWORD)outputSize);
    if (result == 0 || result >= outputSize) {
        LOG_ERROR("Failed to get executable path");
        return FALSE;
    }
    return TRUE;
}

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

/** Centralizes timer reset to avoid repetition - uses high-precision timer */
static void RestartTimer(HWND hwnd, UINT interval) {
    extern void MainTimer_SetInterval(UINT intervalMs);
    extern BOOL MainTimer_IsHighPrecision(void);
    
    if (MainTimer_IsHighPrecision()) {
        MainTimer_SetInterval(interval);
    } else {
        KillTimer(hwnd, TIMER_ID_MAIN);
        SetTimer(hwnd, TIMER_ID_MAIN, interval, NULL);
    }
}

static void StopTimer(HWND hwnd) {
    extern void MainTimer_Cleanup(void);
    extern BOOL MainTimer_IsHighPrecision(void);
    
    if (MainTimer_IsHighPrecision()) {
        /* Don't cleanup entirely, just set very long interval */
        extern void MainTimer_SetInterval(UINT intervalMs);
        MainTimer_SetInterval(60000);  /* 1 minute */
    } else {
        KillTimer(hwnd, TIMER_ID_MAIN);
    }
}

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

/** Unified mode application via data-driven config */
static void ApplyModeConfig(HWND hwnd, const StartupModeConfig* config) {
    LOG_INFO("Applying startup mode: %s - %s", config->modeName, config->description);
    
    CLOCK_SHOW_CURRENT_TIME = config->showCurrentTime;
    CLOCK_COUNT_UP = config->countUp;
    CLOCK_TOTAL_TIME = (config->totalTime == -1) ? g_AppConfig.timer.default_start_time : config->totalTime;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    
    extern void ResetMillisecondAccumulator(void);
    ResetMillisecondAccumulator();
    
    if (config->enableTimer) {
        RestartTimer(hwnd, GetTimerInterval());
    } else {
        StopTimer(hwnd);
    }
}

static const StartupModeConfig* FindModeConfig(const char* modeName) {
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(modeName, STARTUP_MODE_CONFIGS[i].modeName) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    return NULL;
}

static const StartupModeConfig* GetDefaultModeConfig(void) {
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(STARTUP_MODE_CONFIGS[i].modeName, MODE_NAME_DEFAULT) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    return &STARTUP_MODE_CONFIGS[0];
}

BOOL IsAutoStartEnabled(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (!GetStartupShortcutPath(startupPath, MAX_PATH)) {
        return FALSE;
    }
    
    BOOL exists = (GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES);
    LOG_INFO("Startup shortcut %s", exists ? "exists" : "does not exist");
    
    return exists;
}

/** --startup argument enables startup behavior detection */
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
    
    hr = link.shellLink->lpVtbl->SetPath(link.shellLink, exePath);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut path, hr=0x%08X", (unsigned int)hr);
        CleanupComShellLink(&link);
        return FALSE;
    }
    
    hr = link.shellLink->lpVtbl->SetArguments(link.shellLink, STARTUP_CMD_ARG);
    if (FAILED(hr)) {
        LOG_WARNING("Failed to set shortcut arguments, hr=0x%08X", (unsigned int)hr);
    }
    
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

/** Recreates shortcut to handle app relocations */
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

/** Reads STARTUP_MODE from config, falls back to DEFAULT if not found */
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
    InvalidateRect(hwnd, NULL, TRUE);
}
