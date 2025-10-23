/**
 * @file shortcut_checker.c
 * @brief Desktop shortcut management for package manager installations
 * @version 2.0 - Refactored for better maintainability and reduced code duplication
 * 
 * Handles auto-creation and maintenance of shortcuts for Store/WinGet installs
 * with data-driven package detection and unified COM object management.
 */
#include "../include/shortcut_checker.h"
#include "../include/config.h"
#include "../include/log.h"
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Shortcut filename */
#define SHORTCUT_FILENAME "Catime.lnk"

/** @brief Shortcut description text */
#define SHORTCUT_DESCRIPTION L"A very useful timer (Pomodoro Clock)"

/** @brief Microsoft Store installation path prefix */
#define STORE_PATH_PREFIX "C:\\Program Files\\WindowsApps"

/** @brief WinGet standard installation path pattern */
#define WINGET_PATH_PATTERN "\\AppData\\Local\\Microsoft\\WinGet\\Packages"

/** @brief WinGet Microsoft subdirectory pattern */
#define WINGET_MS_PATH_PATTERN "\\AppData\\Local\\Microsoft\\"

/** @brief WinGet keyword for pattern matching */
#define WINGET_KEYWORD "WinGet"

/** @brief WinGet executable name pattern */
#define WINGET_EXE_PATTERN "\\WinGet\\catime.exe"

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Package manager detection rule
 * Data-driven approach for identifying managed installations
 */
typedef struct {
    const char* pattern;           /**< Path pattern to match */
    bool (*matcher)(const char*);  /**< Matching function (StartsWith or Contains) */
    const char* description;       /**< Human-readable description */
} PackageDetectionRule;

/**
 * @brief Shortcut check result codes
 */
typedef enum {
    SHORTCUT_NOT_FOUND = 0,       /**< No shortcut exists */
    SHORTCUT_POINTS_TO_CURRENT = 1, /**< Shortcut points to current executable */
    SHORTCUT_POINTS_TO_OTHER = 2   /**< Shortcut points to different executable */
} ShortcutStatus;

/**
 * @brief COM Shell Link wrapper for RAII-style resource management
 */
typedef struct {
    IShellLinkW* shellLink;       /**< IShellLink interface pointer */
    IPersistFile* persistFile;    /**< IPersistFile interface pointer */
    bool initialized;             /**< Whether COM objects are valid */
} ComShellLink;

/* ============================================================================
 * String Utility Functions
 * ============================================================================ */

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
 * @brief Check if string contains both substrings
 * @param str String to search in
 * @param sub1 First substring
 * @param sub2 Second substring
 * @return true if both substrings are found
 */
static bool ContainsBoth(const char* str, const char* sub1, const char* sub2) {
    return Contains(str, sub1) && Contains(str, sub2);
}

/* ============================================================================
 * Character Encoding Conversion Functions
 * ============================================================================ */

/**
 * @brief Convert wide string to UTF-8 multi-byte string
 * @param wide_str Wide string to convert
 * @param output Output buffer for UTF-8 string
 * @param output_size Size of output buffer
 * @return true on success
 */
static bool WideToUtf8(const wchar_t* wide_str, char* output, size_t output_size) {
    int result = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, output, (int)output_size, NULL, NULL);
    return result > 0;
}

/**
 * @brief Convert UTF-8 multi-byte string to wide string
 * @param utf8_str UTF-8 string to convert
 * @param output Output buffer for wide string
 * @param output_size Size of output buffer (in wchar_t count)
 * @return true on success
 */
static bool Utf8ToWide(const char* utf8_str, wchar_t* output, size_t output_size) {
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, output, (int)output_size);
    return result > 0;
}

/* ============================================================================
 * HRESULT Error Handling Macros
 * ============================================================================ */

/**
 * @brief Check HRESULT and log error if failed, then return from function
 */
