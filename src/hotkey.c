/**
 * @file hotkey.c
 * @brief Data-driven hotkey management system (modeless dialog version)
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>
#include <stdlib.h>

/** Enable visual styles */
#ifdef _MSC_VER
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include "hotkey.h"
#include "language.h"
#include "config.h"
#include "log.h"
#include "window_procedure/window_procedure.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_common.h"
#include "../resource/resource.h"

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"

typedef struct {
    int editCtrlId;
    int labelCtrlId;
    const wchar_t* labelCN;
    const wchar_t* labelEN;
} HotkeyMetadata;

typedef struct {
    HWND hwndParent;
    HBRUSH hBackgroundBrush;
    HBRUSH hButtonBrush;
    WNDPROC origDlgProc;
    BOOL hotkeysSuspended;
    BOOL reregisterPosted;
} HotkeyDialogState;

/** Dialog-local hotkey storage (avoids per-action global variables) */
static WORD g_dialogHotkeys[HOTKEY_COUNT] = {0};

static const HotkeyMetadata g_hotkeyMetadata[HOTKEY_COUNT] = {
    {IDC_HOTKEY_EDIT1,  IDC_HOTKEY_LABEL1,  NULL, L"Show Current Time:"},
    {IDC_HOTKEY_EDIT2,  IDC_HOTKEY_LABEL2,  NULL, L"Count Up:"},
    {IDC_HOTKEY_EDIT12, IDC_HOTKEY_LABEL12, NULL, L"Countdown:"},
    {IDC_HOTKEY_EDIT3,  IDC_HOTKEY_LABEL3,  NULL, L"Default Countdown:"},
    {IDC_HOTKEY_EDIT9,  IDC_HOTKEY_LABEL9,  NULL, L"Quick Countdown 1:"},
    {IDC_HOTKEY_EDIT10, IDC_HOTKEY_LABEL10, NULL, L"Quick Countdown 2:"},
    {IDC_HOTKEY_EDIT11, IDC_HOTKEY_LABEL11, NULL, L"Quick Countdown 3:"},
    {IDC_HOTKEY_EDIT4,  IDC_HOTKEY_LABEL4,  NULL, L"Start Pomodoro:"},
    {IDC_HOTKEY_EDIT5,  IDC_HOTKEY_LABEL5,  NULL, L"Hide/Show Window:"},
    {IDC_HOTKEY_EDIT6,  IDC_HOTKEY_LABEL6,  NULL, L"Enter Edit Mode:"},
    {IDC_HOTKEY_EDIT7,  IDC_HOTKEY_LABEL7,  NULL, L"Pause/Resume Timer:"},
    {IDC_HOTKEY_EDIT8,  IDC_HOTKEY_LABEL8,  NULL, L"Restart Timer:"},
    {IDC_HOTKEY_EDIT13, IDC_HOTKEY_LABEL13, NULL, L"Show Milliseconds:"},
    {IDC_HOTKEY_EDIT14, IDC_HOTKEY_LABEL14, NULL, L"Always on Top:"},
};

static inline BOOL IsHotkeyEditControl(DWORD ctrlId) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        if (g_hotkeyMetadata[i].editCtrlId == (int)ctrlId) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL IsValidHotkeyParentWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static HWND GetHotkeyDialogParent(HWND hwndDlg) {
    HWND hwndParent = hwndDlg ? GetParent(hwndDlg) : NULL;
    return IsValidHotkeyParentWindow(hwndParent) ? hwndParent : NULL;
}

