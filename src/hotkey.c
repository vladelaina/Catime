/**
 * @file hotkey.c
 * @brief Hotkey management implementation
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#include "../include/hotkey.h"
#include "../include/language.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../resource/resource.h"

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

static WORD g_dlgShowTimeHotkey = 0;
static WORD g_dlgCountUpHotkey = 0;
static WORD g_dlgCountdownHotkey = 0;
static WORD g_dlgCustomCountdownHotkey = 0;
static WORD g_dlgQuickCountdown1Hotkey = 0;
static WORD g_dlgQuickCountdown2Hotkey = 0;
static WORD g_dlgQuickCountdown3Hotkey = 0;
static WORD g_dlgPomodoroHotkey = 0;
static WORD g_dlgToggleVisibilityHotkey = 0;
static WORD g_dlgEditModeHotkey = 0;
static WORD g_dlgPauseResumeHotkey = 0;
static WORD g_dlgRestartTimerHotkey = 0;

static WNDPROC g_OldHotkeyDlgProc = NULL;

/**
 * @brief Dialog subclassing procedure
 */
LRESULT CALLBACK HotkeyDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        BYTE vk = (BYTE)wParam;
        if (!(vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)) {
            BYTE currentModifiers = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= HOTKEYF_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= HOTKEYF_CONTROL;
            if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || (GetKeyState(VK_MENU) & 0x8000)) {
                currentModifiers |= HOTKEYF_ALT;
            }

            WORD currentEventKeyCombination = MAKEWORD(vk, currentModifiers);

            const WORD originalHotkeys[] = {
                g_dlgShowTimeHotkey, g_dlgCountUpHotkey, g_dlgCountdownHotkey,
                g_dlgQuickCountdown1Hotkey, g_dlgQuickCountdown2Hotkey, g_dlgQuickCountdown3Hotkey,
                g_dlgPomodoroHotkey, g_dlgToggleVisibilityHotkey, g_dlgEditModeHotkey,
                g_dlgPauseResumeHotkey, g_dlgRestartTimerHotkey
            };
            BOOL isAnOriginalHotkeyEvent = FALSE;
            for (size_t i = 0; i < sizeof(originalHotkeys) / sizeof(originalHotkeys[0]); ++i) {
                if (originalHotkeys[i] != 0 && originalHotkeys[i] == currentEventKeyCombination) {
                    isAnOriginalHotkeyEvent = TRUE;
                    break;
                }
            }

            if (isAnOriginalHotkeyEvent) {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    BOOL isHotkeyEditControl = FALSE;
                    for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                        if (ctrlId == i) { isHotkeyEditControl = TRUE; break; }
                    }
                    if (!isHotkeyEditControl) {
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
        case WM_SYSKEYUP:
            {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    BOOL isHotkeyEditControl = FALSE;
                    for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                        if (ctrlId == i) { isHotkeyEditControl = TRUE; break; }
                    }
                    if (isHotkeyEditControl) {
                        break;
                    }
                }
                return 0;
            }

        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                BYTE vk_code = (BYTE)wParam;
                if (vk_code == VK_SHIFT || vk_code == VK_CONTROL || vk_code == VK_LWIN || vk_code == VK_RWIN) {
                    HWND hwndFocus = GetFocus();
                    if (hwndFocus) {
                        DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                        BOOL isHotkeyEditControl = FALSE;
                        for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                            if (ctrlId == i) { isHotkeyEditControl = TRUE; break; }
                        }
                        if (!isHotkeyEditControl) {
                            return 0;
                        }
                    } else {
                        return 0;
                    }
                }
            }
            break;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;
    }

    return CallWindowProc(g_OldHotkeyDlgProc, hwnd, msg, wParam, lParam);
}

/**
 * @brief Show hotkey settings dialog
 */
void ShowHotkeySettingsDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL),
             MAKEINTRESOURCE(CLOCK_IDD_HOTKEY_DIALOG),
             hwndParent,
             HotkeySettingsDlgProc);
}

/**
 * @brief Check if a hotkey is a single key
 */
BOOL IsSingleKey(WORD hotkey) {
    BYTE modifiers = HIBYTE(hotkey);

    return modifiers == 0;
}

/**
 * @brief Check if a hotkey is a standalone letter, number, or symbol
 */
