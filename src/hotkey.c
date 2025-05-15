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

// 文件作用域的静态变量，用于存储对话框初始化时的热键值
static WORD g_dlgShowTimeHotkey = 0;
static WORD g_dlgCountUpHotkey = 0;
static WORD g_dlgCountdownHotkey = 0;
static WORD g_dlgCustomCountdownHotkey = 0; // 新增倒计时热键
static WORD g_dlgQuickCountdown1Hotkey = 0;
static WORD g_dlgQuickCountdown2Hotkey = 0;
static WORD g_dlgQuickCountdown3Hotkey = 0;
static WORD g_dlgPomodoroHotkey = 0;
static WORD g_dlgToggleVisibilityHotkey = 0;
static WORD g_dlgEditModeHotkey = 0;
static WORD g_dlgPauseResumeHotkey = 0;
static WORD g_dlgRestartTimerHotkey = 0;

// 保存对话框原始窗口过程
static WNDPROC g_OldHotkeyDlgProc = NULL;

/**
 * @brief 对话框子类化处理函数
 * @param hwnd 对话框窗口句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return LRESULT 消息处理结果
 * 
 * 处理对话框的键盘消息，阻止系统提示音
 */
LRESULT CALLBACK HotkeyDialogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 第一部分：检查当前按键消息（按下或释放）是否对应于原始热键
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        BYTE vk = (BYTE)wParam;
        // 仅当事件本身不是单独的修饰键时才继续处理。
        // 单独的修饰键按下/释放事件由后续的 switch 语句进行通用抑制处理。
        if (!(vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN)) {
            BYTE currentModifiers = 0;
            if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= HOTKEYF_SHIFT;
            if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= HOTKEYF_CONTROL;
            // 对于 WM_SYSKEY* 消息，Alt 键已参与其中。对于非 WM_SYSKEY* 消息，需明确检查 Alt 键状态。
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
                        return 0; // 如果焦点不在热键编辑控件上，则抑制原始热键的按下或释放事件
                    }
                } else {
                    return 0; // 如果没有焦点，则抑制原始热键的按下或释放事件
                }
                // 如果焦点在热键编辑控件上，消息将传递给 CallWindowProc，
                // 允许控件自身的子类（如果有）或默认处理。
            }
        }
    }

    // 第二部分：对 WM_SYSKEY* 消息以及特定修饰键的单独按下/释放进行通用抑制
    switch (msg) {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            // 此处处理单独的 Alt 键按下/释放，或非原始热键的 Alt+组合键。
            {
                HWND hwndFocus = GetFocus();
                if (hwndFocus) {
                    DWORD ctrlId = GetDlgCtrlID(hwndFocus);
                    BOOL isHotkeyEditControl = FALSE;
                    for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT11; i++) {
                        if (ctrlId == i) { isHotkeyEditControl = TRUE; break; }
                    }
                    if (isHotkeyEditControl) {
                        break; // 让热键编辑控件的子类处理（最终会调用 CallWindowProc）
                    }
                }
                return 0; // 如果焦点不在热键编辑控件上（或无焦点），则消费通用的 WM_SYSKEY* 消息
            }
            
        case WM_KEYDOWN:
        case WM_KEYUP:
            // 此处处理焦点不在热键编辑控件上时的单独 Shift, Ctrl, Win 键按下/释放。
            // （Alt 键已由上面的 WM_SYSKEYDOWN/WM_SYSKEYUP 处理）。
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
                            return 0; // 如果焦点不在热键编辑控件上，则抑制单独的 Shift/Ctrl/Win 键
                        }
                    } else {
                        return 0; // 如果没有焦点，则抑制该事件
                    }
                }
            }
            // 如果不是单独的修饰键或尚未处理，则传递给 CallWindowProc。
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
    
    // 以下变量用于存储当前设置的热键 - 这些已被移至文件作用域 g_dlg...
    // static WORD showTimeHotkey = 0;
    // static WORD countUpHotkey = 0;
    // ... (其他类似注释掉)
    
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
            
            // 本地化按钮
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));
            
            // 创建背景刷子
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            
            // 使用新函数读取热键配置到文件作用域的静态变量
            ReadConfigHotkeys(&g_dlgShowTimeHotkey, &g_dlgCountUpHotkey, &g_dlgCountdownHotkey,
                             &g_dlgQuickCountdown1Hotkey, &g_dlgQuickCountdown2Hotkey, &g_dlgQuickCountdown3Hotkey,
                             &g_dlgPomodoroHotkey, &g_dlgToggleVisibilityHotkey, &g_dlgEditModeHotkey,
                             &g_dlgPauseResumeHotkey, &g_dlgRestartTimerHotkey);
            
            // 读取自定义倒计时热键
            ReadCustomCountdownHotkey(&g_dlgCustomCountdownHotkey);
            
            // 设置热键控件的初始值
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT1, HKM_SETHOTKEY, g_dlgShowTimeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT2, HKM_SETHOTKEY, g_dlgCountUpHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT12, HKM_SETHOTKEY, g_dlgCustomCountdownHotkey, 0); // 为新的倒计时热键设置初始值
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT3, HKM_SETHOTKEY, g_dlgCountdownHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT9, HKM_SETHOTKEY, g_dlgQuickCountdown1Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT10, HKM_SETHOTKEY, g_dlgQuickCountdown2Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT11, HKM_SETHOTKEY, g_dlgQuickCountdown3Hotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT4, HKM_SETHOTKEY, g_dlgPomodoroHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT5, HKM_SETHOTKEY, g_dlgToggleVisibilityHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT6, HKM_SETHOTKEY, g_dlgEditModeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT7, HKM_SETHOTKEY, g_dlgPauseResumeHotkey, 0);
            SendDlgItemMessage(hwndDlg, IDC_HOTKEY_EDIT8, HKM_SETHOTKEY, g_dlgRestartTimerHotkey, 0);
            
            // 临时注销所有热键，以便用户可以立即测试新的热键设置
            UnregisterGlobalHotkeys(GetParent(hwndDlg));
            
            // 为所有热键编辑控件设置子类处理函数
            for (int i = IDC_HOTKEY_EDIT1; i <= IDC_HOTKEY_EDIT12; i++) {
                HWND hHotkeyCtrl = GetDlgItem(hwndDlg, i);
                if (hHotkeyCtrl) {
                    SetWindowSubclass(hHotkeyCtrl, HotkeyControlSubclassProc, i, 0);
                }
            }
            
            // 对对话框窗口进行子类化，以拦截所有键盘消息
            g_OldHotkeyDlgProc = (WNDPROC)SetWindowLongPtr(hwndDlg, GWLP_WNDPROC, (LONG_PTR)HotkeyDialogSubclassProc);
            
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