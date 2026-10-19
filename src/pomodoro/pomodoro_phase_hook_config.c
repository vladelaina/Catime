#include "pomodoro_phase_hook_config.h"
#include "config.h"
#include "utils/path_utils.h"
#include "utils/string_convert.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    POMODORO_PHASE phase;
    const char* configFile;
    const char* section;
    const char* defaultColor;
} PomodoroPhaseHookDefinition;

/* Each phase hook owns its configuration path, relative to the main config directory. */
static const PomodoroPhaseHookDefinition PHASE_HOOKS[] = {
    {POMODORO_PHASE_WORK, "pomodoro_hooks.ini", "Work", POMODORO_HOOK_DEFAULT},
    {POMODORO_PHASE_BREAK, "pomodoro_hooks.ini", "ShortBreak", "#89F7FE_#66A6FF"},
    {POMODORO_PHASE_LONG_BREAK, "pomodoro_hooks.ini", "LongBreak", "#10BD92_#8CE442"}
};

static const PomodoroPhaseHookDefinition* GetPhaseHook(POMODORO_PHASE phase) {
    for (size_t i = 0; i < _countof(PHASE_HOOKS); ++i) {
        if (PHASE_HOOKS[i].phase == phase) return &PHASE_HOOKS[i];
    }
    return NULL;
}

BOOL GetPomodoroPhaseHookConfigPath(POMODORO_PHASE phase, char* output, size_t outputSize) {
    const PomodoroPhaseHookDefinition* hook = GetPhaseHook(phase);
    if (!hook || !output || outputSize == 0) return FALSE;

    char mainConfigPath[MAX_PATH];
    GetConfigPath(mainConfigPath, sizeof(mainConfigPath));
    return ExtractDirectoryU8(mainConfigPath, output, outputSize) &&
           PathJoinU8(output, outputSize, hook->configFile);
}

