/**
 * @file hotkey.c
 * @brief 热键管理实现
 * 
 * 本文件实现了应用程序的热键管理功能，
 * 包括热键设置对话框和热键配置处理。
 */

#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>
#include <windowsx.h>
#include <wchar.h>
// Windows通用控件版本6（用于子类化）
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

/**
 * @brief 显示热键设置对话框
 * @param hwndParent 父窗口句柄
 */
void ShowHotkeySettingsDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(CLOCK_IDD_HOTKEY_DIALOG), 
             hwndParent, 
             HotkeySettingsDlgProc);
}

/**
 * @brief 检查热键是否是单个按键
 * @param hotkey 热键值
 * @return BOOL 如果是单个按键则返回TRUE，否则返回FALSE
 * 
 * 检查热键是否只包含一个单一的按键，不包含修饰键
 */
BOOL IsSingleKey(WORD hotkey) {
    // 提取修饰键部分
    BYTE modifiers = HIBYTE(hotkey);
    
    // 没有修饰键（Alt, Ctrl, Shift）
    return modifiers == 0;
}

/**
 * @brief 检查热键值是否是单独的字母、数字或符号
 * @param hotkey 热键值
 * @return BOOL 如果是单独的字母、数字或符号则返回TRUE
 * 
 * 检查热键值是否缺少修饰键，仅仅包含单个字母、数字或符号
 */
BOOL IsRestrictedSingleKey(WORD hotkey) {
    // 如果热键为0，表示未设置，直接返回FALSE
    if (hotkey == 0) {
        return FALSE;
    }
    
    // 提取修饰键和虚拟键
    BYTE vk = LOBYTE(hotkey);
    BYTE modifiers = HIBYTE(hotkey);
    
    // 如果有任何修饰键，则是组合键，返回FALSE
    if (modifiers != 0) {
        return FALSE;
    }
    
    // 判断是否是字母、数字或符号
    // 虚拟键码参考: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    
    // 字母键 (A-Z)
    if (vk >= 'A' && vk <= 'Z') {
        return TRUE;
    }
    
    // 数字键 (0-9)
    if (vk >= '0' && vk <= '9') {
        return TRUE;
    }
    
    // 数字小键盘 (VK_NUMPAD0 - VK_NUMPAD9)
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return TRUE;
    }
    
    // 各种符号键
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
        case VK_SPACE:      // 空格
        case VK_RETURN:     // 回车
        case VK_ESCAPE:     // ESC
        case VK_TAB:        // Tab
            return TRUE;
    }
    
    // 其他单个功能键允许使用，比如F1-F12
    
    return FALSE;
}

/**
 * @brief 热键设置对话框消息处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 */
