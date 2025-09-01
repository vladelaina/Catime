/**
 * @file hotkey.c
 * @brief Global hotkey management and hotkey settings dialog
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>

/** @brief Architecture-specific manifest dependencies for Common Controls v6 */
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
#include "../include/dialog_procedure.h"
#include "../resource/resource.h"

/** @brief Hotkey modifier flag definitions for compatibility */
#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

/** @brief Dialog-local hotkey storage for show current time */
static WORD g_dlgShowTimeHotkey = 0;
/** @brief Dialog-local hotkey storage for count up timer */
static WORD g_dlgCountUpHotkey = 0;
/** @brief Dialog-local hotkey storage for default countdown */
static WORD g_dlgCountdownHotkey = 0;
/** @brief Dialog-local hotkey storage for custom countdown */
static WORD g_dlgCustomCountdownHotkey = 0;
/** @brief Dialog-local hotkey storage for quick countdown 1 */
static WORD g_dlgQuickCountdown1Hotkey = 0;
/** @brief Dialog-local hotkey storage for quick countdown 2 */
static WORD g_dlgQuickCountdown2Hotkey = 0;
/** @brief Dialog-local hotkey storage for quick countdown 3 */
static WORD g_dlgQuickCountdown3Hotkey = 0;
/** @brief Dialog-local hotkey storage for pomodoro timer */
static WORD g_dlgPomodoroHotkey = 0;
/** @brief Dialog-local hotkey storage for window visibility toggle */
static WORD g_dlgToggleVisibilityHotkey = 0;
/** @brief Dialog-local hotkey storage for edit mode toggle */
static WORD g_dlgEditModeHotkey = 0;
/** @brief Dialog-local hotkey storage for pause/resume timer */
static WORD g_dlgPauseResumeHotkey = 0;
/** @brief Dialog-local hotkey storage for restart timer */
static WORD g_dlgRestartTimerHotkey = 0;

/** @brief Original window procedure for hotkey dialog subclassing */
static WNDPROC g_OldHotkeyDlgProc = NULL;

/**
 * @brief Subclass procedure for hotkey dialog to handle special key processing
 * @param hwnd Window handle
 * @param msg Message identifier
 * @param wParam First message parameter
 * @param lParam Second message parameter
 * @return Message processing result
 */
LRESULT CALLBACK HotkeyDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    /** Handle key events for hotkey conflict detection */
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        BYTE vk = (BYTE)wParam;
        /** Process non-modifier keys */
        if (!(vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)) {
            /** Build current modifier state */
            BYTE currentModifiers = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= HOTKEYF_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= HOTKEYF_CONTROL;
            if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || (GetKeyState(VK_MENU) & 0x8000)) {
                currentModifiers |= HOTKEYF_ALT;
            }

            /** Create hotkey combination from current key event */
            WORD currentEventKeyCombination = MAKEWORD(vk, currentModifiers);

            /** Check if current key combination matches any existing hotkey */
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

            /** Block existing hotkey events unless focused on hotkey edit control */
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

    /** Process specific message types */
    switch (msg) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            {
                /** Allow Alt key combinations only for hotkey edit controls */
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
                /** Allow modifier keys only for hotkey edit controls */
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
            /** Block Alt+Space system menu activation */
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;
    }

    /** Forward to original window procedure */
    return CallWindowProc(g_OldHotkeyDlgProc, hwnd, msg, wParam, lParam);
}

/**
 * @brief Display the hotkey settings dialog
 * @param hwndParent Parent window handle
 */
void ShowHotkeySettingsDialog(HWND hwndParent) {
    DialogBoxW(GetModuleHandle(NULL),
              MAKEINTRESOURCE(CLOCK_IDD_HOTKEY_DIALOG),
              hwndParent,
              HotkeySettingsDlgProc);
}

/**
 * @brief Check if hotkey has no modifiers (single key)
 * @param hotkey Hotkey combination to check
 * @return TRUE if no modifiers present, FALSE otherwise
 */
BOOL IsSingleKey(WORD hotkey) {
    BYTE modifiers = HIBYTE(hotkey);

    return modifiers == 0;
}

