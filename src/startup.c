/**
 * @file startup.c
 * @brief Windows startup integration with table-driven mode configs
 */
#include "startup.h"
#include "startup_policy.h"
#include "startup_shortcut.h"
#include "config.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "timer/timer_events.h"
#include "utils/path_utils.h"
#include "utils/package_identity.h"
#include "log.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

#define STARTUP_LINK_FILENAME L"Catime.lnk"
#define STARTUP_MARKER_FILENAME L"startup_shortcut_target.txt"
#define STARTUP_CMD_ARG L"--startup"
#define STARTUP_SHORTCUT_MUTEX_NAME L"Local\\Vladelaina.Catime.StartupShortcut"
#define STARTUP_SHORTCUT_LOCK_TIMEOUT_MS 5000
#define STARTUP_APPROVED_SUBKEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder"
#define AUTO_START_PREFERENCE_KEY "AUTO_START_PREFERENCE"
#define CONFIG_KEY_STARTUP_MODE "STARTUP_MODE="
#define STARTUP_MODE_MAX_LEN 20
#define CONFIG_LINE_BUFFER_SIZE 256
#define MODE_NAME_COUNT_UP "COUNT_UP"
#define MODE_NAME_SHOW_TIME "SHOW_TIME"
#define MODE_NAME_NO_DISPLAY "NO_DISPLAY"
#define MODE_NAME_DEFAULT "DEFAULT"

typedef struct {
    HANDLE handle;
    BOOL acquired;
} StartupShortcutLock;

/** Table-driven design eliminates if-else chains */
typedef struct {
    const char* modeName;
    BOOL showCurrentTime;
    BOOL countUp;
    BOOL enableTimer;
    int totalTime;
    const char* description;
} StartupModeConfig;

static const StartupModeConfig STARTUP_MODE_CONFIGS[] = {
    { 
        MODE_NAME_COUNT_UP, 
        FALSE, TRUE, TRUE, 0,
        "Count-up timer from zero" 
    },
    { 
        MODE_NAME_SHOW_TIME, 
        TRUE, FALSE, TRUE, 0,
        "Display current system time" 
    },
    { 
        MODE_NAME_NO_DISPLAY, 
        FALSE, FALSE, FALSE, 0,
        "Hidden mode, no timer display" 
    },
    { 
        MODE_NAME_DEFAULT, 
        FALSE, FALSE, TRUE, -1,
        "Standard countdown timer with default time" 
    },
};

static const size_t STARTUP_MODE_COUNT = sizeof(STARTUP_MODE_CONFIGS) / sizeof(STARTUP_MODE_CONFIGS[0]);

/** PathCombineW prevents buffer overflows vs sprintf */
static BOOL GetStartupShortcutPath(wchar_t* output, size_t outputSize) {
    if (!output || outputSize == 0 || outputSize > (size_t)MAXDWORD) return FALSE;
    output[0] = L'\0';
    if (outputSize < MAX_PATH) return FALSE;

    wchar_t startupFolder[MAX_PATH] = {0};
    HRESULT hr;
    
    hr = SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupFolder);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to get startup folder path, hr=0x%08X", (unsigned int)hr);
        return FALSE;
    }
    
    if (!PathCombineW(output, startupFolder, STARTUP_LINK_FILENAME)) {
        LOG_ERROR("Failed to combine startup path");
        return FALSE;
    }
    
    return TRUE;
}

static BOOL GetExecutablePath(wchar_t* output, size_t outputSize) {
    if (!GetShortcutExecutablePathW(output, outputSize)) {
        LOG_ERROR("Failed to get executable path");
        return FALSE;
    }
    return TRUE;
}