#define CHECK_HR_RETURN(hr, msg, ret_val) \
    do { \
        if (FAILED(hr)) { \
            LOG_ERROR(msg ", hr=0x%08X", (unsigned int)(hr)); \
            return (ret_val); \
        } \
    } while(0)

/**
 * @brief Check HRESULT and log error if failed, then goto cleanup label
 */
#define CHECK_HR_GOTO(hr, msg, label) \
    do { \
        if (FAILED(hr)) { \
            LOG_ERROR(msg ", hr=0x%08X", (unsigned int)(hr)); \
            goto label; \
        } \
    } while(0)

/**
 * @brief Check HRESULT and log warning (non-fatal)
 */
#define CHECK_HR_WARN(hr, msg) \
    do { \
        if (FAILED(hr)) { \
            LOG_WARNING(msg ", hr=0x%08X", (unsigned int)(hr)); \
        } \
    } while(0)

/* ============================================================================
 * COM Shell Link Management
 * ============================================================================ */

/**
 * @brief Initialize COM Shell Link wrapper
 * @param link Pointer to ComShellLink structure
 * @return true on success
 */
static bool InitComShellLink(ComShellLink* link) {
    HRESULT hr;
    
    link->shellLink = NULL;
    link->persistFile = NULL;
    link->initialized = false;
    
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void**)&link->shellLink);
    CHECK_HR_RETURN(hr, "Failed to create IShellLink interface", false);
    
    hr = link->shellLink->lpVtbl->QueryInterface(link->shellLink, 
                                                  &IID_IPersistFile, 
                                                  (void**)&link->persistFile);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get IPersistFile interface, hr=0x%08X", (unsigned int)hr);
        link->shellLink->lpVtbl->Release(link->shellLink);
        link->shellLink = NULL;
        return false;
    }
    
    link->initialized = true;
    return true;
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
    link->initialized = false;
}

/* ============================================================================
 * Path Utility Functions
 * ============================================================================ */

/**
 * @brief Get desktop directory path
 * @param desktop_type CSIDL constant (CSIDL_DESKTOP or CSIDL_COMMON_DESKTOPDIRECTORY)
 * @param output Output buffer for path
 * @param output_size Size of output buffer
 * @return true on success
 */
static bool GetDesktopPath(int desktop_type, char* output, size_t output_size) {
    wchar_t path_w[MAX_PATH];
    HRESULT hr = SHGetFolderPathW(NULL, desktop_type, NULL, 0, path_w);
    
    if (FAILED(hr)) {
        return false;
    }
    
    return WideToUtf8(path_w, output, output_size);
}

/**
 * @brief Build shortcut path from desktop directory
 * @param desktop_path Desktop directory path
 * @param output Output buffer for shortcut path
 * @param output_size Size of output buffer
 */
static void BuildShortcutPath(const char* desktop_path, char* output, size_t output_size) {
    snprintf(output, output_size, "%s\\%s", desktop_path, SHORTCUT_FILENAME);
}

/**
 * @brief Extract directory from full file path
 * @param file_path Full file path
 * @param output Output buffer for directory path
 * @param output_size Size of output buffer
 */
