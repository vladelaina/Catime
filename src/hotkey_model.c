/**
 * @file hotkey_model.c
 * @brief Dialog-local hotkey metadata, validation, and persistence mapping
 */
#include "hotkey_internal.h"

#include "config.h"

WORD g_dialogHotkeys[HOTKEY_COUNT] = {0};

const HotkeyMetadata g_hotkeyMetadata[HOTKEY_COUNT] = {
    {IDC_HOTKEY_EDIT1, IDC_HOTKEY_LABEL1, NULL, L"Show Current Time"},
    {IDC_HOTKEY_EDIT2, IDC_HOTKEY_LABEL2, NULL, L"Count Up"},
    {IDC_HOTKEY_EDIT12, IDC_HOTKEY_LABEL12, NULL, L"Countdown"},
    {IDC_HOTKEY_EDIT3, IDC_HOTKEY_LABEL3, NULL, L"Default Countdown:"},
    {IDC_HOTKEY_EDIT9, IDC_HOTKEY_LABEL9, NULL, L"Quick Countdown 1:"},
    {IDC_HOTKEY_EDIT10, IDC_HOTKEY_LABEL10, NULL, L"Quick Countdown 2:"},
    {IDC_HOTKEY_EDIT11, IDC_HOTKEY_LABEL11, NULL, L"Quick Countdown 3:"},
    {IDC_HOTKEY_EDIT4, IDC_HOTKEY_LABEL4, NULL, L"Start Pomodoro:"},
    {IDC_HOTKEY_EDIT5, IDC_HOTKEY_LABEL5, NULL, L"Hide/Show Window:"},
    {IDC_HOTKEY_EDIT6, IDC_HOTKEY_LABEL6, NULL, L"Enter Edit Mode:"},
    {IDC_HOTKEY_EDIT7, IDC_HOTKEY_LABEL7, NULL, L"Pause/Resume Timer:"},
    {IDC_HOTKEY_EDIT8, IDC_HOTKEY_LABEL8, NULL, L"Restart Timer:"},
    {IDC_HOTKEY_EDIT13, IDC_HOTKEY_LABEL13, NULL, L"Show Milliseconds"},
    {IDC_HOTKEY_EDIT14, IDC_HOTKEY_LABEL14, NULL, L"Always on Top"},
};

BOOL Hotkey_IsEditControl(DWORD controlId) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (g_hotkeyMetadata[i].editCtrlId == (int)controlId) return TRUE;
    }
    return FALSE;
}

BOOL Hotkey_IsModifierKey(BYTE virtualKey) {
    return virtualKey == VK_SHIFT || virtualKey == VK_CONTROL ||
           virtualKey == VK_MENU || virtualKey == VK_LSHIFT ||
           virtualKey == VK_RSHIFT || virtualKey == VK_LCONTROL ||
           virtualKey == VK_RCONTROL || virtualKey == VK_LMENU ||
           virtualKey == VK_RMENU || virtualKey == VK_LWIN ||
           virtualKey == VK_RWIN;
}

BOOL Hotkey_ValidateAndSanitize(WORD* hotkey) {
    WORD normalized;
    BOOL changed;

    if (!hotkey) return FALSE;
    normalized = NormalizeHotkeyValue(*hotkey);
    changed = *hotkey != normalized;
    *hotkey = normalized;
    if (!IsHotkeyValueAllowed(*hotkey)) {
        *hotkey = 0;
        return TRUE;
    }
    return changed;
}

BOOL Hotkey_IsExistingEvent(WORD keyCombination) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        if (g_dialogHotkeys[i] != 0 &&
            g_dialogHotkeys[i] == keyCombination) {
            return TRUE;
        }
    }
    return FALSE;
}

void Hotkey_ClearDuplicates(HWND dialog, int currentControlId,
                            WORD newHotkey) {
    if (newHotkey == 0) return;
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        int controlId = g_hotkeyMetadata[i].editCtrlId;
        WORD otherHotkey;
        if (controlId == currentControlId) continue;
        otherHotkey = (WORD)SendDlgItemMessage(
            dialog, controlId, HKM_GETHOTKEY, 0, 0);
        if (otherHotkey == newHotkey) {
            SendDlgItemMessage(dialog, controlId, HKM_SETHOTKEY, 0, 0);
        }
    }
}

void Hotkey_LoadConfiguration(void) {
    ReadConfigHotkeys(
        &g_dialogHotkeys[0], &g_dialogHotkeys[1], &g_dialogHotkeys[3],
        &g_dialogHotkeys[4], &g_dialogHotkeys[5], &g_dialogHotkeys[6],
        &g_dialogHotkeys[7], &g_dialogHotkeys[8], &g_dialogHotkeys[9],
        &g_dialogHotkeys[10], &g_dialogHotkeys[11], &g_dialogHotkeys[12],
        &g_dialogHotkeys[13]);
    ReadCustomCountdownHotkey(&g_dialogHotkeys[2]);
}

void Hotkey_SetControlValues(HWND dialog) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        SendDlgItemMessage(dialog, g_hotkeyMetadata[i].editCtrlId,
                           HKM_SETHOTKEY, g_dialogHotkeys[i], 0);
    }
}

void Hotkey_GetControlValues(HWND dialog) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        g_dialogHotkeys[i] = (WORD)SendDlgItemMessage(
            dialog, g_hotkeyMetadata[i].editCtrlId, HKM_GETHOTKEY, 0, 0);
    }
}

void Hotkey_ValidateAll(void) {
    for (int i = 0; i < HOTKEY_COUNT; ++i) {
        Hotkey_ValidateAndSanitize(&g_dialogHotkeys[i]);
    }
}

BOOL Hotkey_SaveConfiguration(void) {
    return WriteConfigHotkeys(
        g_dialogHotkeys[0], g_dialogHotkeys[1], g_dialogHotkeys[3],
        g_dialogHotkeys[2], g_dialogHotkeys[4], g_dialogHotkeys[5],
        g_dialogHotkeys[6], g_dialogHotkeys[7], g_dialogHotkeys[8],
        g_dialogHotkeys[9], g_dialogHotkeys[10], g_dialogHotkeys[11],
        g_dialogHotkeys[12], g_dialogHotkeys[13]);
}