static BOOL GetStartupShortcutMarkerPath(wchar_t* output, size_t outputSize) {
    char configPath[MAX_PATH] = {0};
    wchar_t configPathW[MAX_PATH] = {0};
    wchar_t configDir[MAX_PATH] = {0};
    wchar_t* lastSlash = NULL;
    wchar_t* lastForwardSlash = NULL;

    if (!output || outputSize == 0 || outputSize > (size_t)MAXDWORD) return FALSE;
    output[0] = L'\0';

    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') {
        return FALSE;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, configPathW, MAX_PATH) == 0) {
        LOG_WARNING("Failed to convert config path for startup shortcut marker");
        return FALSE;
    }

    if (wcscpy_s(configDir, _countof(configDir), configPathW) != 0) {
        return FALSE;
    }

    lastSlash = wcsrchr(configDir, L'\\');
    lastForwardSlash = wcsrchr(configDir, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == configDir) {
        return FALSE;
    }
    *lastSlash = L'\0';

    if (!PathCombineW(output, configDir, STARTUP_MARKER_FILENAME)) {
        LOG_WARNING("Failed to build startup shortcut marker path");
        return FALSE;
    }

    return TRUE;
}

static void RemoveLegacyStartupShortcutMarker(void) {
    wchar_t markerPath[MAX_PATH] = {0};

    if (GetStartupShortcutMarkerPath(markerPath, _countof(markerPath))) {
        DeleteFileW(markerPath);
    }
}

static BOOL WriteAutoStartPreference(const char* preference) {
    char configPath[MAX_PATH] = {0};
    if (!preference) return FALSE;
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') return FALSE;
    if (!WriteIniString(INI_SECTION_GENERAL, AUTO_START_PREFERENCE_KEY,
                        preference, configPath)) {
        LOG_WARNING("Failed to persist auto-start preference: %s", preference);
        return FALSE;
    }
    return TRUE;
}

static BOOL AcquireStartupShortcutLock(StartupShortcutLock* lock) {
    DWORD waitResult;
    if (!lock) return FALSE;
    lock->handle = CreateMutexW(NULL, FALSE, STARTUP_SHORTCUT_MUTEX_NAME);
    lock->acquired = FALSE;
    if (!lock->handle) {
        LOG_WARNING("Failed to create startup shortcut mutex, error=%lu",
                    GetLastError());
        return FALSE;
    }

    waitResult = WaitForSingleObject(lock->handle,
                                     STARTUP_SHORTCUT_LOCK_TIMEOUT_MS);
    if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
        lock->acquired = TRUE;
        return TRUE;
    }

    LOG_WARNING("Failed to acquire startup shortcut mutex, result=%lu error=%lu",
                waitResult, GetLastError());
    CloseHandle(lock->handle);
    lock->handle = NULL;
    return FALSE;
}

static void ReleaseStartupShortcutLock(StartupShortcutLock* lock) {
    if (!lock || !lock->handle) return;
    if (lock->acquired) ReleaseMutex(lock->handle);
    CloseHandle(lock->handle);
    lock->handle = NULL;
    lock->acquired = FALSE;
}

static BOOL QueryStartupApprovedDisabled(BOOL* disabled) {
    HKEY key = NULL;
    BYTE data[32] = {0};
    DWORD dataSize = sizeof(data);
    DWORD type = 0;
    LSTATUS status;

    if (!disabled) return FALSE;
    *disabled = FALSE;

    status = RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_APPROVED_SUBKEY, 0,
                           KEY_QUERY_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND) return TRUE;
    if (status != ERROR_SUCCESS) {
        LOG_WARNING("Failed to open Windows startup approval state, error=%ld",
                    status);
        return FALSE;
    }

    status = RegQueryValueExW(key, STARTUP_LINK_FILENAME, NULL, &type,
                              data, &dataSize);
    RegCloseKey(key);
    if (status == ERROR_FILE_NOT_FOUND) return TRUE;
    if (status != ERROR_SUCCESS) {
        LOG_WARNING("Failed to read Windows startup approval state, error=%ld",
                    status);
        return FALSE;
    }
    if (type != REG_BINARY || dataSize == 0) {
        LOG_WARNING("Ignoring malformed Windows startup approval state");
        return FALSE;
    }

    *disabled = StartupPolicy_IsWindowsStartupDisabled(data, dataSize);
    return TRUE;
}