static void ExtractDirectory(const char* file_path, char* output, size_t output_size) {
    strncpy(output, file_path, output_size);
    output[output_size - 1] = '\0';
    
    char* last_slash = strrchr(output, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
}

/* ============================================================================
 * Package Manager Detection
 * ============================================================================ */

/** @brief Package detection rules in priority order */
static const PackageDetectionRule PACKAGE_RULES[] = {
    { STORE_PATH_PREFIX,      StartsWith,   "Microsoft Store (WindowsApps)" },
    { WINGET_PATH_PATTERN,    Contains,     "WinGet standard path" },
    { WINGET_EXE_PATTERN,     Contains,     "WinGet executable pattern" },
    { NULL,                   ContainsBoth, "WinGet Microsoft directory" } // Special case
};

static const size_t PACKAGE_RULES_COUNT = sizeof(PACKAGE_RULES) / sizeof(PACKAGE_RULES[0]);

/**
 * @brief Detect if current executable is from Microsoft Store or WinGet
 * @param exe_path Current executable path
 * @return true if installed via Store or WinGet package managers
 */
static bool IsPackageManagerInstall(const char* exe_path) {
    for (size_t i = 0; i < PACKAGE_RULES_COUNT; i++) {
        if (PACKAGE_RULES[i].pattern == NULL) {
            // Special case: check both patterns
            if (ContainsBoth(exe_path, WINGET_MS_PATH_PATTERN, WINGET_KEYWORD)) {
                return true;
            }
        } else {
            if (PACKAGE_RULES[i].matcher(exe_path, PACKAGE_RULES[i].pattern)) {
                return true;
            }
        }
    }
    
    return false;
}

/* ============================================================================
 * Shortcut Operations
 * ============================================================================ */

/**
 * @brief Check if file exists
 * @param path_utf8 UTF-8 file path
 * @return true if file exists
 */
static bool FileExists(const char* path_utf8) {
    wchar_t path_w[MAX_PATH];
    if (!Utf8ToWide(path_utf8, path_w, MAX_PATH)) {
        return false;
    }
    return GetFileAttributesW(path_w) != INVALID_FILE_ATTRIBUTES;
}

/**
 * @brief Read target path from existing shortcut
 * @param shortcut_path Path to .lnk file
 * @param target_output Output buffer for target path
 * @param target_size Size of output buffer
 * @return true on success
 */
static bool ReadShortcutTarget(const char* shortcut_path, char* target_output, size_t target_size) {
    ComShellLink link;
    wchar_t shortcut_path_w[MAX_PATH];
    wchar_t target_w[MAX_PATH];
    WIN32_FIND_DATAW find_data;
    HRESULT hr;
    bool success = false;
    
    if (!InitComShellLink(&link)) {
        return false;
    }
    
    if (!Utf8ToWide(shortcut_path, shortcut_path_w, MAX_PATH)) {
        CleanupComShellLink(&link);
        return false;
    }
    
    hr = link.persistFile->lpVtbl->Load(link.persistFile, shortcut_path_w, STGM_READ);
    CHECK_HR_GOTO(hr, "Failed to load shortcut", cleanup);
    
    hr = link.shellLink->lpVtbl->GetPath(link.shellLink, target_w, MAX_PATH, &find_data, 0);
    CHECK_HR_GOTO(hr, "Failed to get shortcut target path", cleanup);
    
    success = WideToUtf8(target_w, target_output, target_size);
    
cleanup:
    CleanupComShellLink(&link);
    return success;
}

/**
 * @brief Find existing shortcut on user or public desktop
 * @param shortcut_path_output Output buffer for found shortcut path
 * @param path_size Size of output buffer
 * @return true if shortcut found
 */
static bool FindExistingShortcut(char* shortcut_path_output, size_t path_size) {
    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    
    // Check user desktop first
    if (GetDesktopPath(CSIDL_DESKTOP, desktop_path, MAX_PATH)) {
        BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH);
        if (FileExists(shortcut_path)) {
            strncpy(shortcut_path_output, shortcut_path, path_size);
            shortcut_path_output[path_size - 1] = '\0';
            return true;
        }
    }
    
    // Check public desktop
    if (GetDesktopPath(CSIDL_COMMON_DESKTOPDIRECTORY, desktop_path, MAX_PATH)) {
        BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH);
        if (FileExists(shortcut_path)) {
            strncpy(shortcut_path_output, shortcut_path, path_size);
            shortcut_path_output[path_size - 1] = '\0';
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Check existing shortcut and compare with current executable
 * @param exe_path Current executable path
 * @param shortcut_path_output Output buffer for found shortcut path
 * @param shortcut_path_size Size of shortcut path buffer
 * @param target_path_output Output buffer for shortcut target
 * @param target_path_size Size of target path buffer
 * @return ShortcutStatus enum value
 */
static ShortcutStatus CheckShortcutStatus(const char* exe_path,
                                         char* shortcut_path_output, size_t shortcut_path_size,
                                         char* target_path_output, size_t target_path_size) {
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    
    // Find shortcut
    if (!FindExistingShortcut(shortcut_path, MAX_PATH)) {
        return SHORTCUT_NOT_FOUND;
    }
    
    // Return shortcut path to caller
    if (shortcut_path_output && shortcut_path_size > 0) {
        strncpy(shortcut_path_output, shortcut_path, shortcut_path_size);
        shortcut_path_output[shortcut_path_size - 1] = '\0';
    }
    
    // Read target path
    if (!ReadShortcutTarget(shortcut_path, target_path, MAX_PATH)) {
        return SHORTCUT_NOT_FOUND;
    }
    
    // Return target path to caller
    if (target_path_output && target_path_size > 0) {
        strncpy(target_path_output, target_path, target_path_size);
        target_path_output[target_path_size - 1] = '\0';
    }
    
    // Compare paths
    if (_stricmp(target_path, exe_path) == 0) {
        return SHORTCUT_POINTS_TO_CURRENT;
    }
    
    return SHORTCUT_POINTS_TO_OTHER;
}

/**
 * @brief Configure IShellLink with shortcut properties
 * @param link COM Shell Link wrapper
 * @param exe_path Target executable path
 * @return true on success
 */
static bool ConfigureShellLink(ComShellLink* link, const char* exe_path) {
    wchar_t exe_path_w[MAX_PATH];
    wchar_t work_dir_w[MAX_PATH];
    char work_dir[MAX_PATH];
    HRESULT hr;
    
    // Convert executable path
    if (!Utf8ToWide(exe_path, exe_path_w, MAX_PATH)) {
        return false;
    }
    
    // Set target path
    hr = link->shellLink->lpVtbl->SetPath(link->shellLink, exe_path_w);
    CHECK_HR_RETURN(hr, "Failed to set shortcut target path", false);
    
    // Extract and set working directory
    ExtractDirectory(exe_path, work_dir, MAX_PATH);
    if (Utf8ToWide(work_dir, work_dir_w, MAX_PATH)) {
        hr = link->shellLink->lpVtbl->SetWorkingDirectory(link->shellLink, work_dir_w);
        CHECK_HR_WARN(hr, "Failed to set working directory");
    }
    
    // Set icon (use executable's embedded icon)
    hr = link->shellLink->lpVtbl->SetIconLocation(link->shellLink, exe_path_w, 0);
    CHECK_HR_WARN(hr, "Failed to set icon");
    
    // Set description
    hr = link->shellLink->lpVtbl->SetDescription(link->shellLink, SHORTCUT_DESCRIPTION);
    CHECK_HR_WARN(hr, "Failed to set description");
    
    // Set show command
    link->shellLink->lpVtbl->SetShowCmd(link->shellLink, SW_SHOWNORMAL);
    
    return true;
}

/**
 * @brief Create or update desktop shortcut
 * @param exe_path Target executable path
 * @param existing_shortcut_path Path to existing shortcut (NULL to create new)
 * @return true on success
 */
static bool CreateOrUpdateShortcut(const char* exe_path, const char* existing_shortcut_path) {
    ComShellLink link;
    char shortcut_path[MAX_PATH];
    wchar_t shortcut_path_w[MAX_PATH];
    HRESULT hr;
    bool success = false;
    
    // Determine shortcut path
    if (existing_shortcut_path && *existing_shortcut_path) {
        LOG_INFO("Updating desktop shortcut: %s -> %s", existing_shortcut_path, exe_path);
        strncpy(shortcut_path, existing_shortcut_path, MAX_PATH);
        shortcut_path[MAX_PATH - 1] = '\0';
    } else {
        LOG_INFO("Creating desktop shortcut for: %s", exe_path);
        
        char desktop_path[MAX_PATH];
        if (!GetDesktopPath(CSIDL_DESKTOP, desktop_path, MAX_PATH)) {
            LOG_ERROR("Failed to get desktop path");
            return false;
        }
        
        BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH);
    }
    
    // Initialize COM Shell Link
    if (!InitComShellLink(&link)) {
        return false;
    }
    
    // Configure shortcut properties
    if (!ConfigureShellLink(&link, exe_path)) {
        CleanupComShellLink(&link);
        return false;
    }
    
    // Save to disk
    if (!Utf8ToWide(shortcut_path, shortcut_path_w, MAX_PATH)) {
        CleanupComShellLink(&link);
        return false;
    }
    
    hr = link.persistFile->lpVtbl->Save(link.persistFile, shortcut_path_w, TRUE);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to save shortcut, hr=0x%08X", (unsigned int)hr);
    } else {
        LOG_INFO("Desktop shortcut %s successful: %s", 
                existing_shortcut_path ? "update" : "creation", shortcut_path);
        success = true;
    }
    
    CleanupComShellLink(&link);
    return success;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Main function to check and manage desktop shortcuts
 * @return 0 on success, 1 on error
 * 
 * Decision logic:
 * - No shortcut + already checked -> Don't create (respect user choice)
 * - No shortcut + package install -> Create shortcut
 * - No shortcut + manual install -> Don't create
 * - Shortcut points to current -> OK
 * - Shortcut points to other -> Update
 */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    wchar_t exe_path_w[MAX_PATH];
    bool shortcut_check_done;
    bool is_package_install;
    ShortcutStatus status;
    HRESULT hr;
    int result = 0;
    
    // Initialize COM
    hr = CoInitialize(NULL);
    CHECK_HR_RETURN(hr, "COM library initialization failed", 1);
    
    // Get current executable path
    if (GetModuleFileNameW(NULL, exe_path_w, MAX_PATH) == 0) {
        LOG_ERROR("Failed to get program path");
        CoUninitialize();
        return 1;
    }
    
    if (!WideToUtf8(exe_path_w, exe_path, MAX_PATH)) {
        LOG_ERROR("Failed to convert executable path");
        CoUninitialize();
        return 1;
    }
    
    // Load configuration state
    shortcut_check_done = IsShortcutCheckDone();
    
    // Detect installation type
    is_package_install = IsPackageManagerInstall(exe_path);
    
    // Check existing shortcut
    status = CheckShortcutStatus(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    // Decision logic based on status
    switch (status) {
        case SHORTCUT_NOT_FOUND:
            if (shortcut_check_done) {
                // Already checked, don't create (user may have deleted it)
                LOG_INFO("No shortcut found, but already checked - respecting user choice");
            } else if (is_package_install) {
                // First run of package install - auto-create
                LOG_INFO("Package manager installation detected - creating shortcut");
                result = CreateOrUpdateShortcut(exe_path, NULL) ? 0 : 1;
                SetShortcutCheckDone(true);
            } else {
                // Manual installation - don't auto-create
                LOG_INFO("Manual installation detected - not creating shortcut");
                SetShortcutCheckDone(true);
            }
            break;
            
        case SHORTCUT_POINTS_TO_CURRENT:
            // All good
            LOG_INFO("Desktop shortcut exists and points to current program");
            if (!shortcut_check_done) {
                SetShortcutCheckDone(true);
            }
            break;
            
        case SHORTCUT_POINTS_TO_OTHER:
            // Update to point to current executable
            LOG_INFO("Shortcut points to different path - updating");
            LOG_INFO("  Old: %s", target_path);
            LOG_INFO("  New: %s", exe_path);
            result = CreateOrUpdateShortcut(exe_path, shortcut_path) ? 0 : 1;
            if (!shortcut_check_done) {
                SetShortcutCheckDone(true);
            }
            break;
            
        default:
            LOG_ERROR("Unknown shortcut check status");
            result = 1;
            break;
    }
    
    CoUninitialize();
    return result;
}
