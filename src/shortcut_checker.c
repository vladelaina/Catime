#include "shortcut_checker.h"
#include "config.h"
#include "log.h"
#include "utils/path_utils.h"
#include "utils/string_convert.h"
#include "shortcut_checker_internal.h"
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <stdbool.h>
#include <string.h>
#define STORE_PATH_PREFIX "C:\\Program Files\\WindowsApps"
#define WINGET_PATH_PATTERN "\\AppData\\Local\\Microsoft\\WinGet\\Packages"
#define WINGET_MS_PATH_PATTERN "\\AppData\\Local\\Microsoft\\"
#define WINGET_KEYWORD "WinGet"
#define WINGET_EXE_PATTERN "\\WinGet\\catime.exe"
typedef struct {
    const char* pattern;
    bool (*matcher)(const char*, const char*);
} PackageDetectionRule;
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
    if (!GetShortcutExecutablePathW(exe_path_w, MAX_PATH)) {
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
    status = ShortcutShell_CheckStatus(exe_path, shortcut_path, MAX_PATH,
                                       target_path, MAX_PATH);
    switch (status) {
        case SHORTCUT_NOT_FOUND:
            if (shortcut_check_done) {
            } else if (is_package_install) {
                result = ShortcutShell_CreateOrUpdate(exe_path, NULL) ? 0 : 1;
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            } else {
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            }
            break;
        case SHORTCUT_POINTS_TO_CURRENT:
            if (!shortcut_check_done) {
                if (!SetShortcutCheckDone(true)) {
                    LOG_WARNING("Failed to persist shortcut check completion flag");
                }
            }
            break;
        case SHORTCUT_POINTS_TO_OTHER:
            result = ShortcutShell_CreateOrUpdate(exe_path, shortcut_path) ? 0 : 1;
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