static BOOL ClearStartupApprovedState(void) {
    HKEY key = NULL;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER,
                                   STARTUP_APPROVED_SUBKEY, 0,
                                   KEY_SET_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND) return TRUE;
    if (status != ERROR_SUCCESS) {
        LOG_WARNING("Failed to open Windows startup approval state for cleanup, error=%ld",
                    status);
        return FALSE;
    }

    status = RegDeleteValueW(key, STARTUP_LINK_FILENAME);
    RegCloseKey(key);
    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) return TRUE;

    LOG_WARNING("Failed to clear Windows startup approval state, error=%ld",
                status);
    return FALSE;
}

static void ReadAutoStartPreference(char* output, size_t outputSize) {
    char configPath[MAX_PATH] = {0};
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] != '\0') {
        ReadIniString(INI_SECTION_GENERAL, AUTO_START_PREFERENCE_KEY,
                      AUTO_START_PREFERENCE_DEFAULT, output,
                      (DWORD)outputSize, configPath);
    }
    if (_stricmp(output, AUTO_START_PREFERENCE_ENABLED) != 0 &&
        _stricmp(output, AUTO_START_PREFERENCE_DISABLED) != 0 &&
        _stricmp(output, AUTO_START_PREFERENCE_DEFAULT) != 0) {
        LOG_WARNING("Invalid auto-start preference '%s'; migrating as DEFAULT",
                    output);
        strcpy_s(output, outputSize, AUTO_START_PREFERENCE_DEFAULT);
    }
}

/** Centralizes timer reset to avoid repetition - uses high-precision timer */
static void RestartTimer(HWND hwnd, UINT interval) {
    MainTimer_Start(hwnd, interval);
}

static void StopTimer(HWND hwnd) {
    (void)hwnd;
    MainTimer_Stop();
}

static BOOL ReadStartupModeConfig(char* modeName, size_t modeNameSize) {
    char configPath[MAX_PATH];
    wchar_t wconfigPath[MAX_PATH];
    FILE* configFile;
    char line[CONFIG_LINE_BUFFER_SIZE];
    BOOL found = FALSE;
    UNREFERENCED_PARAMETER(modeNameSize);
    
    GetConfigPath(configPath, MAX_PATH);
    
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wconfigPath, MAX_PATH) == 0) {
        LOG_WARNING("Failed to convert config path for reading startup mode");
        return FALSE;
    }
    
    configFile = _wfopen(wconfigPath, L"r");
    if (!configFile) {
        LOG_WARNING("Failed to open config file for reading startup mode");
        return FALSE;
    }
    
    while (fgets(line, sizeof(line), configFile)) {
        if (strncmp(line, CONFIG_KEY_STARTUP_MODE, strlen(CONFIG_KEY_STARTUP_MODE)) == 0) {
            if (sscanf(line, "STARTUP_MODE=%19s", modeName) == 1) {
                found = TRUE;
                LOG_INFO("Read startup mode from config: %s", modeName);
                break;
            }
        }
    }
    
    fclose(configFile);
    return found;
}

/** Unified mode application via data-driven config */
static void ApplyModeConfig(HWND hwnd, const StartupModeConfig* config) {
    LOG_INFO("Applying startup mode: %s - %s", config->modeName, config->description);
    
    CLOCK_SHOW_CURRENT_TIME = config->showCurrentTime;
    CLOCK_COUNT_UP = config->countUp;
    CLOCK_TOTAL_TIME = (config->totalTime == -1) ? g_AppConfig.timer.default_start_time : config->totalTime;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;
    
    /* Initialize absolute time references for timer calculation */
    int64_t now = GetAbsoluteTimeMs();
    
    if (config->countUp) {
        g_start_time = now;
    } else if (CLOCK_TOTAL_TIME > 0) {
        g_target_end_time = now + ((int64_t)CLOCK_TOTAL_TIME * 1000);
    }
    
    ResetMillisecondAccumulator();
    
    if (config->enableTimer) {
        RestartTimer(hwnd, GetTimerInterval());
    } else {
        StopTimer(hwnd);
    }
}

