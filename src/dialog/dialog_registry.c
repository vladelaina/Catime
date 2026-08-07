#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"

static HWND g_dialogInstances[DIALOG_INSTANCE_COUNT] = {0};
static HWND g_dialogOwners[DIALOG_INSTANCE_COUNT] = {0};

static BOOL Dialog_IsModalInstance(DialogInstanceType type) {
    switch (type) {
        case DIALOG_INSTANCE_MESSAGE_INFO:
        case DIALOG_INSTANCE_MESSAGE_WARNING:
        case DIALOG_INSTANCE_MESSAGE_ERROR:
        case DIALOG_INSTANCE_INPUT_BOX:
        case DIALOG_INSTANCE_COLOR_PICKER:
            return TRUE;
        default:
            return FALSE;
    }
}

static void Dialog_DetachModelessOwner(DialogInstanceType type, HWND hwnd) {
    if (!hwnd || Dialog_IsModalInstance(type)) return;

    HWND owner = GetWindow(hwnd, GW_OWNER);
    if (!owner || !IsWindow(owner)) return;
    g_dialogOwners[type] = owner;

    /* Keep modeless dialogs independent in z-order while retaining the owner
     * handle in the registry for callbacks that need to notify the main window. */
    if (!SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, 0)) {
        g_dialogOwners[type] = NULL;
        return;
    }
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(style & WS_EX_TOOLWINDOW)) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

void Dialog_RegisterInstance(DialogInstanceType type, HWND hwnd) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = hwnd;
    g_dialogOwners[type] = NULL;
}

void Dialog_InitializeInstance(DialogInstanceType type, HWND hwnd) {
    Dialog_RegisterInstance(type, hwnd);
    if (hwnd && IsWindow(hwnd)) {
        DialogModern_Attach(hwnd, (int)type);
        Dialog_DetachModelessOwner(type, hwnd);
    }
}

void Dialog_UnregisterInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return;
    g_dialogInstances[type] = NULL;
    g_dialogOwners[type] = NULL;
}

void Dialog_UnregisterInstanceForWindow(DialogInstanceType type, HWND hwnd) {
    if (type >= 0 && type < DIALOG_INSTANCE_COUNT && g_dialogInstances[type] == hwnd) {
        g_dialogInstances[type] = NULL;
        g_dialogOwners[type] = NULL;
    }
}

HWND Dialog_GetInstance(DialogInstanceType type) {
    if (type < 0 || type >= DIALOG_INSTANCE_COUNT) return NULL;
    HWND hwnd = g_dialogInstances[type];
    if (hwnd && !IsWindow(hwnd)) {
        g_dialogInstances[type] = NULL;
        g_dialogOwners[type] = NULL;
        return NULL;
    }
    return hwnd;
}

BOOL Dialog_IsOpen(DialogInstanceType type) {
    return Dialog_GetInstance(type) != NULL;
}

HWND Dialog_GetOwnerWindow(HWND hwndDlg) {
    if (!hwndDlg) return NULL;
    for (int type = 0; type < DIALOG_INSTANCE_COUNT; type++) {
        if (g_dialogInstances[type] == hwndDlg) {
            HWND owner = g_dialogOwners[type];
            return owner && IsWindow(owner) ? owner : NULL;
        }
    }
    return GetWindow(hwndDlg, GW_OWNER);
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
