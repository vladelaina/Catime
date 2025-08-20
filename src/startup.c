/**
 * @file startup.c
 * @brief Windows startup integration and application launch mode management
 * Handles automatic startup shortcut creation and startup behavior configuration
 */
#include "../include/startup.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <stdio.h>
#include <string.h>
#include "../include/config.h"
#include "../include/timer.h"

/** @brief Startup folder CSIDL constant for older Windows versions */
#ifndef CSIDL_STARTUP
#define CSIDL_STARTUP 0x0007
#endif

/** @brief Shell Link COM class ID declaration for older compilers */
#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

/** @brief Shell Link interface ID declaration for older compilers */
#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

/** @brief Global timer state variables accessed by startup mode configuration */
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern int CLOCK_DEFAULT_START_TIME;
extern char CLOCK_STARTUP_MODE[20];

/**
 * @brief Check if application is configured to start automatically with Windows
 * @return TRUE if startup shortcut exists in user's Startup folder
 */
BOOL IsAutoStartEnabled(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        wcscat(startupPath, L"\\Catime.lnk");
        return GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES;
    }
    return FALSE;
}

/**
 * @brief Create startup shortcut in Windows Startup folder
 * @return TRUE if shortcut creation succeeded
 * Creates shortcut with --startup argument for startup behavior detection
 */
BOOL CreateShortcut(void) {
    wchar_t startupPath[MAX_PATH];
    wchar_t exePath[MAX_PATH];
    IShellLinkW* pShellLink = NULL;
    IPersistFile* pPersistFile = NULL;
    BOOL success = FALSE;
    
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        wcscat(startupPath, L"\\Catime.lnk");
        
        /** Create COM object for shortcut creation */
        HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    &IID_IShellLinkW, (void**)&pShellLink);
        if (SUCCEEDED(hr)) {
            hr = pShellLink->lpVtbl->SetPath(pShellLink, exePath);
            if (SUCCEEDED(hr)) {
                /** Add --startup flag to distinguish startup launches */
                pShellLink->lpVtbl->SetArguments(pShellLink, L"--startup");
                hr = pShellLink->lpVtbl->QueryInterface(pShellLink,
                                                      &IID_IPersistFile,
                                                      (void**)&pPersistFile);
                if (SUCCEEDED(hr)) {
                    hr = pPersistFile->lpVtbl->Save(pPersistFile, startupPath, TRUE);
                    if (SUCCEEDED(hr)) {
                        success = TRUE;
                    }
                    pPersistFile->lpVtbl->Release(pPersistFile);
                }
            }
            pShellLink->lpVtbl->Release(pShellLink);
        }
    }
    
    return success;
}

/**
 * @brief Remove startup shortcut from Windows Startup folder
 * @return TRUE if shortcut removal succeeded or shortcut didn't exist
 */
BOOL RemoveShortcut(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        wcscat(startupPath, L"\\Catime.lnk");
        
        return DeleteFileW(startupPath);
    }
    return FALSE;
}

/**
 * @brief Update existing startup shortcut to current executable location
 * @return TRUE if update succeeded or no shortcut exists
 * Recreates shortcut if one exists to ensure it points to current executable
 */
BOOL UpdateStartupShortcut(void) {
    if (IsAutoStartEnabled()) {
        RemoveShortcut();
        return CreateShortcut();
    }
    return TRUE;
}

/**
 * @brief Apply configured startup mode behavior to timer application
 * @param hwnd Main window handle for timer operations
 * Reads STARTUP_MODE from config and sets application state accordingly:
 * - COUNT_UP: Start in count-up timer mode
 * - SHOW_TIME: Display current system time
 * - NO_DISPLAY: Hide timer and disable updates
 * - Default: Standard countdown timer mode
 */
void ApplyStartupMode(HWND hwnd) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    /** Read startup mode from configuration file */
    FILE *configFile = fopen(configPath, "r");
    if (configFile) {
        char line[256];
        while (fgets(line, sizeof(line), configFile)) {
            if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                sscanf(line, "STARTUP_MODE=%19s", CLOCK_STARTUP_MODE);
                break;
            }
        }
        fclose(configFile);
        
        /** Configure application state based on startup mode */
        if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
            /** Count-up timer: start from zero and count upward */
            CLOCK_COUNT_UP = TRUE;
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
            /** Clock mode: display current system time */
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_COUNT_UP = FALSE;
            
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
            /** Hidden mode: disable timer and hide window */
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            
            KillTimer(hwnd, 1);
            
        } else {
            /** Default countdown mode: standard timer functionality */
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            
            CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
            countdown_elapsed_time = 0;
            
            if (CLOCK_TOTAL_TIME > 0) {
                KillTimer(hwnd, 1);
                SetTimer(hwnd, 1, 1000, NULL);
            }
        }
        
        /** Trigger window repaint to reflect mode changes */
        InvalidateRect(hwnd, NULL, TRUE);
    }
}