static const StartupModeConfig* FindModeConfig(const char* modeName) {
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(modeName, STARTUP_MODE_CONFIGS[i].modeName) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    return NULL;
}

static const StartupModeConfig* GetDefaultModeConfig(void) {
    for (size_t i = 0; i < STARTUP_MODE_COUNT; i++) {
        if (strcmp(STARTUP_MODE_CONFIGS[i].modeName, MODE_NAME_DEFAULT) == 0) {
            return &STARTUP_MODE_CONFIGS[i];
        }
    }
    return &STARTUP_MODE_CONFIGS[0];
}

AutoStartStatus GetAutoStartStatus(void) {
    wchar_t startupPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};
    BOOL windowsDisabled = FALSE;

    if (IsRunningPackagedApp() ||
        !GetStartupShortcutPath(startupPath, _countof(startupPath))) {
        return AUTO_START_STATUS_ABSENT;
    }
    if (GetFileAttributesW(startupPath) == INVALID_FILE_ATTRIBUTES) {
        return AUTO_START_STATUS_ABSENT;
    }

    if (QueryStartupApprovedDisabled(&windowsDisabled) && windowsDisabled) {
        return AUTO_START_STATUS_DISABLED_BY_WINDOWS;
    }
    if (!GetExecutablePath(executablePath, _countof(executablePath)) ||
        !StartupShortcut_IsCurrent(startupPath, executablePath,
                                   STARTUP_CMD_ARG)) {
        return AUTO_START_STATUS_BROKEN;
    }
    return AUTO_START_STATUS_ENABLED;
}

BOOL IsAutoStartEnabled(void) {
    return GetAutoStartStatus() == AUTO_START_STATUS_ENABLED;
}

static BOOL CreateShortcutInternal(BOOL clearWindowsDisableState) {
    wchar_t startupPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};

    if (!GetExecutablePath(executablePath, _countof(executablePath)) ||
        !GetStartupShortcutPath(startupPath, _countof(startupPath))) {
        LOG_ERROR("Failed to prepare startup shortcut paths");
        return FALSE;
    }

    if (!StartupShortcut_ReplaceAtomically(startupPath, executablePath,
                                           STARTUP_CMD_ARG)) {
        LOG_ERROR("Failed to transactionally replace the startup shortcut");
        return FALSE;
    }

    RemoveLegacyStartupShortcutMarker();
    if (clearWindowsDisableState && !ClearStartupApprovedState()) {
        LOG_WARNING("Startup shortcut was repaired but Windows still reports it disabled");
        return FALSE;
    }
    if (!WriteAutoStartPreference(AUTO_START_PREFERENCE_ENABLED)) {
        LOG_WARNING("Startup shortcut is active but its preference was not persisted");
    }
    LOG_INFO("Startup shortcut created and verified successfully: %ls",
             startupPath);
    return TRUE;
}

static BOOL CreateShortcutWithIntent(BOOL clearWindowsDisableState) {
    StartupShortcutLock lock = {0};
    BOOL result;
    if (!AcquireStartupShortcutLock(&lock)) return FALSE;
    result = CreateShortcutInternal(clearWindowsDisableState);
    ReleaseStartupShortcutLock(&lock);
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
    if (!AcquireStartupShortcutLock(&lock)) return FALSE;
    if (!WriteAutoStartPreference(AUTO_START_PREFERENCE_ENABLED)) {
        ReleaseStartupShortcutLock(&lock);
        return FALSE;
    }
    result = CreateShortcutInternal(TRUE);
    ReleaseStartupShortcutLock(&lock);
    return result;
}

