#include "config.h"
#include "hotkey.h"
#include <windows.h>
typedef struct {
    const char* key;
    WORD* value;
} HotkeyConfigEntry;
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                       WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                       WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                       WORD* pauseResumeHotkey, WORD* restartTimerHotkey, WORD* toggleMillisecondsHotkey,
                       WORD* toggleTopmostHotkey)
{
    if (!showTimeHotkey || !countUpHotkey || !countdownHotkey ||
        !quickCountdown1Hotkey || !quickCountdown2Hotkey || !quickCountdown3Hotkey ||
        !pomodoroHotkey || !toggleVisibilityHotkey || !editModeHotkey ||
        !pauseResumeHotkey || !restartTimerHotkey || !toggleMillisecondsHotkey ||
        !toggleTopmostHotkey) return;
    HotkeyConfigEntry entries[] = {
        {"HOTKEY_SHOW_TIME",           showTimeHotkey},
        {"HOTKEY_COUNT_UP",            countUpHotkey},
        {"HOTKEY_COUNTDOWN",           countdownHotkey},
        {"HOTKEY_QUICK_COUNTDOWN1",    quickCountdown1Hotkey},
        {"HOTKEY_QUICK_COUNTDOWN2",    quickCountdown2Hotkey},
        {"HOTKEY_QUICK_COUNTDOWN3",    quickCountdown3Hotkey},
        {"HOTKEY_POMODORO",            pomodoroHotkey},
        {"HOTKEY_TOGGLE_VISIBILITY",   toggleVisibilityHotkey},
        {"HOTKEY_EDIT_MODE",           editModeHotkey},
        {"HOTKEY_PAUSE_RESUME",        pauseResumeHotkey},
        {"HOTKEY_RESTART_TIMER",       restartTimerHotkey},
        {"HOTKEY_TOGGLE_MILLISECONDS", toggleMillisecondsHotkey},
        {"HOTKEY_TOPMOST",             toggleTopmostHotkey},
    };
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        char hotkeyStr[64];
        ReadIniString(INI_SECTION_HOTKEYS, entries[i].key, "None",
                     hotkeyStr, sizeof(hotkeyStr), config_path);
        WORD parsedHotkey = NormalizeHotkeyValue(StringToHotkey(hotkeyStr));
        *(entries[i].value) = IsHotkeyValueAllowed(parsedHotkey)
                              ? parsedHotkey
                              : 0;
    }
}
BOOL WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD customCountdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey, WORD toggleMillisecondsHotkey,
                        WORD toggleTopmostHotkey) {
    struct {
        const char* key;
        WORD value;
    } entries[] = {
        {"HOTKEY_SHOW_TIME",           showTimeHotkey},
        {"HOTKEY_COUNT_UP",            countUpHotkey},
        {"HOTKEY_COUNTDOWN",           countdownHotkey},
        {"HOTKEY_QUICK_COUNTDOWN1",    quickCountdown1Hotkey},
        {"HOTKEY_QUICK_COUNTDOWN2",    quickCountdown2Hotkey},
        {"HOTKEY_QUICK_COUNTDOWN3",    quickCountdown3Hotkey},
        {"HOTKEY_POMODORO",            pomodoroHotkey},
        {"HOTKEY_TOGGLE_VISIBILITY",   toggleVisibilityHotkey},
        {"HOTKEY_EDIT_MODE",           editModeHotkey},
        {"HOTKEY_PAUSE_RESUME",        pauseResumeHotkey},
        {"HOTKEY_RESTART_TIMER",       restartTimerHotkey},
        {"HOTKEY_CUSTOM_COUNTDOWN",    customCountdownHotkey},
        {"HOTKEY_TOGGLE_MILLISECONDS", toggleMillisecondsHotkey},
        {"HOTKEY_TOPMOST",             toggleTopmostHotkey},
    };
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    enum { HOTKEY_WRITE_COUNT = sizeof(entries) / sizeof(entries[0]) };
    char hotkeyStrings[HOTKEY_WRITE_COUNT][64];
    IniKeyValue updates[HOTKEY_WRITE_COUNT];
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        WORD normalizedValue = NormalizeHotkeyValue(entries[i].value);
        WORD value = IsHotkeyValueAllowed(normalizedValue)
                     ? normalizedValue
                     : 0;
        if (value != 0) {
            for (size_t previous = 0; previous < i; ++previous) {
                if (entries[previous].value == value) {
                    value = 0;
                    break;
                }
            }
        }
        entries[i].value = value;
        HotkeyToString(value, hotkeyStrings[i], sizeof(hotkeyStrings[i]));
        updates[i].section = INI_SECTION_HOTKEYS;
        updates[i].key = entries[i].key;
        updates[i].value = hotkeyStrings[i];
    }
    return WriteIniMultipleAtomic(config_path, updates, HOTKEY_WRITE_COUNT);
}
void ReadCustomCountdownHotkey(WORD* hotkey) {
    if (!hotkey) return;
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    char hotkeyStr[64];
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None",
                  hotkeyStr, sizeof(hotkeyStr), config_path);
    WORD parsedHotkey = NormalizeHotkeyValue(StringToHotkey(hotkeyStr));
    *hotkey = IsHotkeyValueAllowed(parsedHotkey) ? parsedHotkey : 0;
}