static BOOL EnsureHookConfigFile(const PomodoroPhaseHookDefinition* hook, const char* path) {
    wchar_t widePath[MAX_PATH];
    if (!Utf8ToWide(path, widePath, _countof(widePath))) return FALSE;

    DWORD attributes = GetFileAttributesW(widePath);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
    if (GetLastError() != ERROR_FILE_NOT_FOUND) return FALSE;

    char content[1024] =
        "; Catime Pomodoro phase hooks. Changes apply on the next phase entry.\r\n"
        "; Color=DEFAULT follows the current clock color.\r\n"
        "; SoundFile=SYSTEM_BEEP uses the system sound; empty means silent.\r\n"
        "; SoundFile=DEFAULT uses the notification sound, or system sound if unset.\r\n"
        "; Relative sound paths are resolved beside this file.\r\n";
    for (size_t i = 0; i < _countof(PHASE_HOOKS); ++i) {
        const PomodoroPhaseHookDefinition* phaseHook = &PHASE_HOOKS[i];
        if (strcmp(phaseHook->configFile, hook->configFile) != 0) continue;
        size_t used = strlen(content);
        if (_snprintf_s(content + used, sizeof(content) - used, _TRUNCATE,
                        "\r\n[%s]\r\nColor=%s\r\nSoundFile=%s\r\n",
                        phaseHook->section, phaseHook->defaultColor,
                        POMODORO_HOOK_DEFAULT_SOUND) < 0) {
            return FALSE;
        }
    }

    /* CREATE_NEW preserves user settings if another process creates the file first. */
    HANDLE file = CreateFileW(widePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS;
    DWORD length = (DWORD)strlen(content);
    DWORD written = 0;
    BOOL result = WriteFile(file, content, length, &written, NULL) && written == length;
    CloseHandle(file);
    if (!result) DeleteFileW(widePath);
    return result;
}

static BOOL NormalizeHookColor(const char* input, char* output, size_t outputSize) {
    if (_stricmp(input, POMODORO_HOOK_DEFAULT) == 0) {
        return strcpy_s(output, outputSize, POMODORO_HOOK_DEFAULT) == 0;
    }
    if (!isValidColorOrGradient(input)) return FALSE;

    char normalized[COLOR_HEX_BUFFER];
    if (strchr(input, '_')) {
        strcpy_s(normalized, sizeof(normalized), input);
    } else if (input[0] != '#' && _strnicmp(input, "rgb", 3) != 0 &&
               strpbrk(input, ",;| ")) {
        /* Mark bare RGB explicitly so its digits cannot be mistaken for hex. */
        char rgbInput[COLOR_HEX_BUFFER + 5];
        if (_snprintf_s(rgbInput, sizeof(rgbInput), _TRUNCATE, "rgb(%s)", input) < 0) {
            return FALSE;
        }
        normalizeColor(rgbInput, normalized, sizeof(normalized));
    } else {
        normalizeColor(input, normalized, sizeof(normalized));
    }
    if (!isValidColorOrGradient(normalized)) return FALSE;
    ReplaceBlackColor(normalized, output, outputSize);
    return TRUE;
}

static BOOL ResolveHookSoundFile(const char* input, const char* configPath,
                                 char* output, size_t outputSize) {
    if (_stricmp(input, POMODORO_HOOK_DEFAULT) == 0 ||
        _stricmp(input, POMODORO_HOOK_DEFAULT_SOUND) == 0 || input[0] == '\0') {
        const char* value = input[0] == '\0' ? "" :
            (_stricmp(input, POMODORO_HOOK_DEFAULT_SOUND) == 0
                ? POMODORO_HOOK_DEFAULT_SOUND : POMODORO_HOOK_DEFAULT);
        return strcpy_s(output, outputSize, value) == 0;
    }

    wchar_t wideInput[MAX_PATH], wideExpanded[MAX_PATH];
    char expanded[MAX_PATH];
    if (!Utf8ToWide(input, wideInput, _countof(wideInput))) return FALSE;
    DWORD length = ExpandEnvironmentStringsW(wideInput, wideExpanded, _countof(wideExpanded));
    if (length == 0 || length > _countof(wideExpanded) ||
        !WideToUtf8(wideExpanded, expanded, sizeof(expanded))) return FALSE;

    if (expanded[0] == '\\' || expanded[0] == '/' ||
        (expanded[0] != '\0' && expanded[1] == ':')) {
        return strcpy_s(output, outputSize, expanded) == 0;
    }
    return ExtractDirectoryU8(configPath, output, outputSize) &&
           PathJoinU8(output, outputSize, expanded);
}

BOOL LoadPomodoroPhaseHookConfig(POMODORO_PHASE phase, PomodoroPhaseHookConfig* config) {
    const PomodoroPhaseHookDefinition* hook = GetPhaseHook(phase);
    if (!hook || !config) return FALSE;

    strcpy_s(config->color, sizeof(config->color), hook->defaultColor);
    strcpy_s(config->soundFile, sizeof(config->soundFile), POMODORO_HOOK_DEFAULT_SOUND);

    char path[MAX_PATH];
    if (!GetPomodoroPhaseHookConfigPath(phase, path, sizeof(path)) ||
        !EnsureHookConfigFile(hook, path)) {
        LOG_WARNING("Could not open Pomodoro hook config for %s; using defaults", hook->section);
        return TRUE;
    }

    char value[COLOR_HEX_BUFFER];
    if (!ReadIniStringExact(hook->section, "Color", hook->defaultColor,
                            value, sizeof(value), path) ||
        !NormalizeHookColor(value, config->color, sizeof(config->color))) {
        LOG_WARNING("Invalid Pomodoro hook color for %s; using default", hook->section);
    }

    char soundValue[MAX_PATH], resolvedSound[MAX_PATH];
    if (ReadIniStringExact(hook->section, "SoundFile", POMODORO_HOOK_DEFAULT_SOUND,
                           soundValue, sizeof(soundValue), path) &&
        ResolveHookSoundFile(soundValue, path, resolvedSound, sizeof(resolvedSound))) {
        strcpy_s(config->soundFile, sizeof(config->soundFile), resolvedSound);
    } else {
        LOG_WARNING("Invalid Pomodoro hook sound path for %s; using default", hook->section);
    }
    return TRUE;
}