static BOOL RemoveShortcutFileInternal(void) {
    wchar_t startupPath[MAX_PATH] = {0};
    if (!GetStartupShortcutPath(startupPath, _countof(startupPath))) return FALSE;

    if (!DeleteFileW(startupPath)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            LOG_ERROR("Failed to delete startup shortcut, error=%lu", error);
            return FALSE;
        }
    }

    RemoveLegacyStartupShortcutMarker();
    if (!ClearStartupApprovedState()) {
        LOG_WARNING("Could not clean the stale Windows startup approval state");
    }
    LOG_INFO("Startup shortcut removed successfully");
    return TRUE;
}

BOOL RemoveShortcut(void) {
    StartupShortcutLock lock = {0};
    BOOL result;
    if (IsRunningPackagedApp()) return OpenStartupSettings();
    if (!AcquireStartupShortcutLock(&lock)) return FALSE;
    result = RemoveShortcutFileInternal();
    if (result &&
        !WriteAutoStartPreference(AUTO_START_PREFERENCE_DISABLED)) {
        LOG_WARNING("Startup shortcut was removed but its preference was not persisted");
    }
    ReleaseStartupShortcutLock(&lock);
    return result;
}

BOOL DisableAutoStart(void) {
    StartupShortcutLock lock = {0};
    BOOL result;
    if (IsRunningPackagedApp()) return OpenStartupSettings();
    if (!AcquireStartupShortcutLock(&lock)) return FALSE;
    if (!WriteAutoStartPreference(AUTO_START_PREFERENCE_DISABLED)) {
        ReleaseStartupShortcutLock(&lock);
        return FALSE;
    }

    result = RemoveShortcutFileInternal();
    if (!result) {
        (void)WriteAutoStartPreference(AUTO_START_PREFERENCE_ENABLED);
    }
    ReleaseStartupShortcutLock(&lock);
    return result;
}

BOOL RepairExistingAutoStartShortcut(void) {
    char preference[32] = {0};
    AutoStartPreference preferenceState;
    BOOL shortcutExists;
    wchar_t startupPath[MAX_PATH] = {0};
    wchar_t exePath[MAX_PATH] = {0};

    if (IsRunningPackagedApp()) return TRUE;
    if (!GetStartupShortcutPath(startupPath, _countof(startupPath))) return FALSE;

    shortcutExists = GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES;
    ReadAutoStartPreference(preference, sizeof(preference));
    preferenceState = StartupPolicy_ParsePreference(preference);

    if (preferenceState == AUTO_START_PREFERENCE_STATE_DISABLED) {
        return shortcutExists ? RemoveShortcut() : TRUE;
    }

    if (!shortcutExists) {
        if (preferenceState == AUTO_START_PREFERENCE_STATE_ENABLED) {
            LOG_INFO("Restoring explicitly enabled startup shortcut before single-instance routing");
            return CreateShortcut();
        }

        /* FIRST_RUN is tied to font extraction and may remain TRUE after an
         * old installation. Version detection is the reliable migration
         * signal before ReadConfig rewrites CONFIG_VERSION. */
        char configPath[MAX_PATH] = {0};
        GetConfigPath(configPath, sizeof(configPath));
        if (configPath[0] != '\0' && FileExists(configPath)) {
            char configVersion[32] = {0};
            ReadIniString(INI_SECTION_GENERAL, "CONFIG_VERSION", "",
                          configVersion, sizeof(configVersion), configPath);
            if (strcmp(configVersion, CATIME_VERSION) != 0) {
                LOG_INFO("Preserving legacy auto-start opt-out during upgrade from %s",
                         configVersion[0] ? configVersion : "an unversioned config");
                return WriteAutoStartPreference(AUTO_START_PREFERENCE_DISABLED);
            }
        }
        return TRUE;
    }

    if (!GetExecutablePath(exePath, _countof(exePath))) return FALSE;
    if (StartupShortcut_IsCurrent(startupPath, exePath,
                                  STARTUP_CMD_ARG)) return TRUE;

    LOG_INFO("Repairing stale startup shortcut before single-instance routing");
    return CreateShortcut();
}

