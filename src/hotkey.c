/**
 * @file hotkey.c
 * @brief Hotkey management implementation
 * 
 * This file implements the hotkey management functionality for the application,
 * including the hotkey settings dialog and hotkey configuration handling.
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>
// Windows Common Controls version 6 (for subclassing)
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

// 热键修饰符标志 - 如果commctrl.h中没有定义
#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

// File scope static variables to store hotkey values when dialog is initialized
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

// Save original dialog window procedure
static WNDPROC g_OldHotkeyDlgProc = NULL;

/**
 * @brief Dialog subclassing procedure
 * @param hwnd Dialog window handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return LRESULT Message processing result
 * 
 * Handles dialog keyboard messages to prevent system alert sounds
 */
LRESULT CALLBACK HotkeyDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Part 1: Check if the current key message (press or release) corresponds to the original hotkey
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        BYTE vk = (BYTE)wParam;
        // Only continue processing when the event itself is not a standalone modifier key.
        // Standalone modifier key press/release events are handled by the subsequent switch statement for general suppression.
        if (!(vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)) {
            BYTE currentModifiers = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= HOTKEYF_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= HOTKEYF_CONTROL;
            // For WM_SYSKEY* messages, Alt key is already involved. For non-WM_SYSKEY* messages, Alt key state needs to be checked explicitly.
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
                        return 0; // If focus is not on a hotkey edit control, suppress the original hotkey press or release event
                    }
                } else {
                    return 0; // If there is no focus, suppress the original hotkey press or release event
                }
                // If focus is on a hotkey edit control, the message will be passed to CallWindowProc,
                // allowing the control's own subclass (if any) or default handling.
            }
        }
    }

    // Part 2: General suppression for WM_SYSKEY* messages and standalone modifier key presses/releases
    switch (msg) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            // Handle standalone Alt key press/release, or Alt+combination keys that are not original hotkeys.
            {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    BOOL isHotkeyEditControl = FALSE;
                    for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                        if (ctrlId == i) { isHotkeyEditControl = TRUE; break; }
                    }
                    if (isHotkeyEditControl) {
                        break; // Let the hotkey edit control's subclass handle it (which will eventually call CallWindowProc)
                    }
                }
                return 0; // If focus is not on a hotkey edit control (or no focus), consume the general WM_SYSKEY* message
            }
            
        case WM_KEYDOWN:
        case WM_KEYUP:
            // Handle standalone Shift, Ctrl, Win key press/release when focus is not on a hotkey edit control.
            // (Alt key is already handled by WM_SYSKEYDOWN/WM_SYSKEYUP above).
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
                            return 0; // If focus is not on a hotkey edit control, suppress standalone Shift/Ctrl/Win key
                        }
                    } else {
                        return 0; // If there is no focus, suppress the event
                    }
                }
            }
            // If not a standalone modifier key or not yet handled, pass to CallWindowProc.
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
 * @param hwndParent Parent window handle
 */
void ShowHotkeySettingsDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(CLOCK_IDD_HOTKEY_DIALOG), 
             hwndParent, 
             HotkeySettingsDlgProc);
}

/**
 * @brief Check if a hotkey is a single key
 * @param hotkey Hotkey value
 * @return BOOL Returns TRUE if it's a single key, FALSE otherwise
 * 
 * Checks if the hotkey contains only a single key without any modifier keys
 */
BOOL IsSingleKey(WORD hotkey) {
    // Extract modifier part
    BYTE modifiers = HIBYTE(hotkey);
    
    // No modifiers (Alt, Ctrl, Shift)
    return modifiers == 0;
}

/**
 * @brief Check if a hotkey is a standalone letter, number, or symbol
 * @param hotkey Hotkey value
 * @return BOOL Returns TRUE if it's a standalone letter, number, or symbol
 * 
 * Checks if the hotkey lacks modifier keys and only contains a single letter, number, or symbol
 */
