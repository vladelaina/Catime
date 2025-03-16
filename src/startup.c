/**
 * @file startup.c
 * @brief 开机自启动功能实现
 * 
 * 本文件实现了应用程序开机自启动相关的功能，
 * 包括检查是否已启用自启动、创建和删除自启动快捷方式。
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

/// @name 外部变量声明
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
 * @brief 检查应用程序是否已设置为开机自启动
 * @return BOOL 如果已启用开机自启动则返回TRUE，否则返回FALSE
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
 * @brief 创建开机自启动快捷方式
 * @return BOOL 如果创建成功则返回TRUE，否则返回FALSE
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
 * @brief 删除开机自启动快捷方式
 * @return BOOL 如果删除成功则返回TRUE，否则返回FALSE
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
 * @brief 更新开机自启动快捷方式
 * 
 * 检查是否已启用自启动，如果已启用，则删除旧的快捷方式并创建新的，
 * 确保即使应用程序位置发生变化，自启动功能也能正常工作。
 * 
 * @return BOOL 如果更新成功则返回TRUE，否则返回FALSE
 */
BOOL UpdateStartupShortcut(void) {
    // 如果已经启用了自启动
    if (IsAutoStartEnabled()) {
        // 先删除现有的快捷方式
        RemoveShortcut();
        // 创建新的快捷方式
        return CreateShortcut();
    }
    return TRUE; // 未启用自启动，视为成功
}

/**
 * @brief 应用启动模式设置
 * @param hwnd 窗口句柄
 * 
 * 根据配置文件中的启动模式设置，初始化应用程序的显示状态
 */
void ApplyStartupMode(HWND hwnd) {
    // 从配置文件读取启动模式
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
        
        // 应用启动模式
        if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
            // 设置为正计时模式
            CLOCK_COUNT_UP = TRUE;
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            
            // 启动计时器
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
            // 设置为显示当前时间模式
            CLOCK_SHOW_CURRENT_TIME = TRUE;
            CLOCK_COUNT_UP = FALSE;
            
            // 启动计时器
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
            
        } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
            // 设置为不显示模式
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            CLOCK_TOTAL_TIME = 0;
            countdown_elapsed_time = 0;
            
            // 停止计时器
            KillTimer(hwnd, 1);
            
        } else { // 默认为倒计时模式 "COUNTDOWN"
            // 设置为倒计时模式
            CLOCK_SHOW_CURRENT_TIME = FALSE;
            CLOCK_COUNT_UP = FALSE;
            
            // 读取默认倒计时时间
            CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
            countdown_elapsed_time = 0;
            
            // 如果有设置倒计时，则启动计时器
            if (CLOCK_TOTAL_TIME > 0) {
                KillTimer(hwnd, 1);
                SetTimer(hwnd, 1, 1000, NULL);
            }
        }
        
        // 刷新显示
        InvalidateRect(hwnd, NULL, TRUE);
    }
}