BOOL EnsureAutoStart(void) {
    char preference[32] = {0};
    AutoStartPreference preferenceState;
    BOOL shortcutExists;
    BOOL shouldEnable;
    BOOL firstRun;
    wchar_t startupPath[MAX_PATH] = {0};
    wchar_t exePath[MAX_PATH] = {0};

    LOG_INFO("Ensuring auto-start registration and preference");
    if (IsRunningPackagedApp()) {
        LOG_INFO("Packaged app uses the Windows Startup Apps registration");
        return TRUE;
    }

    if (!GetStartupShortcutPath(startupPath, _countof(startupPath)) ||
        !GetExecutablePath(exePath, _countof(exePath))) {
        return FALSE;
    }

    shortcutExists = GetFileAttributesW(startupPath) != INVALID_FILE_ATTRIBUTES;
    ReadAutoStartPreference(preference, sizeof(preference));
    preferenceState = StartupPolicy_ParsePreference(preference);
    firstRun = IsFirstRun();
    shouldEnable = StartupPolicy_ShouldEnable(preferenceState, firstRun,
                                              shortcutExists);

    if (preferenceState == AUTO_START_PREFERENCE_STATE_DEFAULT) {
        /* New installs default on. Legacy installs adopt the shortcut's
         * existing presence so a prior opt-out is never silently reversed. */
        LOG_INFO("Migrating auto-start preference: firstRun=%d shortcutExists=%d -> %s",
                 firstRun, shortcutExists,
                 shouldEnable ? AUTO_START_PREFERENCE_ENABLED
                              : AUTO_START_PREFERENCE_DISABLED);
        if (shouldEnable &&
            !WriteAutoStartPreference(AUTO_START_PREFERENCE_ENABLED)) {
            LOG_WARNING("Could not record the default enabled auto-start intent for retry");
        }
    }

    if (!shouldEnable) {
        if (shortcutExists) {
            return RemoveShortcut();
        }
        RemoveLegacyStartupShortcutMarker();
        return WriteAutoStartPreference(AUTO_START_PREFERENCE_DISABLED);
    }

    if (shortcutExists &&
        StartupShortcut_IsCurrent(startupPath, exePath, STARTUP_CMD_ARG)) {
        RemoveLegacyStartupShortcutMarker();
        return WriteAutoStartPreference(AUTO_START_PREFERENCE_ENABLED);
    }

    if (!CreateShortcut()) {
        LOG_ERROR("Failed to create or repair startup shortcut; existing shortcut was preserved when possible");
        return FALSE;
    }

    LOG_INFO("Startup shortcut created or repaired successfully");
    return TRUE;
}

/** Compatibility wrapper for older callers. */
BOOL UpdateStartupShortcut(void) {
    return EnsureAutoStart();
}

BOOL OpenStartupSettings(void) {
    HINSTANCE result = ShellExecuteW(NULL, L"open", L"ms-settings:startupapps",
                                     NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        LOG_WARNING("Failed to open Windows Startup Apps settings");
        return FALSE;
    }

    LOG_INFO("Opened Windows Startup Apps settings");
    return TRUE;
}

BOOL OpenPackagedStartupSettings(void) {
    return OpenStartupSettings();
}

/** Reads STARTUP_MODE from config, falls back to DEFAULT if not found */
void ApplyStartupMode(HWND hwnd) {
    char modeName[STARTUP_MODE_MAX_LEN] = {0};
    const StartupModeConfig* config = NULL;
    
    LOG_INFO("Applying startup mode configuration");
    
    if (ReadStartupModeConfig(modeName, sizeof(modeName))) {
        config = FindModeConfig(modeName);
        if (!config) {
            LOG_WARNING("Unknown startup mode '%s', using default", modeName);
            config = GetDefaultModeConfig();
        }
    } else {
        LOG_INFO("No startup mode configured, using default");
        config = GetDefaultModeConfig();
    }
    
    ApplyModeConfig(hwnd, config);
    InvalidateRect(hwnd, NULL, TRUE);
}
