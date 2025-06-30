/**
 * @file shortcut_checker.c
 * @brief Implementation of desktop shortcut detection and creation
 *
 * Detects if the program is installed from the App Store or WinGet,
 * and creates a desktop shortcut when necessary.
 */

#include "../include/shortcut_checker.h"
#include "../include/config.h"
#include "../include/log.h"  // Include log header
#include <stdio.h>  // For printf debug output
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <stdbool.h>
#include <string.h>

// Import required COM interfaces
#include <shobjidl.h>

// We don't need to manually define IID_IShellLinkA, it's already defined in system headers

/**
 * @brief Check if a string starts with a specified prefix
 * 
 * @param str The string to check
 * @param prefix The prefix string
 * @return bool true if the string starts with the specified prefix, false otherwise
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
 * @brief Check if a string contains a specified substring
 * 
 * @param str The string to check
 * @param substring The substring
 * @return bool true if the string contains the specified substring, false otherwise
 */
static bool Contains(const char* str, const char* substring) {
    return strstr(str, substring) != NULL;
}

/**
 * @brief Check if the program is installed from the App Store or WinGet
 * 
 * @param exe_path Buffer to output the program path
 * @param path_size Buffer size
 * @return bool true if installed from the App Store or WinGet, false otherwise
 */
static bool IsStoreOrWingetInstall(char* exe_path, size_t path_size) {
    // Get program path
    if (GetModuleFileNameA(NULL, exe_path, path_size) == 0) {
        LOG_ERROR("Failed to get program path");
        return false;
    }
    
    LOG_DEBUG("Checking program path: %s", exe_path);
    
    // Check if it's an App Store installation path (starts with C:\Program Files\WindowsApps)
    if (StartsWith(exe_path, "C:\\Program Files\\WindowsApps")) {
        LOG_DEBUG("Detected App Store installation path");
        return true;
    }
    
    // Check if it's a WinGet installation path
    // 1. Regular path containing \AppData\Local\Microsoft\WinGet\Packages
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\WinGet\\Packages")) {
        LOG_DEBUG("Detected WinGet installation path (regular)");
        return true;
    }
    
    // 2. Possible custom WinGet installation path (if in C:\Users\username\AppData\Local\Microsoft\*)
    if (Contains(exe_path, "\\AppData\\Local\\Microsoft\\") && Contains(exe_path, "WinGet")) {
        LOG_DEBUG("Detected possible WinGet installation path (custom)");
        return true;
    }
    
    // Force test: When the path contains specific strings, consider it a path that needs to create shortcuts
    // This test path matches the installation path seen in user logs
    if (Contains(exe_path, "\\WinGet\\catime.exe")) {
        LOG_DEBUG("Detected specific WinGet installation path");
        return true;
    }
    
    LOG_DEBUG("Not a Store or WinGet installation path");
    return false;
}

/**
 * @brief Check if the desktop already has a shortcut and if the shortcut points to the current program
 * 
 * @param exe_path Program path
 * @param shortcut_path_out If a shortcut is found, output the shortcut path
 * @param shortcut_path_size Shortcut path buffer size
 * @param target_path_out If a shortcut is found, output the shortcut target path
 * @param target_path_size Target path buffer size
 * @return int 0=shortcut not found, 1=shortcut found and points to current program, 2=shortcut found but points to another path
 */
