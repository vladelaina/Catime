#include "../include/shortcut_checker.h"
#include "../include/config.h"
#include "../include/log.h"
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <stdbool.h>
#include <string.h>

#include <shobjidl.h>

static bool StartsWith(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t str_len = strlen(str);
    
    if (str_len < prefix_len) {
        return false;
    }
    
    return strncmp(str, prefix, prefix_len) == 0;
}

static bool Contains(const char* str, const char* substring) {
    return strstr(str, substring) != NULL;
}

static bool IsStoreOrWingetInstall(char* exe_path, size_t path_size) {
    wchar_t exe_path_w[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        return false;
    }
    
    WideCharToMultiByte(CP_UTF8, 0, exe_path_w, -1, exe_path, path_size, NULL, NULL);
    
    LOG_DEBUG("Checking program path: %s", exe_path);
    
    if (StartsWith(exe_path, "C:\\Program Files\\WindowsApps")) {
        LOG_DEBUG("Detected App Store installation path");
        return true;
    }
    
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\WinGet\\Packages")) {
        LOG_DEBUG("Detected WinGet installation path (regular)");
        return true;
    }
    
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\") && Contains(exe_path, "WinGet")) {
        LOG_DEBUG("Detected possible WinGet installation path (custom)");
        return true;
    }
    
    if (Contains(exe_path, "\\WinGet\\catime.exe")) {
        LOG_DEBUG("Detected specific WinGet installation path");
        return true;
    }
    
    LOG_DEBUG("Not a Store or WinGet installation path");
    return false;
}

static int CheckShortcutTarget(const char* exe_path, char* shortcut_path_out, size_t shortcut_path_size, 
                              char* target_path_out, size_t target_path_size) {
    char desktop_path[MAX_PATH];
    char public_desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char link_target[MAX_PATH];
    HRESULT hr;
    IShellLinkW* psl = NULL;
    IPersistFile* ppf = NULL;
    WIN32_FIND_DATAW find_data;
    int result = 0;
    
    wchar_t desktop_path_w[MAX_PATH];
    hr = SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path_w);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get desktop path, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    WideCharToMultiByte(CP_UTF8, 0, desktop_path_w, -1, desktop_path, MAX_PATH, NULL, NULL);
    LOG_DEBUG("User desktop path: %s", desktop_path);
    
    wchar_t public_desktop_path_w[MAX_PATH];
    hr = SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, public_desktop_path_w);
    if (FAILED(hr)) {
        LOG_WARNING("Failed to get public desktop path, hr=0x%08X", (unsigned int)hr);
    } else {
        WideCharToMultiByte(CP_UTF8, 0, public_desktop_path_w, -1, public_desktop_path, MAX_PATH, NULL, NULL);
        LOG_DEBUG("Public desktop path: %s", public_desktop_path);
    }
    
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("Checking user desktop shortcut: %s", shortcut_path);
    
    wchar_t shortcut_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, shortcut_path_w, MAX_PATH);
    bool file_exists = (GetFileAttributesW(shortcut_path_w) != INVALID_FILE_ATTRIBUTES);
    
    if (!file_exists && SUCCEEDED(hr)) {
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", public_desktop_path);
        LOG_DEBUG("Checking public desktop shortcut: %s", shortcut_path);
        
        MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, shortcut_path_w, MAX_PATH);
        file_exists = (GetFileAttributesW(shortcut_path_w) != INVALID_FILE_ATTRIBUTES);
    }
    
    if (!file_exists) {
        LOG_DEBUG("No shortcut files found");
        return 0;
    }
    
    if (shortcut_path_out && shortcut_path_size > 0) {
        strncpy(shortcut_path_out, shortcut_path, shortcut_path_size);
        shortcut_path_out[shortcut_path_size - 1] = '\0';
    }
    
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    hr = ppf->lpVtbl->Load(ppf, shortcut_path_w, STGM_READ);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to load shortcut, hr=0x%08X", (unsigned int)hr);
        ppf->lpVtbl->Release(ppf);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    wchar_t link_target_w[MAX_PATH];
    hr = psl->lpVtbl->GetPath(psl, link_target_w, MAX_PATH, &find_data, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get shortcut target path, hr=0x%08X", (unsigned int)hr);
        result = 0;
    } else {
        WideCharToMultiByte(CP_UTF8, 0, link_target_w, -1, link_target, MAX_PATH, NULL, NULL);
        
        LOG_DEBUG("Shortcut target path: %s", link_target);
        LOG_DEBUG("Current program path: %s", exe_path);
        
        if (target_path_out && target_path_size > 0) {
            strncpy(target_path_out, link_target, target_path_size);
            target_path_out[target_path_size - 1] = '\0';
        }
        
        if (_stricmp(link_target, exe_path) == 0) {
            LOG_DEBUG("Shortcut points to current program");
            result = 1;
        } else {
            LOG_DEBUG("Shortcut points to another path");
            result = 2;
        }
    }
    
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return result;
}