static HotkeyDialogState* GetHotkeyDialogState(HWND hwndDlg) {
    return (HotkeyDialogState*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
}

static void SetHotkeyDialogState(HWND hwndDlg, HotkeyDialogState* state) {
    SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
}

static HotkeyDialogState* CreateHotkeyDialogState(HWND hwndParent) {
    HotkeyDialogState* state = (HotkeyDialogState*)calloc(1, sizeof(*state));
    if (!state) {
        return NULL;
    }

    state->hwndParent = hwndParent;
    state->hBackgroundBrush = CreateSolidBrush(DIALOG_BG_COLOR);
    state->hButtonBrush = CreateSolidBrush(BUTTON_BG_COLOR);
    return state;
}

static void DestroyHotkeyDialogState(HWND hwndDlg, HotkeyDialogState* state) {
    if (!state) {
        return;
    }

    if (state->hBackgroundBrush) {
        DeleteObject(state->hBackgroundBrush);
        state->hBackgroundBrush = NULL;
    }
    if (state->hButtonBrush) {
        DeleteObject(state->hButtonBrush);
        state->hButtonBrush = NULL;
    }

    SetHotkeyDialogState(hwndDlg, NULL);
    free(state);
}

static BOOL EnsureHotkeyBrush(HBRUSH* brush, COLORREF color) {
    if (!brush) {
        return FALSE;
    }
    if (!*brush) {
        *brush = CreateSolidBrush(color);
    }
    return *brush != NULL;
}

static void PostHotkeyReregister(HWND hwndDlg) {
    HotkeyDialogState* state = GetHotkeyDialogState(hwndDlg);
    if (!state || !state->hotkeysSuspended || state->reregisterPosted) {
        return;
    }

    if (!IsValidHotkeyParentWindow(state->hwndParent)) {
        return;
    }

    if (PostMessage(state->hwndParent, WM_APP + 1, 0, 0)) {
        state->reregisterPosted = TRUE;
    } else {
        LOG_WARNING("Failed to post hotkey re-register message: %lu", GetLastError());
        RegisterGlobalHotkeys(state->hwndParent);
        state->reregisterPosted = TRUE;
    }
}

static inline BOOL IsModifierKey(BYTE vk) {
    return (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
            vk == VK_LSHIFT || vk == VK_RSHIFT ||
            vk == VK_LCONTROL || vk == VK_RCONTROL ||
            vk == VK_LMENU || vk == VK_RMENU ||
            vk == VK_LWIN || vk == VK_RWIN);
}

/** @return TRUE if hotkey was cleared */
static BOOL ValidateAndSanitizeHotkey(WORD* hotkey) {
    if (!hotkey) return FALSE;

    WORD normalized = NormalizeHotkeyValue(*hotkey);
    BOOL changed = (*hotkey != normalized);
    *hotkey = normalized;

    if (!IsHotkeyValueAllowed(*hotkey)) {
        *hotkey = 0;
        return TRUE;
    }

    return changed;
}

static void InitializeDialogLabels(HWND hwndDlg) {
    SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Hotkey Settings"));

    for (int i = 0; i < HOTKEY_COUNT; i++) {
        SetDlgItemTextW(hwndDlg, g_hotkeyMetadata[i].labelCtrlId,
                       GetLocalizedString(g_hotkeyMetadata[i].labelCN, 
                                        g_hotkeyMetadata[i].labelEN));
    }

    SetDlgItemTextW(hwndDlg, IDC_HOTKEY_NOTE,
                   GetLocalizedString(NULL, L"* Hotkeys will work globally"));
    SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
    SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(NULL, L"Cancel"));
}

static void LoadHotkeyConfiguration(void) {
    ReadConfigHotkeys(&g_dialogHotkeys[0], &g_dialogHotkeys[1], &g_dialogHotkeys[3],
                     &g_dialogHotkeys[4], &g_dialogHotkeys[5], &g_dialogHotkeys[6],
                     &g_dialogHotkeys[7], &g_dialogHotkeys[8], &g_dialogHotkeys[9],
                     &g_dialogHotkeys[10], &g_dialogHotkeys[11], &g_dialogHotkeys[12],
                     &g_dialogHotkeys[13]);
    ReadCustomCountdownHotkey(&g_dialogHotkeys[2]);
}

static void SetHotkeyControlValues(HWND hwndDlg) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        SendDlgItemMessage(hwndDlg, g_hotkeyMetadata[i].editCtrlId, 
                          HKM_SETHOTKEY, g_dialogHotkeys[i], 0);
    }
}

static void GetHotkeyControlValues(HWND hwndDlg) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        g_dialogHotkeys[i] = (WORD)SendDlgItemMessage(hwndDlg, 
                                                      g_hotkeyMetadata[i].editCtrlId,
                                                      HKM_GETHOTKEY, 0, 0);
    }
}

static void ValidateAllHotkeys(void) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        ValidateAndSanitizeHotkey(&g_dialogHotkeys[i]);
    }
}

