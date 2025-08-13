/**
 * @file startup.c
 * @brief Implementation of auto-start functionality
 * 
 * This file implements the application's auto-start related functionality,
 * including checking if auto-start is enabled, creating and deleting auto-start shortcuts.
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

#ifndef CSIDL_STARTUP
#define CSIDL_STARTUP 0x0007
#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

/// @name External variable declarations
/// @{
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern int CLOCK_DEFAULT_START_TIME;
extern char CLOCK_STARTUP_MODE[20];
/// @}

/**
 * @brief Check if the application is set to auto-start at system boot
 * @return BOOL Returns TRUE if auto-start is enabled, otherwise FALSE
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
 * @brief Create auto-start shortcut
 * @return BOOL Returns TRUE if creation was successful, otherwise FALSE
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
        
        HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    &IID_IShellLinkW, (void**)&pShellLink);
        if (SUCCEEDED(hr)) {
            hr = pShellLink->lpVtbl->SetPath(pShellLink, exePath);
            if (SUCCEEDED(hr)) {
                // Mark this launch as system startup so the app can adjust behavior (e.g., topmost retries)
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
 * @brief Delete auto-start shortcut
 * @return BOOL Returns TRUE if deletion was successful, otherwise FALSE
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
 * @brief Update auto-start shortcut
 * 
 * Check if auto-start is enabled, if so, delete the old shortcut and create a new one,
 * ensuring that the auto-start functionality works correctly even if the application location changes.
 * 
 * @return BOOL Returns TRUE if update was successful, otherwise FALSE
 */
BOOL UpdateStartupShortcut(void) {
    // If auto-start is already enabled
    if (IsAutoStartEnabled()) {
        // First delete the existing shortcut
        RemoveShortcut();
        // Create a new shortcut
        return CreateShortcut();
    }
    return TRUE; // Auto-start not enabled, considered successful
}

/**
 * @brief Apply startup mode settings
 * @param hwnd Window handle
 * 
 * Initialize the application's display state according to the startup mode settings in the configuration file
 */
void ApplyStartupMode(HWND hwnd) {
    // Read startup mode from configuration file
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
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
        
        // Apply startup mode
        if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
            // Set to count-up mode
            CLOCK_COUNT_UP = TRUE;
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            
            // Start timer
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
            // Set to current time display mode
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_COUNT_UP = FALSE;
            
            // Start timer
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
            // Set to no display mode
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            
            // Stop timer
            KillTimer(hwnd, 1);
            
        } else { // Default to countdown mode "COUNTDOWN"
            // Set to countdown mode
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            
            // Read default countdown time
            CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
            countdown_elapsed_time = 0;
            
            // If countdown is set, start the timer
            if (CLOCK_TOTAL_TIME > 0) {
                KillTimer(hwnd, 1);
                SetTimer(hwnd, 1, 1000, NULL);
            }
        }
        
        // Refresh display
        InvalidateRect(hwnd, NULL, TRUE);
    }
}
