#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"

static HWND g_dialogInstances[DIALOG_INSTANCE_COUNT] = {0};

void Dialog_RegisterInstance(DialogInstanceType type, HWND hwnd) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = hwnd;
}

void Dialog_InitializeInstance(DialogInstanceType type, HWND hwnd) {
    Dialog_RegisterInstance(type, hwnd);
    if (hwnd && IsWindow(hwnd)) {
        DialogModern_Attach(hwnd, (int)type);
        Dialog_ApplyTopmost(hwnd);
    }
}

void Dialog_UnregisterInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = NULL;
}

void Dialog_UnregisterInstanceForWindow(DialogInstanceType type, HWND hwnd) {
    if (type >= 0 && type < DIALOG_INSTANCE_COUNT && g_dialogInstances[type] == hwnd)
        g_dialogInstances[type] = NULL;
}

HWND Dialog_GetInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return NULL;
    HWND hwnd = g_dialogInstances[type];
    if (hwnd && !IsWindow(hwnd)) {
        g_dialogInstances[type] = NULL;
        return NULL;
    }
    return hwnd;
}

BOOL Dialog_IsOpen(DialogInstanceType type) {
    return Dialog_GetInstance(type) != NULL;
}

void Dialog_RefreshOpenThemes(void) {
    HWND dialogs[DIALOG_INSTANCE_COUNT] = {0};
    size_t count = 0;
    for (int type = 0; type < DIALOG_INSTANCE_COUNT; type++) {
        HWND hwnd = Dialog_GetInstance((DialogInstanceType)type);
        if (!hwnd) continue;
        BOOL duplicate = FALSE;
        for (size_t i = 0; i < count; i++) {
            if (dialogs[i] == hwnd) { duplicate = TRUE; break; }
        }
        if (!duplicate) dialogs[count++] = hwnd;
    }
    for (size_t i = 0; i < count; i++)
        if (IsWindow(dialogs[i])) SendMessageW(dialogs[i], WM_THEMECHANGED, 0, 0);
}

static BOOL Dialog_IsOpenComboMessage(HWND hwndDlg, HWND hwndMessage) {
    HWND current = hwndMessage;
    while (current && current != hwndDlg) {
        wchar_t className[32] = {0};
        if (GetClassNameW(current, className, _countof(className)) > 0 &&
            (lstrcmpiW(className, L"ComboBox") == 0 ||
             lstrcmpiW(className, L"ComboBoxEx32") == 0) &&
            SendMessageW(current, CB_GETDROPPEDSTATE, 0, 0)) return TRUE;
        current = GetParent(current);
    }
    return FALSE;
}

static BOOL Dialog_IsNativeDialogWindow(HWND hwnd) {
    wchar_t className[32] = {0};
    return hwnd && GetClassNameW(hwnd, className, _countof(className)) > 0 &&
           lstrcmpW(className, L"#32770") == 0;
}

BOOL Dialog_ProcessModelessMessage(MSG* msg) {
    if (!msg || !msg->hwnd) return FALSE;
    for (int type = 0; type < DIALOG_INSTANCE_COUNT; type++) {
        HWND hwndDlg = Dialog_GetInstance((DialogInstanceType)type);
        if (!hwndDlg || (msg->hwnd != hwndDlg && !IsChild(hwndDlg, msg->hwnd)))
            continue;
        if (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE &&
            !Dialog_IsOpenComboMessage(hwndDlg, msg->hwnd)) {
            SendMessageW(hwndDlg, WM_CLOSE, 0, 0);
            return TRUE;
        }
        if (Dialog_IsNativeDialogWindow(hwndDlg) && IsDialogMessageW(hwndDlg, msg))
            return TRUE;
    }
    return FALSE;
}