static BOOL SaveHotkeyConfiguration(void) {
    return WriteConfigHotkeys(g_dialogHotkeys[0], g_dialogHotkeys[1], g_dialogHotkeys[3],
                             g_dialogHotkeys[2],
                             g_dialogHotkeys[4], g_dialogHotkeys[5], g_dialogHotkeys[6],
                             g_dialogHotkeys[7], g_dialogHotkeys[8], g_dialogHotkeys[9],
                             g_dialogHotkeys[10], g_dialogHotkeys[11], g_dialogHotkeys[12],
                             g_dialogHotkeys[13]);
}

static void SetupHotkeyControlSubclassing(HWND hwndDlg) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        HWND hCtrl = GetDlgItem(hwndDlg, g_hotkeyMetadata[i].editCtrlId);
        if (hCtrl) {
            SetWindowSubclass(hCtrl, HotkeyControlSubclassProc, 
                            g_hotkeyMetadata[i].editCtrlId, 0);
        }
    }
}

static void RemoveHotkeyControlSubclassing(HWND hwndDlg) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        HWND hCtrl = GetDlgItem(hwndDlg, g_hotkeyMetadata[i].editCtrlId);
        if (hCtrl) {
            RemoveWindowSubclass(hCtrl, HotkeyControlSubclassProc, 
                               g_hotkeyMetadata[i].editCtrlId);
        }
    }
}

/** Prevents triggering hotkey while configuring it */
static BOOL IsExistingHotkeyEvent(WORD keyCombination) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        if (g_dialogHotkeys[i] != 0 && g_dialogHotkeys[i] == keyCombination) {
            return TRUE;
        }
    }
    return FALSE;
}

/** Enforce uniqueness: clear conflicting hotkeys from other controls */
static void ClearDuplicateHotkeys(HWND hwndDlg, int currentCtrlId, WORD newHotkey) {
    if (newHotkey == 0) return;

    for (int i = 0; i < HOTKEY_COUNT; i++) {
        int ctrlId = g_hotkeyMetadata[i].editCtrlId;
        if (ctrlId == currentCtrlId) continue;

        WORD otherHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);
        if (otherHotkey != 0 && otherHotkey == newHotkey) {
            SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
        }
    }
}

/** @param msg Needed for Alt detection (WM_SYSKEYDOWN) */
static BYTE GetCurrentModifiers(UINT msg) {
    BYTE modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= HOTKEYF_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= HOTKEYF_CONTROL;
    if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || (GetKeyState(VK_MENU) & 0x8000)) {
        modifiers |= HOTKEYF_ALT;
    }
    return modifiers;
}

/** Intercepts hotkey events to prevent accidental trigger during configuration */
LRESULT CALLBACK HotkeyDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        BYTE vk = (BYTE)wParam;
        
        if (!IsModifierKey(vk)) {
            BYTE currentModifiers = GetCurrentModifiers(msg);
            WORD currentEventKeyCombination = MAKEWORD(vk, currentModifiers);

            if (IsExistingHotkeyEvent(currentEventKeyCombination)) {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    if (!IsHotkeyEditControl(ctrlId)) {
                        return 0;
                    }
                } else {
                    return 0;
                }
            }
        }
    }

    switch (msg) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            HWND hwndFocus = GetFocus();
            if (hwndFocus) {
                DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                if (IsHotkeyEditControl(ctrlId)) {
                    break;
                }
            }
            return 0;
        }

        case WM_KEYDOWN:
        case WM_KEYUP: {
            BYTE vk_code = (BYTE)wParam;
            if (IsModifierKey(vk_code)) {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    if (!IsHotkeyEditControl(ctrlId)) {
                        return 0;
                    }
                } else {
                    return 0;
                }
            }
            break;
        }

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;
    }

    HotkeyDialogState* state = GetHotkeyDialogState(hwnd);
    if (!state || !state->origDlgProc) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return CallWindowProc(state->origDlgProc, hwnd, msg, wParam, lParam);
}

void ShowHotkeySettingsDialog(HWND hwndParent) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_HOTKEY)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_HOTKEY);
        SetForegroundWindow(existing);
        return;
    }

    if (!IsValidHotkeyParentWindow(hwndParent)) {
        return;
    }

    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCEW(CLOCK_IDD_HOTKEY_DIALOG),
        hwndParent,
        HotkeySettingsDlgProc
    );

    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    } else {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to create hotkey dialog. Error code: %lu", error);
    }
}

INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    HotkeyDialogState* state = GetHotkeyDialogState(hwndDlg);

    switch (msg) {
        case WM_INITDIALOG: {
            HWND hwndParent = GetHotkeyDialogParent(hwndDlg);
            state = CreateHotkeyDialogState(hwndParent);
            if (!state) {
                LOG_ERROR("Failed to allocate hotkey dialog state");
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            SetHotkeyDialogState(hwndDlg, state);

            Dialog_RegisterInstance(DIALOG_INSTANCE_HOTKEY, hwndDlg);
            MoveDialogToPrimaryScreen(hwndDlg);

            InitializeDialogLabels(hwndDlg);

            LoadHotkeyConfiguration();
            SetHotkeyControlValues(hwndDlg);

            /* Unregister hotkeys while dialog is open */
            if (hwndParent) {
                UnregisterGlobalHotkeys(hwndParent);
                state->hotkeysSuspended = TRUE;
            }
            SetupHotkeyControlSubclassing(hwndDlg);

            SetLastError(0);
            WNDPROC origDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC,
                                                            (LONG_PTR)HotkeyDialogSubclassProc);
            if (origDlgProc) {
                state->origDlgProc = origDlgProc;
            } else if (GetLastError() != 0) {
                LOG_WARNING("Failed to subclass hotkey dialog: %lu", GetLastError());
            }

            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, DIALOG_BG_COLOR);
            if (state && EnsureHotkeyBrush(&state->hBackgroundBrush, DIALOG_BG_COLOR)) {
                return (INT_PTR)state->hBackgroundBrush;
            }
            break;
        }
        
        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, BUTTON_BG_COLOR);
            if (state && EnsureHotkeyBrush(&state->hButtonBrush, BUTTON_BG_COLOR)) {
                return (INT_PTR)state->hButtonBrush;
            }
            break;
        }
        
        case WM_LBUTTONDOWN: {
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            HWND hwndHit = ChildWindowFromPoint(hwndDlg, pt);

            if (hwndHit != NULL && hwndHit != hwndDlg) {
                int ctrlId = GetDlgCtrlID(hwndHit);
                if (!IsHotkeyEditControl(ctrlId)) {
                    SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                }
            } else if (hwndHit == hwndDlg) {
                SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            WORD ctrlId = LOWORD(wParam);
            WORD notifyCode = HIWORD(wParam);

            if (notifyCode == EN_CHANGE && IsHotkeyEditControl(ctrlId)) {
                WORD newHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);

                if (ValidateAndSanitizeHotkey(&newHotkey)) {
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, newHotkey, 0);
                    return TRUE;
                }

                ClearDuplicateHotkeys(hwndDlg, ctrlId, newHotkey);
                return TRUE;
            }
            
            switch (LOWORD(wParam)) {
                case IDOK: {
                    GetHotkeyControlValues(hwndDlg);
                    ValidateAllHotkeys();
                    if (!SaveHotkeyConfiguration()) {
                        Dialog_ShowErrorAndRefocus(hwndDlg, IDC_HOTKEY_NOTE);
                        return TRUE;
                    }

                    /* Re-register hotkeys */
                    PostHotkeyReregister(hwndDlg);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }

                case IDCANCEL: {
                    /* Re-register hotkeys */
                    PostHotkeyReregister(hwndDlg);
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostHotkeyReregister(hwndDlg);
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            PostHotkeyReregister(hwndDlg);
            DestroyWindow(hwndDlg);
            return TRUE;
        
        case WM_DESTROY:
            PostHotkeyReregister(hwndDlg);

            if (state && state->origDlgProc) {
                SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)state->origDlgProc);
                state->origDlgProc = NULL;
            }

            RemoveHotkeyControlSubclassing(hwndDlg);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_HOTKEY, hwndDlg);
            DestroyHotkeyDialogState(hwndDlg, state);
            break;
    }

    return FALSE;
}

LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
    UNREFERENCED_PARAMETER(uIdSubclass);
    UNREFERENCED_PARAMETER(dwRefData);

    switch (uMsg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
