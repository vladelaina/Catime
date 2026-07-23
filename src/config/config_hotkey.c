#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <windows.h>
#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif
#ifndef VK_IME_SHIFT
#define VK_IME_SHIFT 0xE5
#endif
#define HOTKEY_SUPPORTED_MODIFIERS (HOTKEYF_SHIFT | HOTKEYF_CONTROL | HOTKEYF_ALT)
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
static void AppendHotkeyPart(char* buffer, size_t bufferSize, size_t* len, const char* part) {
    if (!buffer || !len || !part || bufferSize == 0 || *len >= bufferSize - 1) {
        return;
    }
    size_t remaining = bufferSize - *len - 1;
    size_t partLen = strlen(part);
    if (partLen > remaining) {
        partLen = remaining;
    }
    memcpy(buffer + *len, part, partLen);
    *len += partLen;
    buffer[*len] = '\0';
}
static void AppendHotkeySeparator(char* buffer, size_t bufferSize, size_t* len) {
    if (!buffer || !len || bufferSize == 0 || *len >= bufferSize - 1) {
        return;
    }
    buffer[(*len)++] = '+';
    buffer[*len] = '\0';
}
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    if (hotkey == 0) {
        strncpy(buffer, "None", bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
        return;
    }
    BYTE vk = LOBYTE(hotkey);
    BYTE mod = HIBYTE(hotkey);
    buffer[0] = '\0';
    size_t len = 0;
    if (mod & HOTKEYF_CONTROL) {
        AppendHotkeyPart(buffer, bufferSize, &len, "Ctrl");
    }
    if (mod & HOTKEYF_SHIFT) {
        if (len > 0) {
            AppendHotkeySeparator(buffer, bufferSize, &len);
        }
        AppendHotkeyPart(buffer, bufferSize, &len, "Shift");
    }
    if (mod & HOTKEYF_ALT) {
        if (len > 0) {
            AppendHotkeySeparator(buffer, bufferSize, &len);
        }
        AppendHotkeyPart(buffer, bufferSize, &len, "Alt");
    }
    if (len > 0 && vk != 0) {
        AppendHotkeySeparator(buffer, bufferSize, &len);
    }
    if (vk >= 'A' && vk <= 'Z') {
        const char keyName[2] = {(char)vk, '\0'};
        AppendHotkeyPart(buffer, bufferSize, &len, keyName);
    } else if (vk >= '0' && vk <= '9') {
        const char keyName[2] = {(char)vk, '\0'};
        AppendHotkeyPart(buffer, bufferSize, &len, keyName);
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        char keyName[8];
        snprintf(keyName, sizeof(keyName), "F%d", vk - VK_F1 + 1);
        AppendHotkeyPart(buffer, bufferSize, &len, keyName);
    } else {
        const char* keyName = NULL;
        for (int i = 0; g_vkeyMap[i].name != NULL; i++) {
            if (g_vkeyMap[i].vk == vk) {
                keyName = g_vkeyMap[i].name;
                break;
            }
        }
        if (keyName) {
            AppendHotkeyPart(buffer, bufferSize, &len, keyName);
        } else {
            char hexKey[8];
            snprintf(hexKey, sizeof(hexKey), "0x%02X", vk);
            AppendHotkeyPart(buffer, bufferSize, &len, hexKey);
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
static char* TrimHotkeyToken(char* token) {
    if (!token) {
        return NULL;
    }
    while (isspace((unsigned char)*token)) {
        token++;
    }
    char* end = token + strlen(token);
    while (end > token && isspace((unsigned char)*(end - 1))) {
        *(--end) = '\0';
    }
    return token;
}
static BOOL ParseHexVirtualKeyToken(const char* token, BYTE* vk) {
    if (!token || !vk || _strnicmp(token, "0x", 2) != 0 || token[2] == '\0') {
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
static BOOL IsModifierVirtualKey(BYTE vk) {
    switch (vk) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return TRUE;
        default:
            return FALSE;
    }
}
WORD StringToHotkey(const char* str) {
    if (!str) {
        return 0;
    }
    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char* input = TrimHotkeyToken(buffer);
    if (!input || input[0] == '\0' || _stricmp(input, "None") == 0) {
        return 0;
    }
    BYTE vk = 0;
    BYTE mod = 0;
    char* token = strtok(input, "+");
    const char* lastToken = NULL;
    while (token) {
        char* part = TrimHotkeyToken(token);
        if (!part || part[0] == '\0') {
            token = strtok(NULL, "+");
            continue;
        }
        if (_stricmp(part, "Ctrl") == 0) {
            mod |= HOTKEYF_CONTROL;
        } else if (_stricmp(part, "Shift") == 0) {
            mod |= HOTKEYF_SHIFT;
        } else if (_stricmp(part, "Alt") == 0) {
            mod |= HOTKEYF_ALT;
        } else {
            lastToken = part;
        }
        token = strtok(NULL, "+");
    }
    if (lastToken) {
        if (strlen(lastToken) == 1) {
            int ch = toupper((unsigned char)lastToken[0]);
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                vk = (BYTE)ch;
            }
        }
        else if (ParseFunctionKeyToken(lastToken, &vk)) {
        }
        else if (ParseHexVirtualKeyToken(lastToken, &vk)) {
        }
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
WORD NormalizeHotkeyValue(WORD hotkey) {
    if (hotkey == 0) {
        return 0;
    }
    return MAKEWORD(LOBYTE(hotkey), HIBYTE(hotkey) & HOTKEY_SUPPORTED_MODIFIERS);
}
BOOL IsHotkeyValueAllowed(WORD hotkey) {
    hotkey = NormalizeHotkeyValue(hotkey);
    if (hotkey == 0) {
        return TRUE;
    }
    if (LOBYTE(hotkey) == 0) {
        return FALSE;
    }
    if (IsModifierVirtualKey(LOBYTE(hotkey))) {
        return FALSE;
    }
    return !(LOBYTE(hotkey) == VK_IME_SHIFT &&
             HIBYTE(hotkey) == HOTKEYF_SHIFT);
}