BOOL IsRestrictedSingleKey(WORD hotkey) {
    // If hotkey is 0, it means not set, return FALSE directly
    if (hotkey == 0) {
        return FALSE;
    }
    
    // Extract modifier keys and virtual key
    BYTE vk = LOBYTE(hotkey);
    BYTE modifiers = HIBYTE(hotkey);
    
    // If there are any modifier keys, it's a combination key, return FALSE
    if (modifiers != 0) {
        return FALSE;
    }
    
    // Determine if it's a letter, number or symbol
    // Virtual key codes reference: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    
    // Letter keys (A-Z)
    if (vk >= 'A' && vk <= 'Z') {
        return TRUE;
    }
    
    // Number keys (0-9)
    if (vk >= '0' && vk <= '9') {
        return TRUE;
    }
    
    // Numeric keypad (VK_NUMPAD0 - VK_NUMPAD9)
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return TRUE;
    }
    
    // Various symbol keys
    switch (vk) {
        case VK_OEM_1:      // ;:
        case VK_OEM_PLUS:   // +
        case VK_OEM_COMMA:  // ,
        case VK_OEM_MINUS:  // -
        case VK_OEM_PERIOD: // .
        case VK_OEM_2:      // /?
        case VK_OEM_3:      // `~
        case VK_OEM_4:      // [{
        case VK_OEM_5:      // \|
        case VK_OEM_6:      // ]}
        case VK_OEM_7:      // '"
        case VK_SPACE:      // Space
        case VK_RETURN:     // Enter
        case VK_ESCAPE:     // ESC
        case VK_TAB:        // Tab
            return TRUE;
    }
    
    // Other single function keys are allowed, such as F1-F12
    
    return FALSE;
}