static bool CreateOrUpdateDesktopShortcut(const char* exe_path, const char* existing_shortcut_path) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char icon_path[MAX_PATH];
    HRESULT hr;
    IShellLinkW* psl = NULL;
    IPersistFile* ppf = NULL;
    bool success = false;
    
    if (existing_shortcut_path && *existing_shortcut_path) {
        LOG_INFO("Starting to update desktop shortcut: %s pointing to: %s", existing_shortcut_path, exe_path);
        strcpy(shortcut_path, existing_shortcut_path);
    } else {
        LOG_INFO("Starting to create desktop shortcut, program path: %s", exe_path);
        
        wchar_t desktop_path_w[MAX_PATH];
        hr = SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path_w);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to get desktop path, hr=0x%08X", (unsigned int)hr);
            return false;
        }
        WideCharToMultiByte(CP_UTF8, 0, desktop_path_w, -1, desktop_path, MAX_PATH, NULL, NULL);
        LOG_DEBUG("Desktop path: %s", desktop_path);
        
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    }
    
    LOG_DEBUG("Shortcut path: %s", shortcut_path);
    
    strcpy(icon_path, exe_path);
    
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    
    wchar_t exe_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, exe_path, -1, exe_path_w, MAX_PATH);
    
    hr = psl->lpVtbl->SetPath(psl, exe_path_w);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut target path, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    char work_dir[MAX_PATH];
    strcpy(work_dir, exe_path);
    char* last_slash = strrchr(work_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    LOG_DEBUG("Working directory: %s", work_dir);
    
    wchar_t work_dir_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, work_dir, -1, work_dir_w, MAX_PATH);
    hr = psl->lpVtbl->SetWorkingDirectory(psl, work_dir_w);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set working directory, hr=0x%08X", (unsigned int)hr);
    }
    
    wchar_t icon_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, icon_path, -1, icon_path_w, MAX_PATH);
    hr = psl->lpVtbl->SetIconLocation(psl, icon_path_w, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set icon, hr=0x%08X", (unsigned int)hr);
    }
    
    hr = psl->lpVtbl->SetDescription(psl, L"A very useful timer (Pomodoro Clock)");
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set description, hr=0x%08X", (unsigned int)hr);
    }
    
    psl->lpVtbl->SetShowCmd(psl, SW_SHOWNORMAL);
    
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    hr = ppf->lpVtbl->Save(ppf, wide_path, TRUE);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_INFO("Desktop shortcut %s successful: %s", existing_shortcut_path ? "update" : "creation", shortcut_path);
        success = true;
    }
    
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return success;
}

int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char config_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    bool shortcut_check_done = false;
    bool isStoreInstall = false;
    
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM library initialization failed, hr=0x%08X", (unsigned int)hr);
        return 1;
    }
    
    LOG_DEBUG("Starting shortcut check");
    
    GetConfigPath(config_path, MAX_PATH);
    shortcut_check_done = IsShortcutCheckDone();
    
    LOG_DEBUG("Configuration path: %s, already checked: %d", config_path, shortcut_check_done);
    
    wchar_t exe_path_w[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        CoUninitialize();
        return 1;
    }
    WideCharToMultiByte(CP_UTF8, 0, exe_path_w, -1, exe_path, MAX_PATH, NULL, NULL);
    LOG_DEBUG("Program path: %s", exe_path);
    
    isStoreInstall = IsStoreOrWingetInstall(exe_path, MAX_PATH);
    LOG_DEBUG("Is Store/WinGet installation: %d", isStoreInstall);
    
    int shortcut_status = CheckShortcutTarget(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    if (shortcut_status == 0) {
        if (shortcut_check_done) {
            LOG_INFO("No shortcut found on desktop, but configuration marked as checked, not creating");
            CoUninitialize();
            return 0;
        } else if (isStoreInstall) {
            LOG_INFO("No shortcut found on desktop, first run of Store/WinGet installation, starting to create");
            bool success = CreateOrUpdateDesktopShortcut(exe_path, NULL);
            
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return success ? 0 : 1;
        } else {
            LOG_INFO("No shortcut found on desktop, not a Store/WinGet installation, not creating shortcut");
            
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return 0;
        }
    } else if (shortcut_status == 1) {
        LOG_INFO("Desktop shortcut already exists and points to the current program");
        
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return 0;
    } else if (shortcut_status == 2) {
        LOG_INFO("Desktop shortcut points to another path: %s, will update to: %s", target_path, exe_path);
        bool success = CreateOrUpdateDesktopShortcut(exe_path, shortcut_path);
        
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return success ? 0 : 1;
    }
    
    LOG_ERROR("Unknown shortcut check status");
    CoUninitialize();
    return 1;
}