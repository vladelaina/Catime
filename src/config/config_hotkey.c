/**
 * @file config_hotkey.c
 * @brief Hotkey configuration management
 * 
 * Manages hotkey configuration including string conversion, reading/writing, and validation.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <windows.h>

/**
 * @brief Virtual Key Code to String mapping table
 */
typedef struct {
    BYTE vk;
    const char* name;
} VKeyMapping;

static const VKeyMapping g_vkeyMap[] = {
    {VK_BACK,      "Backspace"},
    {VK_TAB,       "Tab"},
    {VK_RETURN,    "Enter"},
    {VK_ESCAPE,    "Esc"},
    {VK_SPACE,     "Space"},
    {VK_PRIOR,     "PageUp"},
    {VK_NEXT,      "PageDown"},
    {VK_END,       "End"},
    {VK_HOME,      "Home"},
    {VK_LEFT,      "Left"},
    {VK_UP,        "Up"},
    {VK_RIGHT,     "Right"},
    {VK_DOWN,      "Down"},
    {VK_INSERT,    "Insert"},
    {VK_DELETE,    "Delete"},
    {VK_NUMPAD0,   "Num0"},
    {VK_NUMPAD1,   "Num1"},
    {VK_NUMPAD2,   "Num2"},
    {VK_NUMPAD3,   "Num3"},
    {VK_NUMPAD4,   "Num4"},
    {VK_NUMPAD5,   "Num5"},
    {VK_NUMPAD6,   "Num6"},
    {VK_NUMPAD7,   "Num7"},
    {VK_NUMPAD8,   "Num8"},
    {VK_NUMPAD9,   "Num9"},
    {VK_MULTIPLY,  "Num*"},
    {VK_ADD,       "Num+"},
    {VK_SUBTRACT,  "Num-"},
    {VK_DECIMAL,   "Num."},
    {VK_DIVIDE,    "Num/"},
    {VK_OEM_1,     ";"},
    {VK_OEM_PLUS,  "="},
    {VK_OEM_COMMA, ","},
    {VK_OEM_MINUS, "-"},
    {VK_OEM_PERIOD, "."},
    {VK_OEM_2,     "/"},
    {VK_OEM_3,     "`"},
    {VK_OEM_4,     "["},
    {VK_OEM_5,     "\\"},
    {VK_OEM_6,     "]"},
    {VK_OEM_7,     "'"},
    {0, NULL}
};

/**
 * @brief Convert hotkey code to human-readable string
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    /** Handle empty hotkey */
    if (hotkey == 0) {
        strncpy(buffer, "None", bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }
    
    BYTE vk = LOBYTE(hotkey);
    BYTE mod = HIBYTE(hotkey);
    
    buffer[0] = '\0';
    size_t len = 0;
    
    /** Build modifier string */
    if (mod & HOTKEYF_CONTROL) {
        strncpy(buffer, "Ctrl", bufferSize - 1);
        len = strlen(buffer);
    }
    
    if (mod & HOTKEYF_SHIFT) {
        if (len > 0 && len < bufferSize - 1) {
            buffer[len++] = '+';
            buffer[len] = '\0';
        }
        strncat(buffer, "Shift", bufferSize - len - 1);
        len = strlen(buffer);
    }
    
    if (mod & HOTKEYF_ALT) {
        if (len > 0 && len < bufferSize - 1) {
            buffer[len++] = '+';
            buffer[len] = '\0';
        }
        strncat(buffer, "Alt", bufferSize - len - 1);
        len = strlen(buffer);
    }
    
    /** Add separator before key name */
    if (len > 0 && len < bufferSize - 1 && vk != 0) {
        buffer[len++] = '+';
        buffer[len] = '\0';
    }
    
    /** Handle alphanumeric keys */
    if (vk >= 'A' && vk <= 'Z') {
        const char keyName[2] = {(char)vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= '0' && vk <= '9') {
        const char keyName[2] = {(char)vk, '\0'};
        strncat(buffer, keyName, bufferSize - len - 1);
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        /** Handle function keys */
        char keyName[8];
        snprintf(keyName, sizeof(keyName), "F%d", vk - VK_F1 + 1);
        strncat(buffer, keyName, bufferSize - len - 1);
    } else {
        /** Look up in mapping table */
        const char* keyName = NULL;
        for (int i = 0; g_vkeyMap[i].name != NULL; i++) {
            if (g_vkeyMap[i].vk == vk) {
                keyName = g_vkeyMap[i].name;
                break;
            }
        }
        
        if (keyName) {
            strncat(buffer, keyName, bufferSize - len - 1);
        } else {
            /** Fallback to hex code for unknown keys */
            char hexKey[8];
            snprintf(hexKey, sizeof(hexKey), "0x%02X", vk);
            strncat(buffer, hexKey, bufferSize - len - 1);
        }
    }
}

static BOOL ParseFunctionKeyToken(const char* token, BYTE* vk) {
    if (!token || !vk || token[0] != 'F' || !isdigit((unsigned char)token[1])) {
        return FALSE;
    }

    errno = 0;
    char* end = NULL;
    long fNum = strtol(token + 1, &end, 10);
    if (end == token + 1 || *end != '\0' || errno == ERANGE ||
        fNum < 1 || fNum > 24) {
        return FALSE;
    }

    *vk = (BYTE)(VK_F1 + fNum - 1);
    return TRUE;
}

static BOOL ParseHexVirtualKeyToken(const char* token, BYTE* vk) {
    if (!token || !vk || strncmp(token, "0x", 2) != 0 || token[2] == '\0') {
        return FALSE;
    }

    errno = 0;
    char* end = NULL;
    long parsed = strtol(token + 2, &end, 16);
    if (end == token + 2 || *end != '\0' || errno == ERANGE ||
        parsed <= 0 || parsed > UCHAR_MAX) {
        return FALSE;
    }

    *vk = (BYTE)parsed;
    return TRUE;
}


/**
 * @brief Parse human-readable hotkey string to Windows hotkey code
 */
WORD StringToHotkey(const char* str) {
    if (!str || str[0] == '\0' || strcmp(str, "None") == 0) {
        return 0;
    }
    
    BYTE vk = 0;
    BYTE mod = 0;
    
    /** Create mutable copy for tokenization */
    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    /** Parse modifier and key components */
    const char* token = strtok(buffer, "+");
    const char* lastToken = NULL;
    
    while (token) {
        if (_stricmp(token, "Ctrl") == 0) {
            mod |= HOTKEYF_CONTROL;
        } else if (_stricmp(token, "Shift") == 0) {
            mod |= HOTKEYF_SHIFT;
        } else if (_stricmp(token, "Alt") == 0) {
            mod |= HOTKEYF_ALT;
        } else {
            /** Last token is the key name */
            lastToken = token;
        }
        token = strtok(NULL, "+");
    }
    
    if (lastToken) {
        /** Handle single character keys (A-Z, 0-9) */
        if (strlen(lastToken) == 1) {
            int ch = toupper((unsigned char)lastToken[0]);
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                vk = (BYTE)ch;
            }
        } 
        /** Handle function keys (F1-F24) */
        else if (ParseFunctionKeyToken(lastToken, &vk)) {
            /** Parsed above */
        }
        /** Handle hex format (0xNN) */
        else if (ParseHexVirtualKeyToken(lastToken, &vk)) {
            /** Parsed above */
        }
        /** Look up in mapping table */
        else {
            for (int i = 0; g_vkeyMap[i].name != NULL; i++) {
                if (_stricmp(lastToken, g_vkeyMap[i].name) == 0) {
                    vk = g_vkeyMap[i].vk;
                    break;
                }
            }
        }
    }
    
    return MAKEWORD(vk, mod);
}