BOOL IsRestrictedSingleKey(WORD hotkey) {
    if (hotkey == 0) {
        return FALSE;
    }

    BYTE vk = LOBYTE(hotkey);
    BYTE modifiers = HIBYTE(hotkey);

    if (modifiers != 0) {
        return FALSE;
    }

    if (vk >= 'A' && vk <= 'Z') {
        return TRUE;
    }

    if (vk >= '0' && vk <= '9') {
        return TRUE;
    }

    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return TRUE;
    }

    switch (vk) {
        case VK_OEM_1:
        case VK_OEM_PLUS:
        case VK_OEM_COMMA:
        case VK_OEM_MINUS:
        case VK_OEM_PERIOD:
        case VK_OEM_2:
        case VK_OEM_3:
        case VK_OEM_4:
        case VK_OEM_5:
        case VK_OEM_6:
        case VK_OEM_7:
        case VK_SPACE:
        case VK_RETURN:
        case VK_ESCAPE:
        case VK_TAB:
            return TRUE;
    }

    return FALSE;
}

/**
 * @brief Hotkey settings dialog message processing procedure
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            SetWindowTextW(hwndDlg, GetLocalizedString(L"热键设置", L"Hotkey Settings"));

            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL1,
                          GetLocalizedString(L"显示当前时间:", L"Show Current Time:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL2,
                          GetLocalizedString(L"正计时:", L"Count Up:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL12,
                          GetLocalizedString(L"倒计时:", L"Countdown:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL3,
                          GetLocalizedString(L"默认倒计时:", L"Default Countdown:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL9,
                          GetLocalizedString(L"快捷倒计时1:", L"Quick Countdown 1:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL10,
                          GetLocalizedString(L"快捷倒计时2:", L"Quick Countdown 2:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL11,
                          GetLocalizedString(L"快捷倒计时3:", L"Quick Countdown 3:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL4,
                          GetLocalizedString(L"开始番茄钟:", L"Start Pomodoro:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL5,
                          GetLocalizedString(L"隐藏/显示窗口:", L"Hide/Show Window:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL6,
                          GetLocalizedString(L"进入编辑模式:", L"Enter Edit Mode:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL7,
                          GetLocalizedString(L"暂停/继续计时:", L"Pause/Resume Timer:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL8,
                          GetLocalizedString(L"重新开始计时:", L"Restart Timer:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_NOTE,
                          GetLocalizedString(L"* 热键将全局生效", L"* Hotkeys will work globally"));

            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));

            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            ReadConfigHotkeys(&g_dlgShowTimeHotkey, &g_dlgCountUpHotkey, &g_dlgCountdownHotkey,
                             &g_dlgQuickCountdown1Hotkey, &g_dlgQuickCountdown2Hotkey, &g_dlgQuickCountdown3Hotkey,
                             &g_dlgPomodoroHotkey, &g_dlgToggleVisibilityHotkey, &g_dlgEditModeHotkey,
                             &g_dlgPauseResumeHotkey, &g_dlgRestartTimerHotkey);

            ReadCustomCountdownHotkey(&g_dlgCustomCountdownHotkey);

            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_SETHOTKEY, g_dlgShowTimeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_SETHOTKEY, g_dlgCountUpHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_SETHOTKEY, g_dlgCustomCountdownHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_SETHOTKEY, g_dlgCountdownHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_SETHOTKEY, g_dlgQuickCountdown1Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_SETHOTKEY, g_dlgQuickCountdown2Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_SETHOTKEY, g_dlgQuickCountdown3Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_SETHOTKEY, g_dlgPomodoroHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_SETHOTKEY, g_dlgToggleVisibilityHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_SETHOTKEY, g_dlgEditModeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_SETHOTKEY, g_dlgPauseResumeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_SETHOTKEY, g_dlgRestartTimerHotkey, 0);

            UnregisterGlobalHotkeys(GetParent(hwndDlg));

            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT12; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    SetWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i, 0);
                }
            }

            g_OldHotkeyDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)HotkeyDialogSubclassProc);

            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));

            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, RGB(0xF3, 0xF3, 0xF3));
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            }
            return (INT_PTR)hBackgroundBrush;
        }
        
        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }
        
        case WM_LBUTTONDOWN: {
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            HWND hwndHit = ChildWindowFromPoint(hwndDlg, pt);

            if (hwndHit != NULL && hwndHit != hwndDlg) {
                int ctrlId = GetDlgCtrlID(hwndHit);

                BOOL isHotkeyEdit = FALSE;
                for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                    if (ctrlId == i) {
                        isHotkeyEdit = TRUE;
                        break;
                    }
                }

                if (!isHotkeyEdit) {
                    SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                }
            }
            else if (hwndHit == hwndDlg) {
                SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            WORD ctrlId = LOWORD(wParam);
            WORD notifyCode = HIWORD(wParam);

            if (notifyCode == EN_CHANGE &&
                (ctrlId == IDC_HOTKEY_EDIT1 || ctrlId == IDC_HOTKEY_EDIT2 ||
                 ctrlId == IDC_HOTKEY_EDIT3 || ctrlId == IDC_HOTKEY_EDIT4 ||
                 ctrlId == IDC_HOTKEY_EDIT5 || ctrlId == IDC_HOTKEY_EDIT6 ||
                 ctrlId == IDC_HOTKEY_EDIT7 || ctrlId == IDC_HOTKEY_EDIT8 ||
                 ctrlId == IDC_HOTKEY_EDIT9 || ctrlId == IDC_HOTKEY_EDIT10 ||
                 ctrlId == IDC_HOTKEY_EDIT11 || ctrlId == IDC_HOTKEY_EDIT12)) {

                WORD newHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);

                BYTE vk = LOBYTE(newHotkey);
                BYTE modifiers = HIBYTE(newHotkey);

                if (vk == 0xE5 && modifiers == HOTKEYF_SHIFT) {
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }

                if (newHotkey != 0 && IsRestrictedSingleKey(newHotkey)) {
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }

                if (newHotkey != 0) {
                    static const int hotkeyCtrlIds[] = {
                        IDC_HOTKEY_EDIT1, IDC_HOTKEY_EDIT2, IDC_HOTKEY_EDIT3,
                        IDC_HOTKEY_EDIT9, IDC_HOTKEY_EDIT10, IDC_HOTKEY_EDIT11,
                        IDC_HOTKEY_EDIT4, IDC_HOTKEY_EDIT5, IDC_HOTKEY_EDIT6,
                        IDC_HOTKEY_EDIT7, IDC_HOTKEY_EDIT8, IDC_HOTKEY_EDIT12
                    };

                    for (int i = 0; i < sizeof(hotkeyCtrlIds) / sizeof(hotkeyCtrlIds[0]); i++) {
                        if (hotkeyCtrlIds[i] == ctrlId) {
                            continue;
                        }

                        WORD otherHotkey = (WORD)SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_GETHOTKEY, 0, 0);

                        if (otherHotkey != 0 && otherHotkey == newHotkey) {
                            SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_SETHOTKEY, 0, 0);
                        }
                    }
                }

                return TRUE;
            }
            
            switch (LOWORD(wParam)) {
                case IDOK: {
                    WORD showTimeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_GETHOTKEY, 0, 0);
                    WORD countUpHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_GETHOTKEY, 0, 0);
                    WORD customCountdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_GETHOTKEY, 0, 0);
                    WORD countdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown1Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown2Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown3Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_GETHOTKEY, 0, 0);
                    WORD pomodoroHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_GETHOTKEY, 0, 0);
                    WORD toggleVisibilityHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_GETHOTKEY, 0, 0);
                    WORD editModeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_GETHOTKEY, 0, 0);
                    WORD pauseResumeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_GETHOTKEY, 0, 0);
                    WORD restartTimerHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_GETHOTKEY, 0, 0);

                    WORD* hotkeys[] = {
                        &showTimeHotkey, &countUpHotkey, &countdownHotkey,
                        &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                        &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                        &pauseResumeHotkey, &restartTimerHotkey, &customCountdownHotkey
                    };

                    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
                        if (LOBYTE(*hotkeys[i]) == 0xE5 && HIBYTE(*hotkeys[i]) == HOTKEYF_SHIFT) {
                            *hotkeys[i] = 0;
                            continue;
                        }

                        if (*hotkeys[i] != 0 && IsRestrictedSingleKey(*hotkeys[i])) {
                            *hotkeys[i] = 0;
                        }
                    }

                    WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                                      quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                                      pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                                      pauseResumeHotkey, restartTimerHotkey);
                    g_dlgCustomCountdownHotkey = customCountdownHotkey;
                    char customCountdownStr[64] = {0};
                    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
                    WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr);

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

            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT12; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    RemoveWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i);
                }
            }
            break;
    }

    return FALSE;
}

/**
 * @brief Hotkey control subclass procedure
 */
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
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDOK));
                    return 0;
                }
            }
            break;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}