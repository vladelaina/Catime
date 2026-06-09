/**
 * @file shortcut_checker.c
 * @brief Desktop shortcut auto-management for Store/WinGet installs
 */
#include "shortcut_checker.h"
#include "config.h"
#include "log.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <stdbool.h>
#include <string.h>

#define SHORTCUT_FILENAME "Catime.lnk"
#define SHORTCUT_DESCRIPTION L"A very useful timer (Pomodoro Clock)"
#define STORE_PATH_PREFIX "C:\\Program Files\\WindowsApps"
#define WINGET_PATH_PATTERN "\\AppData\\Local\\Microsoft\\WinGet\\Packages"
#define WINGET_MS_PATH_PATTERN "\\AppData\\Local\\Microsoft\\"
#define WINGET_KEYWORD "WinGet"
#define WINGET_EXE_PATTERN "\\WinGet\\catime.exe"

typedef struct {
    const char* pattern;
    bool (*matcher)(const char*, const char*);
} PackageDetectionRule;

typedef enum {
    SHORTCUT_NOT_FOUND = 0,
    SHORTCUT_POINTS_TO_CURRENT = 1,
    SHORTCUT_POINTS_TO_OTHER = 2
} ShortcutStatus;

typedef struct {
    IShellLinkW* shellLink;
    IPersistFile* persistFile;
    bool initialized;
} ComShellLink;

static bool EnsureComInitializedForShortcut(bool* should_uninitialize) {
    if (!should_uninitialize) {
        return false;
    }

    *should_uninitialize = false;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        *should_uninitialize = true;
        return true;
    }

    if (hr == RPC_E_CHANGED_MODE) {
        return true;
    }

    LOG_ERROR("COM library initialization failed, hr=0x%08X", (unsigned int)hr);
    return false;
}

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

static bool ContainsBoth(const char* str, const char* sub1, const char* sub2) {
    return Contains(str, sub1) && Contains(str, sub2);
}

static bool CopyStringExact(const char* src, char* output, size_t output_size) {
    if (!src || !output || output_size == 0) {
        return false;
    }

    output[0] = '\0';
    size_t len = strlen(src);
    if (len >= output_size) {
        return false;
    }

    memcpy(output, src, len + 1);
    return true;
}

/* Thin wrappers for utils/string_convert.h (bool return type for this file's convention) */
static inline bool LocalWideToUtf8(const wchar_t* wide_str, char* output, size_t output_size) {
    return WideToUtf8(wide_str, output, output_size) ? true : false;
}

static inline bool LocalUtf8ToWide(const char* utf8_str, wchar_t* output, size_t output_size) {
    return Utf8ToWide(utf8_str, output, output_size) ? true : false;
}

#define CHECK_HR_RETURN(hr, msg, ret_val) \
    do { \
        if (FAILED(hr)) { \
            LOG_ERROR(msg ", hr=0x%08X", (unsigned int)(hr)); \
            return (ret_val); \
        } \
    } while(0)

#define CHECK_HR_GOTO(hr, msg, label) \
    do { \
        if (FAILED(hr)) { \
            LOG_ERROR(msg ", hr=0x%08X", (unsigned int)(hr)); \
            goto label; \
        } \
    } while(0)

#define CHECK_HR_WARN(hr, msg) \
    do { \
        if (FAILED(hr)) { \
            LOG_WARNING(msg ", hr=0x%08X", (unsigned int)(hr)); \
        } \
    } while(0)

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

static bool GetDesktopPath(int desktop_type, char* output, size_t output_size) {
    wchar_t path_w[MAX_PATH];
    HRESULT hr = SHGetFolderPathW(NULL, desktop_type, NULL, 0, path_w);
    
    if (FAILED(hr)) {
        return false;
    }
    
    return LocalWideToUtf8(path_w, output, output_size);
}

static bool BuildShortcutPath(const char* desktop_path, char* output, size_t output_size) {
    if (!desktop_path || !output || output_size == 0) {
        return false;
    }

    output[0] = '\0';
    int written = snprintf(output, output_size, "%s\\%s", desktop_path, SHORTCUT_FILENAME);
    if (written < 0 || (size_t)written >= output_size) {
        output[0] = '\0';
        return false;
    }
    return true;
}

static bool ExtractDirectory(const char* file_path, char* output, size_t output_size) {
    if (!file_path || !output || output_size == 0) {
        return false;
    }

    output[0] = '\0';
    if (!CopyStringExact(file_path, output, output_size)) {
        return false;
    }
    
    char* last_slash = strrchr(output, '\\');
    if (!last_slash || last_slash == output) {
        output[0] = '\0';
        return false;
    }
    *last_slash = '\0';
    return true;
}

