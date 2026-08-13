#include "shortcut_checker.h"
#include "config.h"
#include "log.h"
#include "utils/package_identity.h"
#include "utils/path_utils.h"
#include "utils/string_convert.h"
#include "shortcut_checker_internal.h"
#include "shortcut_policy.h"
#include <stdio.h>
#include <shlobj.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <stdbool.h>
#include <string.h>
#define STORE_PACKAGE_NAME "vladelaina.Catime"
#define STORE_EXECUTABLE_NAME "catime.exe"
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
static bool Contains(const char* str, const char* substring) {
    return strstr(str, substring) != NULL;
}
static bool ContainsBoth(const char* str, const char* sub1, const char* sub2) {
    return Contains(str, sub1) && Contains(str, sub2);
}
static const PackageDetectionRule PACKAGE_RULES[] = {
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

static bool CreateAndVerifyPackagedShortcut(
    const wchar_t* appUserModelId,
    const char* existingShortcutPath) {
    if (!ShortcutShell_CreateOrUpdatePackaged(
            appUserModelId, existingShortcutPath)) {
        return false;
    }

    ShortcutStatus verified = existingShortcutPath && *existingShortcutPath
        ? ShortcutShell_CheckPackagedPath(
              appUserModelId, existingShortcutPath, NULL, 0)
        : ShortcutShell_CheckPackagedStatus(
              appUserModelId, NULL, 0, NULL, 0);
    if (verified != SHORTCUT_POINTS_TO_CURRENT) {
        LOG_ERROR("Packaged desktop shortcut failed post-write verification");
        return false;
    }
    return true;
}

static void PersistShortcutCheckDone(void) {
    if (!SetShortcutCheckDone(TRUE)) {
        LOG_WARNING("Failed to persist shortcut check completion flag");
    }
}

static bool CreateAndVerifyRegularShortcut(const char* executablePath,
                                           const char* existingShortcutPath) {
    char targetPath[MAX_PATH] = {0};
    if (!ShortcutShell_CreateOrUpdate(executablePath, existingShortcutPath)) {
        return false;
    }
    ShortcutStatus status = existingShortcutPath && *existingShortcutPath
        ? ShortcutShell_CheckPath(
              executablePath, existingShortcutPath,
              targetPath, sizeof(targetPath))
        : ShortcutShell_CheckStatus(
              executablePath, NULL, 0,
              targetPath, sizeof(targetPath));
    return status ==
           SHORTCUT_POINTS_TO_CURRENT;
}

static int CheckAndCreatePackagedShortcut(void) {
    wchar_t appUserModelId[MAX_PATH] = {0};
    char shortcutPath[MAX_PATH] = {0};
    char targetPath[MAX_PATH] = {0};
    ShortcutStatus status;
    bool shortcutCheckDone = IsShortcutCheckDone();
    int result = 0;

    if (!GetCurrentApplicationUserModelIdSafeW(
            appUserModelId, _countof(appUserModelId))) {
        LOG_ERROR("Failed to resolve packaged shortcut identity");
        return 1;
    }

    status = ShortcutShell_CheckPackagedStatus(
        appUserModelId, shortcutPath, _countof(shortcutPath),
        targetPath, _countof(targetPath));
    switch (status) {
        case SHORTCUT_NOT_FOUND:
            if (!shortcutCheckDone) {
                if (!CreateAndVerifyPackagedShortcut(appUserModelId, NULL)) {
                    result = 1;
                } else {
                    PersistShortcutCheckDone();
                }
            }
            break;
        case SHORTCUT_POINTS_TO_CURRENT:
            if (!shortcutCheckDone) PersistShortcutCheckDone();
            break;
        case SHORTCUT_POINTS_TO_OTHER:
            /* Only repair links previously generated for a Store package. Do
             * not overwrite a user's shortcut for another installation. */
            if (ShortcutPolicy_IsLegacyPackagedTarget(
                    targetPath, STORE_PACKAGE_NAME,
                    STORE_EXECUTABLE_NAME)) {
                if (!CreateAndVerifyPackagedShortcut(
                        appUserModelId, shortcutPath)) {
                    result = 1;
                } else if (!shortcutCheckDone) {
                    PersistShortcutCheckDone();
                }
            } else if (!shortcutCheckDone) {
                PersistShortcutCheckDone();
            }
            break;
        case SHORTCUT_CHECK_ERROR:
            LOG_ERROR("Failed to inspect the packaged desktop shortcut");
            result = 1;
            break;
        default:
            result = 1;
            break;
    }
    return result;
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
    if (IsRunningPackagedApp()) {
        result = CheckAndCreatePackagedShortcut();
        if (should_uninitialize_com) CoUninitialize();
        return result;
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
                if (!CreateAndVerifyRegularShortcut(exe_path, NULL)) {
                    result = 1;
                } else {
                    PersistShortcutCheckDone();
                }
            } else {
                PersistShortcutCheckDone();
            }
            break;
        case SHORTCUT_POINTS_TO_CURRENT:
            if (!shortcut_check_done) PersistShortcutCheckDone();
            break;
        case SHORTCUT_POINTS_TO_OTHER:
            if (!CreateAndVerifyRegularShortcut(exe_path, shortcut_path)) {
                result = 1;
            } else if (!shortcut_check_done) {
                PersistShortcutCheckDone();
            }
            break;
        case SHORTCUT_CHECK_ERROR:
            LOG_ERROR("Failed to inspect the desktop shortcut");
            result = 1;
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