static int CheckShortcutTarget(const char* exe_path, char* shortcut_path_out, size_t shortcut_path_size, 
                              char* target_path_out, size_t target_path_size) {
    char desktop_path[MAX_PATH];
    char public_desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char link_target[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    WIN32_FIND_DATAA find_data;
    int result = 0;
    
    // Get user desktop path
    hr = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get desktop path, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    LOG_DEBUG("User desktop path: %s", desktop_path);
    
    // Get public desktop path
    hr = SHGetFolderPathA(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, 0, public_desktop_path);
    if (FAILED(hr)) {
        LOG_WARNING("Failed to get public desktop path, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_DEBUG("Public desktop path: %s", public_desktop_path);
    }
    
    // First check user desktop - build complete shortcut path (Catime.lnk)
    snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    LOG_DEBUG("Checking user desktop shortcut: %s", shortcut_path);
    
    // Check if the user desktop shortcut file exists
    bool file_exists = (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES);
    
    // If not found on user desktop, check public desktop
    if (!file_exists && SUCCEEDED(hr)) {
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", public_desktop_path);
        LOG_DEBUG("Checking public desktop shortcut: %s", shortcut_path);
        
        file_exists = (GetFileAttributesA(shortcut_path) != INVALID_FILE_ATTRIBUTES);
    }
    
    // If no shortcut file is found, return 0 directly
    if (!file_exists) {
        LOG_DEBUG("No shortcut files found");
        return 0;
    }
    
    // Save the found shortcut path to the output parameter
    if (shortcut_path_out && shortcut_path_size > 0) {
        strncpy(shortcut_path_out, shortcut_path, shortcut_path_size);
        shortcut_path_out[shortcut_path_size - 1] = '\0';
    }
    
    // Found shortcut file, get its target
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return 0;
    }
    
    // Get IPersistFile interface
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    // Convert to wide character
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    // Load shortcut
    hr = ppf->lpVtbl->Load(ppf, wide_path, STGM_READ);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to load shortcut, hr=0x%08X", (unsigned int)hr);
        ppf->lpVtbl->Release(ppf);
        psl->lpVtbl->Release(psl);
        return 0;
    }
    
    // Get shortcut target path
    hr = psl->lpVtbl->GetPath(psl, link_target, MAX_PATH, &find_data, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get shortcut target path, hr=0x%08X", (unsigned int)hr);
        result = 0;
    } else {
        LOG_DEBUG("Shortcut target path: %s", link_target);
        LOG_DEBUG("Current program path: %s", exe_path);
        
        // Save target path to output parameter
        if (target_path_out && target_path_size > 0) {
            strncpy(target_path_out, link_target, target_path_size);
            target_path_out[target_path_size - 1] = '\0';
        }
        
        // Check if the shortcut points to the current program
        if (_stricmp(link_target, exe_path) == 0) {
            LOG_DEBUG("Shortcut points to current program");
            result = 1;
        } else {
            LOG_DEBUG("Shortcut points to another path");
            result = 2;
        }
    }
    
    // Release interfaces
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return result;
}

/**
 * @brief Create or update desktop shortcut
 * 
 * @param exe_path Program path
 * @param existing_shortcut_path Existing shortcut path, create new one if NULL
 * @return bool true for successful creation/update, false for failure
 */
static bool CreateOrUpdateDesktopShortcut(const char* exe_path, const char* existing_shortcut_path) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char icon_path[MAX_PATH];
    HRESULT hr;
    IShellLinkA* psl = NULL;
    IPersistFile* ppf = NULL;
    bool success = false;
    
    // If an existing shortcut path is provided, use it; otherwise create a new shortcut on the user's desktop
    if (existing_shortcut_path && *existing_shortcut_path) {
        LOG_INFO("Starting to update desktop shortcut: %s pointing to: %s", existing_shortcut_path, exe_path);
        strcpy(shortcut_path, existing_shortcut_path);
    } else {
        LOG_INFO("Starting to create desktop shortcut, program path: %s", exe_path);
        
        // Get desktop path
        hr = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path);
        if (FAILED(hr)) {
            LOG_ERROR("Failed to get desktop path, hr=0x%08X", (unsigned int)hr);
            return false;
        }
        LOG_DEBUG("Desktop path: %s", desktop_path);
        
        // Build complete shortcut path
        snprintf(shortcut_path, sizeof(shortcut_path), "%s\\Catime.lnk", desktop_path);
    }
    
    LOG_DEBUG("Shortcut path: %s", shortcut_path);
    
    // Use program path as icon path
    strcpy(icon_path, exe_path);
    
    // Create IShellLink interface
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkA, (void**)&psl);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create IShellLink interface, hr=0x%08X", (unsigned int)hr);
        return false;
    }
    
    // Set target path
    hr = psl->lpVtbl->SetPath(psl, exe_path);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set shortcut target path, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // Set working directory (use the directory where the executable is located)
    char work_dir[MAX_PATH];
    strcpy(work_dir, exe_path);
    char* last_slash = strrchr(work_dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
    LOG_DEBUG("Working directory: %s", work_dir);
    
    hr = psl->lpVtbl->SetWorkingDirectory(psl, work_dir);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set working directory, hr=0x%08X", (unsigned int)hr);
    }
    
    // Set icon
    hr = psl->lpVtbl->SetIconLocation(psl, icon_path, 0);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set icon, hr=0x%08X", (unsigned int)hr);
    }
    
    // Set description
    hr = psl->lpVtbl->SetDescription(psl, "A very useful timer (Pomodoro Clock)");
    if (FAILED(hr)) {
        LOG_ERROR("Failed to set description, hr=0x%08X", (unsigned int)hr);
    }
    
    // Set window style (normal window)
    psl->lpVtbl->SetShowCmd(psl, SW_SHOWNORMAL);
    
    // Get IPersistFile interface
    hr = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void**)&ppf);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        psl->lpVtbl->Release(psl);
        return false;
    }
    
    // Convert to wide characters
    WCHAR wide_path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, shortcut_path, -1, wide_path, MAX_PATH);
    
    // Save shortcut
    hr = ppf->lpVtbl->Save(ppf, wide_path, TRUE);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_INFO("Desktop shortcut %s successful: %s", existing_shortcut_path ? "update" : "creation", shortcut_path);
        success = true;
    }
    
    // Release interfaces
    ppf->lpVtbl->Release(ppf);
    psl->lpVtbl->Release(psl);
    
    return success;
}

