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
            // 获取鼠标点击坐标
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            HWND hwndHit = ChildWindowFromPoint(hwndDlg, pt);
            
            // 如果点击的不是对话框本身，而是某个控件
            if (hwndHit != NULL && hwndHit != hwndDlg) {
                // 获取控件ID
                int ctrlId = GetDlgCtrlID(hwndHit);
                
                // 检查点击的是否为热键输入框
                BOOL isHotkeyEdit = FALSE;
                for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                    if (ctrlId == i) {
                        isHotkeyEdit = TRUE;
                        break;
                    }
                }
                
                // 如果点击的不是热键输入框控件，则清除焦点
                if (!isHotkeyEdit) {
                    // 将焦点设置到静态文本控件上
                    SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                }
            } 
            // 如果点击的是对话框本身（空白区域）
            else if (hwndHit == hwndDlg) {
                // 将焦点设置到静态文本控件上
                SetFocus(GetDlgItem(hwndDlg, IDC_HOTKEY_NOTE));
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND: {
            // 检测热键控件通知消息
            WORD ctrlId = LOWORD(wParam);
            WORD notifyCode = HIWORD(wParam);
            
            // 处理热键编辑控件的变更通知
            if (notifyCode == EN_CHANGE &&
                (ctrlId == IDC_HOTKEY_EDIT1 || ctrlId == IDC_HOTKEY_EDIT2 ||
                 ctrlId == IDC_HOTKEY_EDIT3 || ctrlId == IDC_HOTKEY_EDIT4 ||
                 ctrlId == IDC_HOTKEY_EDIT5 || ctrlId == IDC_HOTKEY_EDIT6 ||
                 ctrlId == IDC_HOTKEY_EDIT7 || ctrlId == IDC_HOTKEY_EDIT8 ||
                 ctrlId == IDC_HOTKEY_EDIT9 || ctrlId == IDC_HOTKEY_EDIT10 ||
                 ctrlId == IDC_HOTKEY_EDIT11 || ctrlId == IDC_HOTKEY_EDIT12)) {
                
                // 获取当前控件的热键值
                WORD newHotkey = (WORD)SendDlgItemMessage(hwndDlg, ctrlId, HKM_GETHOTKEY, 0, 0);
                
                // 提取修饰键和虚拟键
                BYTE vk = LOBYTE(newHotkey);
                BYTE modifiers = HIBYTE(newHotkey);
                
                // 特殊处理: 中文输入法可能会产生Shift+0xE5这种组合，这是无效的
                if (vk == 0xE5 && modifiers == HOTKEYF_SHIFT) {
                    // 清除无效的热键组合
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }
                
                // 检查是否是单独的数字、字母或符号（无修饰键）
                if (newHotkey != 0 && IsRestrictedSingleKey(newHotkey)) {
                    // 发现无效热键，静默清除
                    SendDlgItemMessage(hwndDlg, ctrlId, HKM_SETHOTKEY, 0, 0);
                    return TRUE;
                }
                
                // 如果热键为0（无），则不需要检查冲突
                if (newHotkey != 0) {
                    // 定义热键控件ID数组
                    static const int hotkeyCtrlIds[] = {
                        IDC_HOTKEY_EDIT1, IDC_HOTKEY_EDIT2, IDC_HOTKEY_EDIT3,
                        IDC_HOTKEY_EDIT9, IDC_HOTKEY_EDIT10, IDC_HOTKEY_EDIT11,
                        IDC_HOTKEY_EDIT4, IDC_HOTKEY_EDIT5, IDC_HOTKEY_EDIT6,
                        IDC_HOTKEY_EDIT7, IDC_HOTKEY_EDIT8, IDC_HOTKEY_EDIT12
                    };
                    
                    // 检查是否与其他热键控件冲突
                    for (int i = 0; i < sizeof(hotkeyCtrlIds) / sizeof(hotkeyCtrlIds[0]); i++) {
                        // 跳过当前控件
                        if (hotkeyCtrlIds[i] == ctrlId) {
                            continue;
                        }
                        
                        // 获取其他控件的热键值
                        WORD otherHotkey = (WORD)SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_GETHOTKEY, 0, 0);
                        
                        // 检查是否冲突
                        if (otherHotkey != 0 && otherHotkey == newHotkey) {
                            // 发现冲突，清除旧的热键（设置为0，即"无"）
                            SendDlgItemMessage(hwndDlg, hotkeyCtrlIds[i], HKM_SETHOTKEY, 0, 0);
                        }
                    }
                }
                
                return TRUE;
            }
            
            switch (LOWORD(wParam)) {
                case IDOK: {
                    // 获取热键控件中设置的热键值
                    WORD showTimeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_GETHOTKEY, 0, 0);
                    WORD countUpHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_GETHOTKEY, 0, 0);
                    WORD customCountdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_GETHOTKEY, 0, 0); // 获取新增倒计时热键
                    WORD countdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown1Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown2Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_GETHOTKEY, 0, 0);
                    WORD quickCountdown3Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_GETHOTKEY, 0, 0);
                    WORD pomodoroHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_GETHOTKEY, 0, 0);
                    WORD toggleVisibilityHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_GETHOTKEY, 0, 0);
                    WORD editModeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_GETHOTKEY, 0, 0);
                    WORD pauseResumeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_GETHOTKEY, 0, 0);
                    WORD restartTimerHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_GETHOTKEY, 0, 0);
                    
                    // 再次检查所有热键，确保没有单个键的热键
                    WORD* hotkeys[] = {
                        &showTimeHotkey, &countUpHotkey, &countdownHotkey,
                        &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                        &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                        &pauseResumeHotkey, &restartTimerHotkey, &customCountdownHotkey
                    };
                    
                    // 静默清除任何无效热键
                    // BOOL needsRefresh = FALSE; // (已注释) 用于标记是否需要刷新界面，目前未使用。
                    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
                        // 检查是否是无效的中文输入法热键组合 (Shift+0xE5)
                        if (LOBYTE(*hotkeys[i]) == 0xE5 && HIBYTE(*hotkeys[i]) == HOTKEYF_SHIFT) {
                            *hotkeys[i] = 0;
                            // needsRefresh = TRUE;
                            continue;
                        }
                        
                        if (*hotkeys[i] != 0 && IsRestrictedSingleKey(*hotkeys[i])) {
                            // 发现单键热键，直接置为0
                            *hotkeys[i] = 0;
                            // needsRefresh = TRUE;
                        }
                    }
                    
                    // 使用新的函数保存热键设置到配置文件
                    WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                                      quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                                      pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                                      pauseResumeHotkey, restartTimerHotkey);
                    // 单独保存自定义倒计时热键 - 同时更新全局静态变量
                    g_dlgCustomCountdownHotkey = customCountdownHotkey;
                    char customCountdownStr[64] = {0};
                    HotkeyToString(customCountdownHotkey, customCountdownStr, sizeof(customCountdownStr));
                    WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", customCountdownStr);
                    
                    // 通知主窗口热键设置已更改，需要重新注册
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    
                    // 关闭对话框
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }
                
                case IDCANCEL:
                    // 重新注册原有的热键
                    PostMessage(GetParent(hwndDlg), WM_APP+1, 0, 0);
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
            }
            break;
        }
        
        case WM_DESTROY:
            // 清理资源
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            
            // 如果存在原始窗口过程，恢复它
            if (g_OldHotkeyDlgProc) {
                SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)g_OldHotkeyDlgProc);
                g_OldHotkeyDlgProc = NULL;
            }
            
            // 移除所有热键编辑控件的子类处理函数
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
 * @brief 热键控件子类化处理函数
 * @param hwnd 热键控件窗口句柄
 * @param uMsg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @param uIdSubclass 子类ID
 * @param dwRefData 引用数据
 * @return LRESULT 消息处理结果
 * 
 * 处理热键控件的消息，特别是拦截Alt键和Alt+Shift组合键
 * 防止Windows系统发出提示音
 */
LRESULT CALLBACK HotkeyControlSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, 
                                         LPARAM lParam, UINT_PTR uIdSubclass, 
                                         DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_GETDLGCODE:
            // 告诉Windows我们要处理所有键盘输入，包括Alt和菜单键
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
            
        case WM_KEYDOWN:
            // 处理回车键 - 当按下回车时模拟点击确定按钮
            if (wParam == VK_RETURN) {
                // 获取父对话框句柄
                HWND hwndDlg = GetParent(hwnd);
                if (hwndDlg) {
                    // 发送模拟点击确定按钮的消息
                    SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDOK));
                    return 0;
                }
            }
            break;
    }
    
    // 对于其他所有消息，调用原始的窗口过程
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
} 