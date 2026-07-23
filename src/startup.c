/**
 * @file startup.c
 * @brief Windows startup shortcut orchestration and public API
 */
#include "startup.h"

#include "config.h"
#include "log.h"
#include "startup_internal.h"
#include "startup_policy.h"
#include "startup_shortcut.h"
#include "utils/package_identity.h"
#include "utils/path_utils.h"

#include <shellapi.h>
#include <string.h>

AutoStartStatus GetAutoStartStatus(void) {
    wchar_t shortcutPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};
    BOOL windowsDisabled = FALSE;

    if (IsRunningPackagedApp() ||
        !StartupPaths_GetShortcutPath(shortcutPath,
                                      _countof(shortcutPath))) {
        return AUTO_START_STATUS_ABSENT;
    }
    if (GetFileAttributesW(shortcutPath) == INVALID_FILE_ATTRIBUTES) {
        return AUTO_START_STATUS_ABSENT;
    }
    if (StartupState_QueryWindowsDisabled(&windowsDisabled) &&
        windowsDisabled) {
        return AUTO_START_STATUS_DISABLED_BY_WINDOWS;
    }
    if (!StartupPaths_GetExecutablePath(executablePath,
                                        _countof(executablePath)) ||
        !StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                   STARTUP_CMD_ARG)) {
        return AUTO_START_STATUS_BROKEN;
    }
    return AUTO_START_STATUS_ENABLED;
}

BOOL IsAutoStartEnabled(void) {
    return GetAutoStartStatus() == AUTO_START_STATUS_ENABLED;
}

static BOOL CreateShortcutInternal(BOOL clearWindowsDisableState) {
    wchar_t shortcutPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};

    if (!StartupPaths_GetExecutablePath(executablePath,
                                        _countof(executablePath)) ||
        !StartupPaths_GetShortcutPath(shortcutPath,
                                      _countof(shortcutPath))) {
        LOG_ERROR("Failed to prepare startup shortcut paths");
        return FALSE;
    }
    if (!StartupShortcut_ReplaceAtomically(shortcutPath, executablePath,
                                           STARTUP_CMD_ARG)) {
        LOG_ERROR("Failed to transactionally replace the startup shortcut");
        return FALSE;
    }

    StartupPaths_RemoveLegacyMarker();
    if (clearWindowsDisableState &&
        !StartupState_ClearWindowsApproval()) {
        LOG_WARNING("Startup shortcut was repaired but Windows still reports it disabled");
        return FALSE;
    }
    if (!StartupState_WritePreference(AUTO_START_PREFERENCE_ENABLED)) {
        LOG_WARNING("Startup shortcut is active but its preference was not persisted");
    }
    return TRUE;
}

static BOOL CreateShortcutWithIntent(BOOL clearWindowsDisableState) {
    StartupShortcutLock lock = {0};
    BOOL result;

    if (!StartupPaths_AcquireLock(&lock)) return FALSE;
    result = CreateShortcutInternal(clearWindowsDisableState);
    StartupPaths_ReleaseLock(&lock);
    return result;
}

BOOL CreateShortcut(void) {
    if (IsRunningPackagedApp()) return OpenStartupSettings();
    return CreateShortcutWithIntent(FALSE);
}

BOOL EnableAutoStart(void) {
    StartupShortcutLock lock = {0};
    BOOL result;

    if (IsRunningPackagedApp()) return OpenStartupSettings();
    if (!StartupPaths_AcquireLock(&lock)) return FALSE;
    if (!StartupState_WritePreference(AUTO_START_PREFERENCE_ENABLED)) {
        StartupPaths_ReleaseLock(&lock);
        return FALSE;
    }
    result = CreateShortcutInternal(TRUE);
    StartupPaths_ReleaseLock(&lock);
    return result;
}

static BOOL RemoveShortcutFileInternal(void) {
    wchar_t shortcutPath[MAX_PATH] = {0};

    if (!StartupPaths_GetShortcutPath(shortcutPath,
                                      _countof(shortcutPath))) {
        return FALSE;
    }
    if (!DeleteFileW(shortcutPath)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            LOG_ERROR("Failed to delete startup shortcut, error=%lu", error);
            return FALSE;
        }
    }

    StartupPaths_RemoveLegacyMarker();
    if (!StartupState_ClearWindowsApproval()) {
        LOG_WARNING("Could not clean the stale Windows startup approval state");
    }
    return TRUE;
}

BOOL RemoveShortcut(void) {
    StartupShortcutLock lock = {0};
    BOOL result;

    if (IsRunningPackagedApp()) return OpenStartupSettings();
    if (!StartupPaths_AcquireLock(&lock)) return FALSE;
    result = RemoveShortcutFileInternal();
    if (result &&
        !StartupState_WritePreference(AUTO_START_PREFERENCE_DISABLED)) {
        LOG_WARNING("Startup shortcut was removed but its preference was not persisted");
    }
    StartupPaths_ReleaseLock(&lock);
    return result;
}

