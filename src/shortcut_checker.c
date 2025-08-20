/**
 * @file shortcut_checker.c
 * @brief Desktop shortcut management for package manager installations
 * Handles auto-creation and maintenance of shortcuts for Store/WinGet installs
 */
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

/**
 * @brief Check if string starts with specified prefix
 * @param str String to check
 * @param prefix Prefix to match against
 * @return true if str starts with prefix
 */
static bool StartsWith(const char* str, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    size_t str_len = strlen(str);
    
    if (str_len < prefix_len) {
        return false;
    }
    
    return strncmp(str, prefix, prefix_len) == 0;
}

/**
 * @brief Check if string contains specified substring
 * @param str String to search in
 * @param substring Substring to find
 * @return true if substring is found in str
 */
static bool Contains(const char* str, const char* substring) {
    return strstr(str, substring) != NULL;
}

/**
 * @brief Detect if current executable is from Microsoft Store or WinGet
 * @param exe_path Output buffer for executable path
 * @param path_size Size of the output buffer
 * @return true if installed via Store or WinGet package managers
 * Uses path pattern matching to identify managed installations
 */
static bool IsStoreOrWingetInstall(char* exe_path, size_t path_size) {
    wchar_t exe_path_w[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        return false;
    }
    
    WideCharToMultiByte(CP_UTF8, 0, exe_path_w, -1, exe_path, path_size, NULL, NULL);
    
    LOG_DEBUG("Checking program path: %s", exe_path);
    
    /** Microsoft Store apps are installed under WindowsApps */
    if (StartsWith(exe_path, "C:\\Program Files\\WindowsApps")) {
        LOG_DEBUG("Detected App Store installation path");
        return true;
    }
    
    /** Standard WinGet installation directory */
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\WinGet\\Packages")) {
        LOG_DEBUG("Detected WinGet installation path (regular)");
        return true;
    }
    
    /** Alternative WinGet installation patterns */
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\") && Contains(exe_path, "WinGet")) {
        LOG_DEBUG("Detected possible WinGet installation path (custom)");
        return true;
    }
    
    /** Specific executable pattern for WinGet catime */
    if (Contains(exe_path, "\\WinGet\\catime.exe")) {
        LOG_DEBUG("Detected specific WinGet installation path");
        return true;
    }
    
    LOG_DEBUG("Not a Store or WinGet installation path");
    return false;
}

/**
 * @brief Check existing shortcut and compare its target with current executable
 * @param exe_path Current executable path to compare against
 * @param shortcut_path_out Output buffer for found shortcut path
 * @param shortcut_path_size Size of shortcut path buffer
 * @param target_path_out Output buffer for shortcut target path
 * @param target_path_size Size of target path buffer
 * @return 0=no shortcut, 1=shortcut points to current exe, 2=shortcut points elsewhere
 * Searches both user and public desktop locations
 */
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
    
    /** Get user desktop directory */
    wchar_t desktop_path_w[MAX_PATH];
    hr = SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path_w);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get desktop path, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    WideCharToMultiByte(CP_UTF8, 0, desktop_path_w, -1, desktop_path, MAX_PATH, NULL, NULL);
    LOG_DEBUG("User desktop path: %s", desktop_path);
    
    /** Get public desktop directory (all users) */
    wchar_t public_desktop_path_w[MAX_PATH];
    hr = SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, public_desktop_path_w);
    if (FAILED(hr)) {
        LOG_WARNING("Failed to get public desktop path, hr=0x%08X", (unsigned int)hr);
    } else {
        WideCharToMultiByte(CP_UTF8, 0, public_desktop_path_w, -1, public_desktop_path, MAX_PATH, NULL, NULL);
        LOG_DEBUG("Public desktop path: %s", public_desktop_path);
    }
    
    /** Check user desktop first */
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("Checking user desktop shortcut: %s", shortcut_path);
    
    wchar_t shortcut_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, shortcut_path_w, MAX_PATH);
    bool file_exists = (GetFileAttributesW(shortcut_path_w) != INVALID_FILE_ATTRIBUTES);
    
    /** Fallback to public desktop if user desktop shortcut not found */
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
    
    /** Return found shortcut path to caller */
    if (shortcut_path_out && shortcut_path_size > 0) {
        strncpy(shortcut_path_out, shortcut_path, shortcut_path_size);
        shortcut_path_out[shortcut_path_size - 1] = '\0';
    }
    
    /** Create COM object to read shortcut properties */
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    
    /** Get file persistence interface for loading shortcut */
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    /** Load shortcut file from disk */
    hr = ppf->lpVtbl->Load(ppf, shortcut_path_w, STGM_READ);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to load shortcut, hr=0x%08X", (unsigned int)hr);
        ppf->lpVtbl->Release(ppf);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    /** Extract target path from shortcut and compare with current executable */
    wchar_t link_target_w[MAX_PATH];
    hr = psl->lpVtbl->GetPath(psl, link_target_w, MAX_PATH, &find_data, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get shortcut target path, hr=0x%08X", (unsigned int)hr);
        result = 0;
    } else {
        WideCharToMultiByte(CP_UTF8, 0, link_target_w, -1, link_target, MAX_PATH, NULL, NULL);
        
        LOG_DEBUG("Shortcut target path: %s", link_target);
        LOG_DEBUG("Current program path: %s", exe_path);
        
        /** Return target path to caller */
        if (target_path_out && target_path_size > 0) {
            strncpy(target_path_out, link_target, target_path_size);
            target_path_out[target_path_size - 1] = '\0';
        }
        
        /** Compare paths case-insensitively */
        if (_stricmp(link_target, exe_path) == 0) {
            LOG_DEBUG("Shortcut points to current program");
            result = 1;
        } else {
            LOG_DEBUG("Shortcut points to another path");
            result = 2;
        }
    }
    
    /** Cleanup COM objects */
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return result;
}

