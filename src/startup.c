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

#ifndef CSIDL_STARTUP
#define CSIDL_STARTUP 0x0007
#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

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