/**
 * @brief Hotkey settings dialog message processing procedure
 * @param hwndDlg Dialog handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message processing result
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    // The following variables used to store current hotkey settings - these have been moved to file scope g_dlg...
    // static WORD showTimeHotkey = 0;
    // static WORD countUpHotkey = 0;
    // ... (other similar commented out variables)
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set dialog topmost
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // Apply localized text
            SetWindowTextW(hwndDlg, GetLocalizedString(L"热键设置", L"Hotkey Settings"));
            
            // Localize labels
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
            
            // Localize buttons
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));
            
            // Create background brushes
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            
            // Use new function to read hotkey configuration into file scope static variables
            ReadConfigHotkeys(&g_dlgShowTimeHotkey, &g_dlgCountUpHotkey, &g_dlgCountdownHotkey,
                             &g_dlgQuickCountdown1Hotkey, &g_dlgQuickCountdown2Hotkey, &g_dlgQuickCountdown3Hotkey,
                             &g_dlgPomodoroHotkey, &g_dlgToggleVisibilityHotkey, &g_dlgEditModeHotkey,
                             &g_dlgPauseResumeHotkey, &g_dlgRestartTimerHotkey);
            
            // Read custom countdown hotkey
            ReadCustomCountdownHotkey(&g_dlgCustomCountdownHotkey);
            
            // Set initial values for hotkey controls
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_SETHOTKEY, g_dlgShowTimeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_SETHOTKEY, g_dlgCountUpHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_SETHOTKEY, g_dlgCustomCountdownHotkey, 0); // Set initial value for new countdown hotkey
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_SETHOTKEY, g_dlgCountdownHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_SETHOTKEY, g_dlgQuickCountdown1Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_SETHOTKEY, g_dlgQuickCountdown2Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_SETHOTKEY, g_dlgQuickCountdown3Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_SETHOTKEY, g_dlgPomodoroHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_SETHOTKEY, g_dlgToggleVisibilityHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_SETHOTKEY, g_dlgEditModeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_SETHOTKEY, g_dlgPauseResumeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_SETHOTKEY, g_dlgRestartTimerHotkey, 0);
            
            // Temporarily unregister all hotkeys so the user can immediately test new hotkey settings
            UnregisterGlobalHotkeys(GetParent(hwndDlg));
            
            // Set subclass procedure for all hotkey edit controls
            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT12; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    SetWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i, 0);
                }
            }
            
            // Subclass the dialog window to intercept all keyboard messages
            g_OldHotkeyDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)HotkeyDialogSubclassProc);
            
            // Prevent dialog from automatically setting focus to the first input control
            // This way the dialog won't suddenly enter input state when opened
            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));
            
            return FALSE;  // Return FALSE to indicate we manually set the focus, no need for system default behavior
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
            // Get mouse click coordinates
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            HWND hwndHit = ChildWindowFromPoint(hwndDlg, pt);
            
            // If click is not on the dialog itself, but on a control
            if (hwndHit != NULL && hwndHit != hwndDlg) {
                // Get control ID
                int ctrlId = GetDlgCtrlID(hwndHit);
                
                // Check if the clicked control is a hotkey input box
                BOOL isHotkeyEdit = FALSE;
                for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                    if (ctrlId == i) {
                        isHotkeyEdit = TRUE;
                        break;
                    }
                }
                
                // If the clicked control is not a hotkey input box, clear focus
                if (!isHotkeyEdit) {
                    // Set focus to static text control
                    SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                }
            } 
            // If click is on the dialog itself (blank area)
            else if (hwndHit == hwndDlg) {
                // Set focus to static text control
                SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            // Detect hotkey control notification messages
            WORD ctrlId = LOWORD(wParam);
            WORD notifyCode = HIWORD(wParam);
            
            // Handle hotkey edit control change notifications
            if (notifyCode == EN_CHANGE &&
                (ctrlId == IDC_HOTKEY_EDIT1 || ctrlId == IDC_HOTKEY_EDIT2 ||
                 ctrlId == IDC_HOTKEY_EDIT3 || ctrlId == IDC_HOTKEY_EDIT4 ||
                 ctrlId == IDC_HOTKEY_EDIT5 || ctrlId == IDC_HOTKEY_EDIT6 ||
                 ctrlId == IDC_HOTKEY_EDIT7 || ctrlId == IDC_HOTKEY_EDIT8 ||
                 ctrlId == IDC_HOTKEY_EDIT9 || ctrlId == IDC_HOTKEY_EDIT10 ||
                 ctrlId == IDC_HOTKEY_EDIT11 || ctrlId == IDC_HOTKEY_EDIT12)) {
                
                // Get current control's hotkey value
                WORD newHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);
                
                // Extract modifier and virtual key
                BYTE vk = LOBYTE(newHotkey);
                BYTE modifiers = HIBYTE(newHotkey);
                
                // Special handling: Chinese input method may produce Shift+0xE5 combination, which is invalid
                if (vk == 0xE5 && modifiers == HOTKEYF_SHIFT) {
                    // Clear invalid hotkey combination
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }
                
                // Check if it's a standalone number, letter, or symbol (no modifier)
                if (newHotkey != 0 && IsRestrictedSingleKey(newHotkey)) {
                    // Found invalid hotkey, silently clear it
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }
                
                // If hotkey is 0 (none), no need to check for conflicts
                if (newHotkey != 0) {
                    // Define hotkey control ID array
                    static const int hotkeyCtrlIds[] = {
                        IDC_HOTKEY_EDIT1, IDC_HOTKEY_EDIT2, IDC_HOTKEY_EDIT3,
                        IDC_HOTKEY_EDIT9, IDC_HOTKEY_EDIT10, IDC_HOTKEY_EDIT11,
                        IDC_HOTKEY_EDIT4, IDC_HOTKEY_EDIT5, IDC_HOTKEY_EDIT6,
                        IDC_HOTKEY_EDIT7, IDC_HOTKEY_EDIT8, IDC_HOTKEY_EDIT12
                    };
                    
                    // Check for conflicts with other hotkey controls
                    for (int i = 0; i < sizeof(hotkeyCtrlIds) / sizeof(hotkeyCtrlIds[0]); i++) {
                        // Skip current control
                        if (hotkeyCtrlIds[i] == ctrlId) {
                            continue;
                        }
                        
                        // Get hotkey value from other control
                        WORD otherHotkey = (WORD)SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_GETHOTKEY, 0, 0);
                        
                        // Check for conflict
                        if (otherHotkey != 0 && otherHotkey == newHotkey) {
                            // Found conflict, clear the old hotkey (set to 0, meaning "none")
                            SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_SETHOTKEY, 0, 0);
                        }
                    }
                }
                
                return TRUE;
            }
            
            switch (LOWORD(wParam)) {
                case IDOK: {
                    // Get hotkey values set in hotkey controls
                    WORD showTimeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_GETHOTKEY, 0, 0);
                    WORD countUpHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_GETHOTKEY, 0, 0);
                    WORD customCountdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_GETHOTKEY, 0, 0); // Get new countdown hotkey
                    WORD countdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown1Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown2Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown3Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_GETHOTKEY, 0, 0);
                    WORD pomodoroHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_GETHOTKEY, 0, 0);
                    WORD toggleVisibilityHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_GETHOTKEY, 0, 0);
                    WORD editModeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_GETHOTKEY, 0, 0);
                    WORD pauseResumeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_GETHOTKEY, 0, 0);
                    WORD restartTimerHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_GETHOTKEY, 0, 0);
                    
                    // Check all hotkeys again to ensure there are no single-key hotkeys
                    WORD* hotkeys[] = {
                        &showTimeHotkey, &countUpHotkey, &countdownHotkey,
                        &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                        &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                        &pauseResumeHotkey, &restartTimerHotkey, &customCountdownHotkey
                    };
                    
                    // Silently clear any invalid hotkeys
                    // BOOL needsRefresh = FALSE; // (commented out) Used to mark whether UI refresh is needed, currently unused.
                    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
                        // Check for invalid Chinese input method hotkey combination (Shift+0xE5)
                        if (LOBYTE(*hotkeys[i]) == 0xE5 && HIBYTE(*hotkeys[i]) == HOTKEYF_SHIFT) {
                            *hotkeys[i] = 0;
                            // needsRefresh = TRUE;
                            continue;
                        }
                        
                        if (*hotkeys[i] != 0 && IsRestrictedSingleKey(*hotkeys[i])) {
                            // Found single-key hotkey, set to 0 directly
                            *hotkeys[i] = 0;
                            // needsRefresh = TRUE;
                        }
                    }
                    
                    // Use the new function to save hotkey settings to configuration file
                    WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                                      quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                                      pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                                      pauseResumeHotkey, restartTimerHotkey);
                    // Save custom countdown hotkey separately - also update global static variable
                    g_dlgCustomCountdownHotkey = customCountdownHotkey;
                    char customCountdownStr[64] = {0};
                    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
                    WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr);
                    
                    // Notify main window that hotkey settings have changed and need to be re-registered
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    
                    // Close dialog
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }
                
                case IDCANCEL:
                    // Re-register original hotkeys
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        
        case WM_DESTROY:
            // Clean up resources
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            
            // If original window procedure exists, restore it
            if (g_OldHotkeyDlgProc) {
                SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)g_OldHotkeyDlgProc);
                g_OldHotkeyDlgProc = NULL;
            }
            
            // Remove subclass procedure from all hotkey edit controls
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
 * @param hwnd Hotkey control window handle
 * @param uMsg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @param uIdSubclass Subclass ID
 * @param dwRefData Reference data
 * @return LRESULT Message processing result
 * 
 * Handles hotkey control messages, especially intercepting Alt key and Alt+Shift combinations
 * to prevent Windows system alert sounds
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_GETDLGCODE:
            // Tell Windows we want to handle all keyboard input, including Alt and menu keys
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
            
        case WM_KEYDOWN:
            // Handle Enter key - simulate clicking the OK button when Enter is pressed
            if (wParam == VK_RETURN) {
                // Get parent dialog handle
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    // Send message to simulate clicking the OK button
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDOK));
                    return 0;
                }
            }
            break;
    }
    
    // For all other messages, call the original window procedure
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
} 