/**
 * @brief Create new desktop shortcut or update existing one to point to current executable
 * @param exe_path Path to current executable to set as shortcut target
 * @param existing_shortcut_path Path to existing shortcut to update, or NULL to create new
 * @return true if shortcut creation/update succeeded
 * Sets working directory, icon, and description for the shortcut
 */
static bool CreateOrUpdateDesktopShortcut(const char* exe_path, const char* existing_shortcut_path) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char icon_path[MAX_PATH];
    HRESULT hr;
    IShellLinkW* psl = NULL;
    IPersistFile* ppf = NULL;
    bool success = false;
    
    /** Determine shortcut path: update existing or create new */
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
    
    /** Use executable itself as icon source */
    strcpy(icon_path, exe_path);
    
    /** Create COM object for shortcut creation */
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    
    /** Set target executable path */
    wchar_t exe_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, exe_path, -1, exe_path_w, MAX_PATH);
    
    hr = psl->lpVtbl->SetPath(psl, exe_path_w);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut target path, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    /** Set working directory to executable's directory */
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
    
    /** Set icon to executable's embedded icon */
    wchar_t icon_path_w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, icon_path, -1, icon_path_w, MAX_PATH);
    hr = psl->lpVtbl->SetIconLocation(psl, icon_path_w, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set icon, hr=0x%08X", (unsigned int)hr);
    }
    
    /** Set descriptive tooltip */
    hr = psl->lpVtbl->SetDescription(psl, L"A very useful timer (Pomodoro Clock)");
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set description, hr=0x%08X", (unsigned int)hr);
    }
    
    /** Set window show state to normal */
    psl->lpVtbl->SetShowCmd(psl, SW_SHOWNORMAL);
    
    /** Get file persistence interface for saving */
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    /** Convert path to wide char and save shortcut to disk */
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    hr = ppf->lpVtbl->Save(ppf, wide_path, TRUE);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_INFO("Desktop shortcut %s successful: %s", existing_shortcut_path ? "update" : "creation", shortcut_path);
        success = true;
    }
    
    /** Cleanup COM objects */
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return success;
}

/**
 * @brief Main function to check and manage desktop shortcuts for package installations
 * @return 0 on success, 1 on error
 * Implements smart shortcut management:
 * - Creates shortcuts only for Store/WinGet installations  
 * - Updates existing shortcuts that point to wrong locations
 * - Avoids duplicate work using configuration state tracking
 */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char config_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    bool shortcut_check_done = false;
    bool isStoreInstall = false;
    
    /** Initialize COM for Shell Link operations */
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM library initialization failed, hr=0x%08X", (unsigned int)hr);
        return 1;
    }
    
    LOG_DEBUG("Starting shortcut check");
    
    /** Load configuration state to avoid redundant operations */
    GetConfigPath(config_path, MAX_PATH);
    shortcut_check_done = IsShortcutCheckDone();
    
    LOG_DEBUG("Configuration path: %s, already checked: %d", config_path, shortcut_check_done);
    
    /** Get current executable path for comparison */
    wchar_t exe_path_w[MAX_PATH];
    if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        CoUninitialize();
        return 1;
    }
    WideCharToMultiByte(CP_UTF8, 0, exe_path_w, -1, exe_path, MAX_PATH, NULL, NULL);
    LOG_DEBUG("Program path: %s", exe_path);
    
    /** Determine if this is a managed installation (Store/WinGet) */
    isStoreInstall = IsStoreOrWingetInstall(exe_path, MAX_PATH);
    LOG_DEBUG("Is Store/WinGet installation: %d", isStoreInstall);
    
    /** Check existing shortcut status */
    int shortcut_status = CheckShortcutTarget(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    /** Decision logic based on shortcut status and installation type */
    if (shortcut_status == 0) {
        /** No shortcut found */
        if (shortcut_check_done) {
            /** Already checked before, don't create to respect user choice */
            LOG_INFO("No shortcut found on desktop, but configuration marked as checked, not creating");
            CoUninitialize();
            return 0;
        } else if (isStoreInstall) {
            /** First run of managed installation - auto-create shortcut */
            LOG_INFO("No shortcut found on desktop, first run of Store/WinGet installation, starting to create");
            bool success = CreateOrUpdateDesktopShortcut(exe_path, NULL);
            
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return success ? 0 : 1;
        } else {
            /** Manual installation - don't auto-create shortcut */
            LOG_INFO("No shortcut found on desktop, not a Store/WinGet installation, not creating shortcut");
            
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return 0;
        }
    } else if (shortcut_status == 1) {
        /** Shortcut exists and points to current executable - all good */
        LOG_INFO("Desktop shortcut already exists and points to the current program");
        
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return 0;
    } else if (shortcut_status == 2) {
        /** Shortcut exists but points to wrong location - update it */
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