INT_PTR CALLBACK HotkeySettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    // 以下变量用于存储当前设置的热键
    static WORD showTimeHotkey = 0;        // 显示当前时间的热键
    static WORD countUpHotkey = 0;         // 正计时的热键
    static WORD countdownHotkey = 0;       // 倒计时的热键
    static WORD quickCountdown1Hotkey = 0; // 快捷倒计时1的热键
    static WORD quickCountdown2Hotkey = 0; // 快捷倒计时2的热键
    static WORD quickCountdown3Hotkey = 0; // 快捷倒计时3的热键
    static WORD pomodoroHotkey = 0;        // 番茄钟的热键
    static WORD toggleVisibilityHotkey = 0; // 隐藏/显示的热键
    static WORD editModeHotkey = 0;        // 编辑模式的热键
    static WORD pauseResumeHotkey = 0;     // 暂停/继续的热键
    static WORD restartTimerHotkey = 0;    // 重新开始的热键
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 设置对话框置顶
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // 应用本地化文本
            SetWindowTextW(hwndDlg, GetLocalizedString(L"热键设置", L"Hotkey Settings"));
            
            // 本地化标签
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL1, 
                          GetLocalizedString(L"显示当前时间:", L"Show Current Time:"));
            SetDlgItemTextW(hwndDlg, IDC_HOTKEY_LABEL2, 
                          GetLocalizedString(L"正计时:", L"Count Up:"));
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
            
            // 本地化按钮
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));
            
            // 创建背景刷子
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            
            // 使用新函数读取热键配置
            ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                             &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                             &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                             &pauseResumeHotkey, &restartTimerHotkey);
            
            // 设置热键控件的初始值
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_SETHOTKEY, showTimeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_SETHOTKEY, countUpHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_SETHOTKEY, countdownHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_SETHOTKEY, quickCountdown1Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_SETHOTKEY, quickCountdown2Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_SETHOTKEY, quickCountdown3Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_SETHOTKEY, pomodoroHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_SETHOTKEY, toggleVisibilityHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_SETHOTKEY, editModeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_SETHOTKEY, pauseResumeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_SETHOTKEY, restartTimerHotkey, 0);
            
            // 临时注销所有热键，以便用户可以立即测试新的热键设置
            UnregisterGlobalHotkeys(GetParent(hwndDlg));
            
            // 为所有热键编辑控件设置子类处理函数
            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    SetWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i, 0);
                }
            }
            
            // 阻止对话框自动设置焦点到第一个输入控件
            // 这样对话框打开时就不会突然进入输入状态
            SetFocus(GetDlgItem(hwndDlg, IDCANCEL));
            
            return FALSE;  // 返回FALSE表示我们已手动设置了焦点，不需要系统默认行为
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
                 ctrlId == IDC_HOTKEY_EDIT11)) {
                
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
                        IDC_HOTKEY_EDIT7, IDC_HOTKEY_EDIT8
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
                    // 获取热键控件中设置的值
                    WORD newShowTimeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_GETHOTKEY, 0, 0);
                    WORD newCountUpHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_GETHOTKEY, 0, 0);
                    WORD newCountdownHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_GETHOTKEY, 0, 0);
                    WORD newQuickCountdown1Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_GETHOTKEY, 0, 0);
                    WORD newQuickCountdown2Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_GETHOTKEY, 0, 0);
                    WORD newQuickCountdown3Hotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_GETHOTKEY, 0, 0);
                    WORD newPomodoroHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_GETHOTKEY, 0, 0);
                    WORD newToggleVisibilityHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_GETHOTKEY, 0, 0);
                    WORD newEditModeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_GETHOTKEY, 0, 0);
                    WORD newPauseResumeHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_GETHOTKEY, 0, 0);
                    WORD newRestartTimerHotkey = (WORD)SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_GETHOTKEY, 0, 0);
                    
                    // 再次检查所有热键，确保没有单个键的热键
                    WORD* hotkeys[] = {
                        &newShowTimeHotkey, &newCountUpHotkey, &newCountdownHotkey,
                        &newQuickCountdown1Hotkey, &newQuickCountdown2Hotkey, &newQuickCountdown3Hotkey,
                        &newPomodoroHotkey, &newToggleVisibilityHotkey, &newEditModeHotkey,
                        &newPauseResumeHotkey, &newRestartTimerHotkey
                    };
                    
                    // 静默清除任何无效热键
                    BOOL needsRefresh = FALSE;
                    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
                        // 检查是否是无效的中文输入法热键组合 (Shift+0xE5)
                        if (LOBYTE(*hotkeys[i]) == 0xE5 && HIBYTE(*hotkeys[i]) == HOTKEYF_SHIFT) {
                            *hotkeys[i] = 0;
                            needsRefresh = TRUE;
                            continue;
                        }
                        
                        if (*hotkeys[i] != 0 && IsRestrictedSingleKey(*hotkeys[i])) {
                            // 发现单键热键，直接置为0
                            *hotkeys[i] = 0;
                            needsRefresh = TRUE;
                        }
                    }
                    
                    // 使用新的函数保存热键设置到配置文件
                    WriteConfigHotkeys(newShowTimeHotkey, newCountUpHotkey, newCountdownHotkey,
                                      newQuickCountdown1Hotkey, newQuickCountdown2Hotkey, newQuickCountdown3Hotkey,
                                      newPomodoroHotkey, newToggleVisibilityHotkey, newEditModeHotkey,
                                      newPauseResumeHotkey, newRestartTimerHotkey);
                    
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
            
            // 移除所有热键编辑控件的子类处理函数
            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
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
            
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            // 处理Alt键和Alt+Shift等组合键，防止系统发出提示音
            if (wParam == VK_MENU || wParam == VK_SHIFT || 
                wParam == VK_CONTROL || wParam == VK_LWIN || wParam == VK_RWIN) {
                // 正常处理这些键，但阻止默认的系统音效
                return 0;
            }
            break;
            
        case WM_KEYDOWN:
            // 处理正常按键，但当与Alt组合时要特殊处理
            if (GetKeyState(VK_MENU) < 0) {
                // Alt键被按下的情况
                if (wParam == VK_SHIFT || wParam == VK_CONTROL) {
                    // 阻止Alt+Shift和Alt+Ctrl组合键的系统提示音
                    return 0;
                }
            }
            break;
    }
    
    // 对于其他所有消息，调用原始的窗口过程
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
} 