/**
 * @brief Hotkey configuration entry (for data-driven read/write)
 */
typedef struct {
    const char* key;
    WORD* value;
} HotkeyConfigEntry;

/**
 * @brief Read all hotkey configurations from config file
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                       WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                       WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                       WORD* pauseResumeHotkey, WORD* restartTimerHotkey, WORD* toggleMillisecondsHotkey,
                       WORD* toggleTopmostHotkey)
{
    /** Validate all pointers */
    if (!showTimeHotkey || !countUpHotkey || !countdownHotkey || 
        !quickCountdown1Hotkey || !quickCountdown2Hotkey || !quickCountdown3Hotkey ||
        !pomodoroHotkey || !toggleVisibilityHotkey || !editModeHotkey || 
        !pauseResumeHotkey || !restartTimerHotkey || !toggleMillisecondsHotkey ||
        !toggleTopmostHotkey) return;
    
    /** Data-driven hotkey configuration table */
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
    
    /** Read all hotkeys using data-driven approach */
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        char hotkeyStr[64];
        ReadIniString(INI_SECTION_HOTKEYS, entries[i].key, "None", 
                     hotkeyStr, sizeof(hotkeyStr), config_path);
        *(entries[i].value) = StringToHotkey(hotkeyStr);
    }
}


void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD customCountdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey, WORD toggleMillisecondsHotkey,
                        WORD toggleTopmostHotkey) {
    /** Data-driven hotkey configuration table with values */
    struct {
        const char* key;
        WORD value;
    } entries[] = {
        {"HOTKEY_SHOW_TIME",           showTimeHotkey},
        {"HOTKEY_COUNT_UP",            countUpHotkey},
        {"HOTKEY_COUNTDOWN",           countdownHotkey},
        {"HOTKEY_CUSTOM_COUNTDOWN",    customCountdownHotkey},
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

    enum { HOTKEY_WRITE_COUNT = sizeof(entries) / sizeof(entries[0]) };
    char hotkeyStrings[HOTKEY_WRITE_COUNT][64];
    IniKeyValue updates[HOTKEY_WRITE_COUNT];

    /** Write all hotkeys using data-driven approach */
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
        HotkeyToString(entries[i].value, hotkeyStrings[i], sizeof(hotkeyStrings[i]));
        updates[i].section = INI_SECTION_HOTKEYS;
        updates[i].key = entries[i].key;
        updates[i].value = hotkeyStrings[i];
    }

    WriteIniMultipleAtomic(config_path, updates, HOTKEY_WRITE_COUNT);
}


void ReadCustomCountdownHotkey(WORD* hotkey) {
    if (!hotkey) return;

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    char hotkeyStr[64];
    ReadIniString(INI_SECTION_HOTKEYS, "HOTKEY_CUSTOM_COUNTDOWN", "None",
                  hotkeyStr, sizeof(hotkeyStr), config_path);
    *hotkey = StringToHotkey(hotkeyStr);
}