/** Extensible pattern matching avoids hardcoding install paths */
static const PackageDetectionRule PACKAGE_RULES[] = {
    { STORE_PATH_PREFIX,      StartsWith },
    { WINGET_PATH_PATTERN,    Contains },
    { WINGET_EXE_PATTERN,     Contains },
};

static const size_t PACKAGE_RULES_COUNT = sizeof(PACKAGE_RULES) / sizeof(PACKAGE_RULES[0]);

static bool IsPackageManagerInstall(const char* exe_path) {
    for (size_t i = 0; i < PACKAGE_RULES_COUNT; i++) {
        if (PACKAGE_RULES[i].matcher(exe_path, PACKAGE_RULES[i].pattern)) {
            return true;
        }
    }
    
    if (ContainsBoth(exe_path, WINGET_MS_PATH_PATTERN, WINGET_KEYWORD)) {
        return true;
    }
    
    return false;
}

static bool ShortcutFileExists(const char* path_utf8) {
    wchar_t path_w[MAX_PATH];
    if (!LocalUtf8ToWide(path_utf8, path_w, MAX_PATH)) {
        return false;
    }
    return GetFileAttributesW(path_w) != INVALID_FILE_ATTRIBUTES;
}

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
    
    if (!LocalUtf8ToWide(shortcut_path, shortcut_path_w, MAX_PATH)) {
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

/** User desktop has priority over public desktop */
static bool FindExistingShortcut(char* shortcut_path_output, size_t path_size) {
    if (!shortcut_path_output || path_size == 0) {
        return false;
    }
    shortcut_path_output[0] = '\0';

    char desktop_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    
    if (GetDesktopPath(CSIDL_DESKTOP, desktop_path, MAX_PATH)) {
        if (BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH) &&
            ShortcutFileExists(shortcut_path)) {
            return CopyStringExact(shortcut_path, shortcut_path_output, path_size);
        }
    }
    
    if (GetDesktopPath(CSIDL_COMMON_DESKTOPDIRECTORY, desktop_path, MAX_PATH)) {
        if (BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH) &&
            ShortcutFileExists(shortcut_path)) {
            return CopyStringExact(shortcut_path, shortcut_path_output, path_size);
        }
    }
    
    return false;
}

static ShortcutStatus CheckShortcutStatus(const char* exe_path,
                                         char* shortcut_path_output, size_t shortcut_path_size,
                                         char* target_path_output, size_t target_path_size) {
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    
    if (!FindExistingShortcut(shortcut_path, MAX_PATH)) {
        return SHORTCUT_NOT_FOUND;
    }
    
    if (shortcut_path_output && shortcut_path_size > 0) {
        if (!CopyStringExact(shortcut_path, shortcut_path_output, shortcut_path_size)) {
            return SHORTCUT_NOT_FOUND;
        }
    }
    
    if (!ReadShortcutTarget(shortcut_path, target_path, MAX_PATH)) {
        return SHORTCUT_NOT_FOUND;
    }
    
    if (target_path_output && target_path_size > 0) {
        if (!CopyStringExact(target_path, target_path_output, target_path_size)) {
            return SHORTCUT_NOT_FOUND;
        }
    }
    
    if (_stricmp(target_path, exe_path) == 0) {
        return SHORTCUT_POINTS_TO_CURRENT;
    }
    
    return SHORTCUT_POINTS_TO_OTHER;
}

static bool ConfigureShellLink(ComShellLink* link, const char* exe_path) {
    wchar_t exe_path_w[MAX_PATH];
    wchar_t work_dir_w[MAX_PATH];
    char work_dir[MAX_PATH];
    HRESULT hr;
    
    if (!LocalUtf8ToWide(exe_path, exe_path_w, MAX_PATH)) {
        return false;
    }
    
    hr = link->shellLink->lpVtbl->SetPath(link->shellLink, exe_path_w);
    CHECK_HR_RETURN(hr, "Failed to set shortcut target path", false);
    
    if (ExtractDirectory(exe_path, work_dir, MAX_PATH) &&
        Utf8ToWide(work_dir, work_dir_w, MAX_PATH)) {
        hr = link->shellLink->lpVtbl->SetWorkingDirectory(link->shellLink, work_dir_w);
        CHECK_HR_WARN(hr, "Failed to set working directory");
    }
    
    hr = link->shellLink->lpVtbl->SetIconLocation(link->shellLink, exe_path_w, 0);
    CHECK_HR_WARN(hr, "Failed to set icon");
    
    hr = link->shellLink->lpVtbl->SetDescription(link->shellLink, SHORTCUT_DESCRIPTION);
    CHECK_HR_WARN(hr, "Failed to set description");
    
    link->shellLink->lpVtbl->SetShowCmd(link->shellLink, SW_SHOWNORMAL);
    
    return true;
}