/**
 * @brief Check and create desktop shortcut
 * 
 * All installation types will check if the desktop shortcut exists and points to the current program.
 * If the shortcut already exists but points to another program, it will be updated to the current path.
 * But only versions installed from the Windows App Store or WinGet will create a new shortcut.
 * If SHORTCUT_CHECK_DONE=TRUE is marked in the configuration file, no new shortcut will be created even if the shortcut is deleted.
 * 
 * @return int 0 means no need to create/update or create/update successful, 1 means failure
 */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char config_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    bool shortcut_check_done = false;
    bool isStoreInstall = false;
    
    // Initialize COM library, needed for creating shortcuts later
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("COM library initialization failed, hr=0x%08X", (unsigned int)hr);
        return 1;
    }
    
    LOG_DEBUG("Starting shortcut check");
    
    // Read the flag from the configuration file to determine if it has been checked
    GetConfigPath(config_path, MAX_PATH);
    shortcut_check_done = IsShortcutCheckDone();
    
    LOG_DEBUG("Configuration path: %s, already checked: %d", config_path, shortcut_check_done);
    
    // Get current program path
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        CoUninitialize();
        return 1;
    }
    LOG_DEBUG("Program path: %s", exe_path);
    
    // Check if it's an App Store or WinGet installation (only affects the behavior of creating new shortcuts)
    isStoreInstall = IsStoreOrWingetInstall(exe_path, MAX_PATH);
    LOG_DEBUG("Is Store/WinGet installation: %d", isStoreInstall);
    
    // Check if the shortcut exists and points to the current program
    // Return value: 0=does not exist, 1=exists and points to the current program, 2=exists but points to another path
    int shortcut_status = CheckShortcutTarget(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    if (shortcut_status == 0) {
        // Shortcut does not exist
        if (shortcut_check_done) {
            // If the configuration has already been marked as checked, don't create it even if there's no shortcut
            LOG_INFO("No shortcut found on desktop, but configuration marked as checked, not creating");
            CoUninitialize();
            return 0;
        } else if (isStoreInstall) {
            // Only first-run Store or WinGet installations create shortcuts
            LOG_INFO("No shortcut found on desktop, first run of Store/WinGet installation, starting to create");
            bool success = CreateOrUpdateDesktopShortcut(exe_path, NULL);
            
            // Mark as checked, regardless of whether creation was successful
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return success ? 0 : 1;
        } else {
            LOG_INFO("No shortcut found on desktop, not a Store/WinGet installation, not creating shortcut");
            
            // Mark as checked
            SetShortcutCheckDone(true);
            
            CoUninitialize();
            return 0;
        }
    } else if (shortcut_status == 1) {
        // Shortcut exists and points to the current program, no action needed
        LOG_INFO("Desktop shortcut already exists and points to the current program");
        
        // Mark as checked
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return 0;
    } else if (shortcut_status == 2) {
        // Shortcut exists but points to another program, any installation method will update it
        LOG_INFO("Desktop shortcut points to another path: %s, will update to: %s", target_path, exe_path);
        bool success = CreateOrUpdateDesktopShortcut(exe_path, shortcut_path);
        
        // Mark as checked, regardless of whether the update was successful
        if (!shortcut_check_done) {
            SetShortcutCheckDone(true);
        }
        
        CoUninitialize();
        return success ? 0 : 1;
    }
    
    // Should not reach here
    LOG_ERROR("Unknown shortcut check status");
    CoUninitialize();
    return 1;
} 