BOOL DisableAutoStart(void) {
    StartupShortcutLock lock = {0};
    BOOL result;

    if (IsRunningPackagedApp()) return OpenStartupSettings();
    if (!StartupPaths_AcquireLock(&lock)) return FALSE;
    if (!StartupState_WritePreference(AUTO_START_PREFERENCE_DISABLED)) {
        StartupPaths_ReleaseLock(&lock);
        return FALSE;
    }

    result = RemoveShortcutFileInternal();
    if (!result) {
        (void)StartupState_WritePreference(AUTO_START_PREFERENCE_ENABLED);
    }
    StartupPaths_ReleaseLock(&lock);
    return result;
}

static BOOL PreserveLegacyOptOut(void) {
    char configPath[MAX_PATH] = {0};
    char configVersion[32] = {0};

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0' || !FileExists(configPath)) return TRUE;
    ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "",
                  configVersion, sizeof(configVersion), configPath);
    if (strcmp(configVersion, CATIME_VERSION) == 0) return TRUE;
    return StartupState_WritePreference(AUTO_START_PREFERENCE_DISABLED);
}

BOOL RepairExistingAutoStartShortcut(void) {
    char preference[32] = {0};
    AutoStartPreference preferenceState;
    BOOL shortcutExists;
    wchar_t shortcutPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};

    if (IsRunningPackagedApp()) return TRUE;
    if (!StartupPaths_GetShortcutPath(shortcutPath,
                                      _countof(shortcutPath))) {
        return FALSE;
    }

    shortcutExists = GetFileAttributesW(shortcutPath) !=
                     INVALID_FILE_ATTRIBUTES;
    StartupState_ReadPreference(preference, sizeof(preference));
    preferenceState = StartupPolicy_ParsePreference(preference);
    if (preferenceState == AUTO_START_PREFERENCE_STATE_DISABLED) {
        return shortcutExists ? RemoveShortcut() : TRUE;
    }
    if (!shortcutExists) {
        if (preferenceState == AUTO_START_PREFERENCE_STATE_ENABLED) {
            return CreateShortcut();
        }
        return PreserveLegacyOptOut();
    }

    if (!StartupPaths_GetExecutablePath(executablePath,
                                        _countof(executablePath))) {
        return FALSE;
    }
    if (StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                  STARTUP_CMD_ARG)) {
        return TRUE;
    }
    return CreateShortcut();
}

BOOL EnsureAutoStart(void) {
    char preference[32] = {0};
    AutoStartPreference preferenceState;
    BOOL shortcutExists;
    BOOL shouldEnable;
    wchar_t shortcutPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};

    if (IsRunningPackagedApp()) return TRUE;
    if (!StartupPaths_GetShortcutPath(shortcutPath,
                                      _countof(shortcutPath)) ||
        !StartupPaths_GetExecutablePath(executablePath,
                                        _countof(executablePath))) {
        return FALSE;
    }

    shortcutExists = GetFileAttributesW(shortcutPath) !=
                     INVALID_FILE_ATTRIBUTES;
    StartupState_ReadPreference(preference, sizeof(preference));
    preferenceState = StartupPolicy_ParsePreference(preference);
    shouldEnable = StartupPolicy_ShouldEnable(preferenceState, IsFirstRun(),
                                              shortcutExists);
    if (preferenceState == AUTO_START_PREFERENCE_STATE_DEFAULT &&
        shouldEnable &&
        !StartupState_WritePreference(AUTO_START_PREFERENCE_ENABLED)) {
        LOG_WARNING("Could not record the default enabled auto-start intent for retry");
    }

    if (!shouldEnable) {
        if (shortcutExists) return RemoveShortcut();
        StartupPaths_RemoveLegacyMarker();
        return StartupState_WritePreference(AUTO_START_PREFERENCE_DISABLED);
    }
    if (shortcutExists &&
        StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                  STARTUP_CMD_ARG)) {
        StartupPaths_RemoveLegacyMarker();
        return StartupState_WritePreference(AUTO_START_PREFERENCE_ENABLED);
    }
    if (!CreateShortcut()) {
        LOG_ERROR("Failed to create or repair startup shortcut; existing shortcut was preserved when possible");
        return FALSE;
    }
    return TRUE;
}

BOOL UpdateStartupShortcut(void) {
    return EnsureAutoStart();
}

BOOL OpenStartupSettings(void) {
    HINSTANCE result = ShellExecuteW(NULL, L"open",
                                     L"ms-settings:startupapps",
                                     NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        LOG_WARNING("Failed to open Windows Startup Apps settings");
        return FALSE;
    }
    return TRUE;
}

BOOL OpenPackagedStartupSettings(void) {
    return OpenStartupSettings();
}

void ApplyStartupMode(HWND hwnd) {
    StartupMode_ApplyConfigured(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}