/**
 * @brief Check if hotkey is a restricted single key that should be blocked
 * @param hotkey Hotkey combination to check
 * @return TRUE if key is restricted, FALSE if allowed
 */
BOOL IsRestrictedSingleKey(WORD hotkey) {
    if (hotkey == 0) {
        return FALSE;
    }

    BYTE vk = LOBYTE(hotkey);
    BYTE modifiers = HIBYTE(hotkey);

    /** Only check single keys (no modifiers) */
    if (modifiers != 0) {
        return FALSE;
    }

    /** Block alphanumeric keys */
    if (vk >= 'A' && vk <= 'Z') {
        return TRUE;
    }

    if (vk >= '0' && vk <= '9') {
        return TRUE;
    }

    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return TRUE;
    }

    /** Block common punctuation and control keys */
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
 * @brief Dialog procedure for hotkey settings dialog
 * @param hwndDlg Dialog window handle
 * @param msg Message identifier
 * @param wParam First message parameter
 * @param lParam Second message parameter
 * @return Dialog processing result
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            /** Set dialog as topmost window */
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);

            /** Set localized dialog title */
            SetWindowTextW(hwndDlg, GetLocalizedString(L"热键设置", L"Hotkey Settings"));

            /** Set localized labels for all hotkey functions */
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

            /** Set localized button texts */
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));

            /** Create custom brushes for dialog appearance */
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            /** Load current hotkey configuration */
            ReadConfigHotkeys(&g_dlgShowTimeHotkey, &g_dlgCountUpHotkey, &g_dlgCountdownHotkey,
                             &g_dlgQuickCountdown1Hotkey, &g_dlgQuickCountdown2Hotkey, &g_dlgQuickCountdown3Hotkey,
                             &g_dlgPomodoroHotkey, &g_dlgToggleVisibilityHotkey, &g_dlgEditModeHotkey,
                             &g_dlgPauseResumeHotkey, &g_dlgRestartTimerHotkey);

            ReadCustomCountdownHotkey(&g_dlgCustomCountdownHotkey);

            /** Set hotkey values in edit controls */
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

            /** Temporarily unregister global hotkeys while dialog is open */
            UnregisterGlobalHotkeys(GetParent(hwndDlg));

            /** Subclass all hotkey edit controls for custom behavior */
            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT12; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    SetWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i, 0);
                }
            }

            /** Subclass dialog for special key handling */
            g_OldHotkeyDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)HotkeyDialogSubclassProc);

            /** Set initial focus to Cancel button */
            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));

            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            /** Set custom background color for dialog and static controls */
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, RGB(0xF3, 0xF3, 0xF3));
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            }
            return (INT_PTR)hBackgroundBrush;
        }
        
        case WM_CTLCOLORBTN: {
            /** Set custom background color for button controls */
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }
        
        case WM_LBUTTONDOWN: {
            /** Handle mouse clicks to manage focus properly */
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            HWND hwndHit = ChildWindowFromPoint(hwndDlg, pt);

            if (hwndHit != NULL && hwndHit != hwndDlg) {
                int ctrlId = GetDlgCtrlID(hwndHit);

                /** Check if clicked control is a hotkey edit */
                BOOL isHotkeyEdit = FALSE;
                for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                    if (ctrlId == i) {
                        isHotkeyEdit = TRUE;
                        break;
                    }
                }

                /** Set focus to note control if not clicking hotkey edit */
                if (!isHotkeyEdit) {
                    SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                }
            }
            else if (hwndHit == hwndDlg) {
                /** Clicked on dialog background, set focus to note */
                SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            WORD ctrlId = LOWORD(wParam);
            WORD notifyCode = HIWORD(wParam);

            /** Handle hotkey edit control changes */
            if (notifyCode == EN_CHANGE &&
                (ctrlId == IDC_HOTKEY_EDIT1 || ctrlId == IDC_HOTKEY_EDIT2 ||
                 ctrlId == IDC_HOTKEY_EDIT3 || ctrlId == IDC_HOTKEY_EDIT4 ||
                 ctrlId == IDC_HOTKEY_EDIT5 || ctrlId == IDC_HOTKEY_EDIT6 ||
                 ctrlId == IDC_HOTKEY_EDIT7 || ctrlId == IDC_HOTKEY_EDIT8 ||
                 ctrlId == IDC_HOTKEY_EDIT9 || ctrlId == IDC_HOTKEY_EDIT10 ||
                 ctrlId == IDC_HOTKEY_EDIT11 || ctrlId == IDC_HOTKEY_EDIT12)) {

                /** Get new hotkey value from control */
                WORD newHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);

                BYTE vk = LOBYTE(newHotkey);
                BYTE modifiers = HIBYTE(newHotkey);

                /** Block invalid IME-related hotkey combination */
                if (vk == 0xE5 && modifiers == HOTKEYF_SHIFT) {
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }

                /** Block restricted single keys */
                if (newHotkey != 0 && IsRestrictedSingleKey(newHotkey)) {
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }

                /** Prevent duplicate hotkeys across all controls */
                if (newHotkey != 0) {
                    static const int hotkeyCtrlIds[] = {
                        IDC_HOTKEY_EDIT1, IDC_HOTKEY_EDIT2, IDC_HOTKEY_EDIT3,
                        IDC_HOTKEY_EDIT9, IDC_HOTKEY_EDIT10, IDC_HOTKEY_EDIT11,
                        IDC_HOTKEY_EDIT4, IDC_HOTKEY_EDIT5, IDC_HOTKEY_EDIT6,
                        IDC_HOTKEY_EDIT7, IDC_HOTKEY_EDIT8, IDC_HOTKEY_EDIT12
                    };

                    /** Clear duplicate hotkey from other controls */
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
            
            /** Handle button clicks */
            switch (LOWORD(wParam)) {
                case IDOK: {
                    /** Retrieve all hotkey values from controls */
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

                    /** Array of all hotkey pointers for validation */
                    WORD* hotkeys[] = {
                        &showTimeHotkey, &countUpHotkey, &countdownHotkey,
                        &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                        &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                        &pauseResumeHotkey, &restartTimerHotkey, &customCountdownHotkey
                    };

                    /** Final validation and cleanup of invalid hotkeys */
                    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
                        /** Clear IME-related invalid combinations */
                        if (LOBYTE(*hotkeys[i]) == 0xE5 && HIBYTE(*hotkeys[i]) == HOTKEYF_SHIFT) {
                            *hotkeys[i] = 0;
                            continue;
                        }

                        /** Clear restricted single keys */
                        if (*hotkeys[i] != 0 && IsRestrictedSingleKey(*hotkeys[i])) {
                            *hotkeys[i] = 0;
                        }
                    }

                    /** Save hotkeys to configuration */
                    WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                                      quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                                      pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                                      pauseResumeHotkey, restartTimerHotkey);
                    
                    /** Save custom countdown hotkey separately */
                    g_dlgCustomCountdownHotkey = customCountdownHotkey;
                    char customCountdownStr[64] = {0};
                    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
                    WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr);

                    /** Notify parent to re-register global hotkeys */
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);

                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }

                case IDCANCEL:
                    /** Cancel changes and notify parent to re-register hotkeys */
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        
        case WM_DESTROY:
            /** Clean up GDI resources */
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }

            /** Restore original dialog window procedure */
            if (g_OldHotkeyDlgProc) {
                SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)g_OldHotkeyDlgProc);
                g_OldHotkeyDlgProc = NULL;
            }

            /** Remove subclassing from all hotkey edit controls */
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
 * @brief Subclass procedure for hotkey edit controls to handle Enter key
 * @param hwnd Control window handle
 * @param uMsg Message identifier
 * @param wParam First message parameter
 * @param lParam Second message parameter
 * @param uIdSubclass Subclass identifier
 * @param dwRefData Reference data
 * @return Message processing result
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_GETDLGCODE:
            /** Request all keyboard input for hotkey capture */
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;

        case WM_KEYDOWN:
            /** Handle Enter key to trigger OK button */
            if (wParam == VK_RETURN) {
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDOK));
                    return 0;
                }
            }
            break;
    }

    /** Forward to default subclass procedure */
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}