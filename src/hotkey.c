/**
 * @file hotkey.c
 * @brief Data-driven hotkey management system
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>

/** Enable visual styles */
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "hotkey.h"
#include "language.h"
#include "config.h"
#include "log.h"
#include "window_procedure/window_procedure.h"
#include "dialog/dialog_procedure.h"
#include "../resource/resource.h"

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

#define VK_IME_SHIFT 0xE5

typedef struct {
    int editCtrlId;
    int labelCtrlId;
    const wchar_t* labelCN;
    const wchar_t* labelEN;
} HotkeyMetadata;

/** Dialog-local hotkey storage (avoids 12 global variables) */
static WORD g_dialogHotkeys[HOTKEY_COUNT] = {0};

static WNDPROC g_OldHotkeyDlgProc = NULL;

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
};

static inline BOOL IsHotkeyEditControl(DWORD ctrlId) {
    for (int i = 0; i < HOTKEY_COUNT; i++) {
        if (g_hotkeyMetadata[i].editCtrlId == (int)ctrlId) {
            return TRUE;
        }
    }
    return FALSE;
}

static inline BOOL IsModifierKey(BYTE vk) {
    return (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || 
            vk == VK_LWIN || vk == VK_RWIN);
}

/** IME Shift+Shift conflicts with system input switching */
static inline BOOL IsInvalidIMEHotkey(WORD hotkey) {
    return (LOBYTE(hotkey) == VK_IME_SHIFT && HIBYTE(hotkey) == HOTKEYF_SHIFT);
}

/** Prevent single-key hotkeys that would block normal typing */
static BOOL IsRestrictedSingleKey(WORD hotkey) {
    if (hotkey == 0) return FALSE;

    BYTE vk = LOBYTE(hotkey);
    BYTE modifiers = HIBYTE(hotkey);

    if (modifiers != 0) return FALSE;

    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') ||
        (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)) {
        return TRUE;
    }

    switch (vk) {
        case VK_OEM_1: case VK_OEM_PLUS: case VK_OEM_COMMA:
        case VK_OEM_MINUS: case VK_OEM_PERIOD: case VK_OEM_2:
        case VK_OEM_3: case VK_OEM_4: case VK_OEM_5:
        case VK_OEM_6: case VK_OEM_7: case VK_SPACE:
        case VK_RETURN: case VK_ESCAPE: case VK_TAB:
            return TRUE;
    }

    return FALSE;
}

/** @return TRUE if hotkey was cleared */
static BOOL ValidateAndSanitizeHotkey(WORD* hotkey) {
    if (!hotkey) return FALSE;

    if (IsInvalidIMEHotkey(*hotkey) || IsRestrictedSingleKey(*hotkey)) {
        *hotkey = 0;
        return TRUE;
    }

    return FALSE;
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
                     &g_dialogHotkeys[10], &g_dialogHotkeys[11], &g_dialogHotkeys[12]);
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

static void SaveHotkeyConfiguration(void) {
    WriteConfigHotkeys(g_dialogHotkeys[0], g_dialogHotkeys[1], g_dialogHotkeys[3],
                      g_dialogHotkeys[4], g_dialogHotkeys[5], g_dialogHotkeys[6],
                      g_dialogHotkeys[7], g_dialogHotkeys[8], g_dialogHotkeys[9],
                      g_dialogHotkeys[10], g_dialogHotkeys[11], g_dialogHotkeys[12]);

    char customCountdownStr[64] = {0};
    HotkeyToString(g_dialogHotkeys[2], customCountdownStr, sizeof(customCountdownStr));
    WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr);
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

    return CallWindowProc(g_OldHotkeyDlgProc, hwnd, msg, wParam, lParam);
}

void ShowHotkeySettingsDialog(HWND hwndParent) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!hInstance) {
        LOG_ERROR("Failed to get module handle for hotkey dialog");
        return;
    }
    
    INT_PTR result = DialogBoxW(hInstance,
                                MAKEINTRESOURCE(CLOCK_IDD_HOTKEY_DIALOG),
                                hwndParent,
                                HotkeySettingsDlgProc);
    
    if (result == -1) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to create hotkey dialog. Error code: %lu. Please check log file for details.", error);
    }
}

INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            MoveDialogToPrimaryScreen(hwndDlg);

            InitializeDialogLabels(hwndDlg);

            hBackgroundBrush = CreateSolidBrush(DIALOG_BG_COLOR);
            hButtonBrush = CreateSolidBrush(BUTTON_BG_COLOR);

            LoadHotkeyConfiguration();
            SetHotkeyControlValues(hwndDlg);

            UnregisterGlobalHotkeys(GetParent(hwndDlg));
            SetupHotkeyControlSubclassing(hwndDlg);

            g_OldHotkeyDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, 
                                                           (LONG_PTR)HotkeyDialogSubclassProc);

            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, DIALOG_BG_COLOR);
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(DIALOG_BG_COLOR);
            }
            return (INT_PTR)hBackgroundBrush;
        }
        
        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, BUTTON_BG_COLOR);
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(BUTTON_BG_COLOR);
            }
            return (INT_PTR)hButtonBrush;
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
                    SaveHotkeyConfiguration();

                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        
        case WM_DESTROY:
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }

            if (g_OldHotkeyDlgProc) {
                SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)g_OldHotkeyDlgProc);
                g_OldHotkeyDlgProc = NULL;
            }

            RemoveHotkeyControlSubclassing(hwndDlg);
            break;
    }

    return FALSE;
}

/** Allows Enter key to submit dialog from hotkey control */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 
                              (LPARAM)GetDlgItem(hwndDlg, IDOK));
                    return 0;
                }
            }
            break;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
