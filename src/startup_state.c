/**
 * @file startup_state.c
 * @brief Persisted startup preference and Windows approval state
 */
#include "startup_internal.h"

#include "config.h"
#include "log.h"
#include "startup_policy.h"

#include <string.h>

#define STARTUP_APPROVED_SUBKEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder"
#define AUTO_START_PREFERENCE_KEY "AUTO_START_PREFERENCE"

BOOL StartupState_QueryWindowsDisabled(BOOL* disabled) {
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

BOOL StartupState_ClearWindowsApproval(void) {
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

BOOL StartupState_WritePreference(const char* preference) {
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

void StartupState_ReadPreference(char* output, size_t outputSize) {
    char configPath[MAX_PATH] = {0};

    if (!output || outputSize == 0 || outputSize > MAXDWORD) return;
    strcpy_s(output, outputSize, AUTO_START_PREFERENCE_DEFAULT);
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