static bool CreateOrUpdateShortcut(const char* exe_path, const char* existing_shortcut_path) {
    ComShellLink link;
    char shortcut_path[MAX_PATH];
    wchar_t shortcut_path_w[MAX_PATH];
    HRESULT hr;
    bool success = false;
    
    if (existing_shortcut_path && *existing_shortcut_path) {
        LOG_INFO("Updating desktop shortcut: %s -> %s", existing_shortcut_path, exe_path);
        size_t shortcut_path_len = strlen(existing_shortcut_path);
        if (shortcut_path_len >= MAX_PATH) {
            LOG_ERROR("Existing shortcut path is too long");
            return false;
        }
        memcpy(shortcut_path, existing_shortcut_path, shortcut_path_len + 1);
    } else {
        LOG_INFO("Creating desktop shortcut for: %s", exe_path);
        
        char desktop_path[MAX_PATH];
        if (!GetDesktopPath(CSIDL_DESKTOP, desktop_path, MAX_PATH)) {
            LOG_ERROR("Failed to get desktop path");
            return false;
        }
        
        if (!BuildShortcutPath(desktop_path, shortcut_path, MAX_PATH)) {
            LOG_ERROR("Failed to build desktop shortcut path");
            return false;
        }
    }
    
    if (!InitComShellLink(&link)) {
        return false;
    }
    
    if (!ConfigureShellLink(&link, exe_path)) {
        CleanupComShellLink(&link);
        return false;
    }
    
    if (!LocalUtf8ToWide(shortcut_path, shortcut_path_w, MAX_PATH)) {
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

/** State machine respects user intent: deleted shortcuts stay deleted, stale shortcuts get updated */
int CheckAndCreateShortcut(void) {
    char exe_path[MAX_PATH];
    char shortcut_path[MAX_PATH];
    char target_path[MAX_PATH];
    wchar_t exe_path_w[MAX_PATH];
    bool shortcut_check_done;
    bool is_package_install;
    bool should_uninitialize_com = false;
    ShortcutStatus status;
    int result = 0;
    
    if (!EnsureComInitializedForShortcut(&should_uninitialize_com)) {
        return 1;
    }
    
    DWORD exe_path_len = GetModuleFileNameW(NULL, exe_path_w, MAX_PATH);
    if (exe_path_len == 0 || exe_path_len >= MAX_PATH) {
        LOG_ERROR("Failed to get program path");
        if (should_uninitialize_com) {
            CoUninitialize();
        }
        return 1;
    }
    
    if (!WideToUtf8(exe_path_w, exe_path, MAX_PATH)) {
        LOG_ERROR("Failed to convert executable path");
        if (should_uninitialize_com) {
            CoUninitialize();
        }
        return 1;
    }
    
    shortcut_check_done = IsShortcutCheckDone();
    is_package_install = IsPackageManagerInstall(exe_path);
    status = CheckShortcutStatus(exe_path, shortcut_path, MAX_PATH, target_path, MAX_PATH);
    
    switch (status) {
        case SHORTCUT_NOT_FOUND:
            if (shortcut_check_done) {
                LOG_INFO("No shortcut found, but already checked - respecting user choice");
            } else if (is_package_install) {
                LOG_INFO("Package manager installation detected - creating shortcut");
                result = CreateOrUpdateShortcut(exe_path, NULL) ? 0 : 1;
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            } else {
                LOG_INFO("Manual installation detected - not creating shortcut");
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            }
            break;
            
        case SHORTCUT_POINTS_TO_CURRENT:
            LOG_INFO("Desktop shortcut exists and points to current program");
            if (!shortcut_check_done) {
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            }
            break;
            
        case SHORTCUT_POINTS_TO_OTHER:
            LOG_INFO("Shortcut points to different path - updating");
            LOG_INFO("  Old: %s", target_path);
            LOG_INFO("  New: %s", exe_path);
            result = CreateOrUpdateShortcut(exe_path, shortcut_path) ? 0 : 1;
            if (!shortcut_check_done) {
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            }
            break;
            
        default:
            LOG_ERROR("Unknown shortcut check status");
            result = 1;
            break;
    }
    
    if (should_uninitialize_com) {
        CoUninitialize();
    }
    return result;
}
