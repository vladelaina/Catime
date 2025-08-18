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

extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_IS_PAUSED;
extern int CLOCK_TOTAL_TIME;
extern int countdown_elapsed_time;
extern int countup_elapsed_time;
extern int CLOCK_DEFAULT_START_TIME;
extern char CLOCK_STARTUP_MODE[20];

BOOL IsAutoStartEnabled(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        wcscat(startupPath, L"\\Catime.lnk");
        return GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES;
    }
    return FALSE;
}

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

BOOL RemoveShortcut(void) {
    wchar_t startupPath[MAX_PATH];
    
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        wcscat(startupPath, L"\\Catime.lnk");
        
        return DeleteFileW(startupPath);
    }
    return FALSE;
}

BOOL UpdateStartupShortcut(void) {
    if (IsAutoStartEnabled()) {
        RemoveShortcut();
        return CreateShortcut();
    }
    return TRUE;
}

void ApplyStartupMode(HWND hwnd) {
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
        
        if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
            CLOCK_COUNT_UP = TRUE;
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_COUNT_UP = FALSE;
            
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            
            KillTimer(hwnd, 1);
            
        } else {
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            
            CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
            countdown_elapsed_time = 0;
            
            if (CLOCK_TOTAL_TIME > 0) {
                KillTimer(hwnd, 1);
                SetTimer(hwnd, 1, 1000, NULL);
            }
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}