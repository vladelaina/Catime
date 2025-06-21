/**
 * @file dialog_procedure.c
 * @brief 对话框消息处理过程实现
 * 
 * 本文件实现了应用程序的对话框消息处理回调函数，
 * 处理对话框的所有消息事件包括初始化、颜色管理、按钮点击和键盘事件。
 */

#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <strsafe.h>
#include <shlobj.h>
#include <uxtheme.h>
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"
#include "../include/language.h"
#include "../include/config.h"
#include "../include/audio_player.h" 
#include "../include/window_procedure.h"  // 添加窗口处理头文件以使用RegisterGlobalHotkeys和UnregisterGlobalHotkeys函数
#include "../include/hotkey.h"  // 引入热键管理头文件
#include "../include/dialog_language.h"  // 添加对话框语言支持头文件

// 函数声明
static void DrawColorSelectButton(HDC hdc, HWND hwnd);

// 从main.c引入的变量
extern char inputText[256];

// 添加番茄钟相关的外部变量声明
#define MAX_POMODORO_TIMES 10  // 保持番茄钟时间数量上限不变
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // 存储所有番茄钟时间
extern int POMODORO_TIMES_COUNT;               // 实际的番茄钟时间数量
extern int POMODORO_WORK_TIME;                 // 番茄钟工作时间（秒）
extern int POMODORO_SHORT_BREAK;               // 番茄钟短休息时间（秒）
extern int POMODORO_LONG_BREAK;                // 番茄钟长休息时间（秒）
extern int POMODORO_LOOP_COUNT;                // 番茄钟循环次数

// 存储旧的编辑框过程
WNDPROC wpOrigEditProc;

// 添加全局变量来跟踪关于对话框句柄
static HWND g_hwndAboutDlg = NULL;

// 添加全局变量来跟踪错误对话框句柄
static HWND g_hwndErrorDlg = NULL;

// 添加全局变量来跟踪倒计时输入对话框句柄
HWND g_hwndInputDialog = NULL;

// 添加循环次数编辑框的子类化过程
static WNDPROC wpOrigLoopEditProc;  // 存储原始的编辑框过程

// 添加常量字符串
#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"

// 子类化编辑框过程
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BOOL firstKeyProcessed = FALSE;
    
    switch (msg) {
    case WM_SETFOCUS:
        // 当获取焦点时，确保选中全部文本
        PostMessage(hwnd, EM_SETSEL, 0, -1);
        // 重置首次按键标记
        firstKeyProcessed = FALSE;
        break;
        
    case WM_KEYDOWN:
        // 处理首次按键问题
        if (!firstKeyProcessed) {
            // 强制清除所有修饰键状态
            // 这有助于解决热键残留状态问题
            firstKeyProcessed = TRUE;
            
            // 标记已经处理了首次按键，但不做特殊处理
            // 让系统正常处理这个按键，避免重复输入
        }
        
        // 回车键处理
        if (wParam == VK_RETURN) {
            // 发送BM_CLICK消息给父窗口的OK按钮
            HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwndOkButton);
            return 0;
        }
        // Ctrl+A全选处理
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    
    case WM_CHAR:
        // 阻止Ctrl+A生成字符，避免发出提示音
        if (wParam == 1 || (wParam == 'a' || wParam == 'A') && GetKeyState(VK_CONTROL) < 0) {
            return 0;
        }
        // 阻止回车键产生的字符消息进一步处理，防止发出提示音
        if (wParam == VK_RETURN) { // VK_RETURN (0x0D) is the char code for Enter
            return 0;
        }
        break;
    }
    
    return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
}

// 在文件开头添加错误对话框处理函数声明
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// 添加显示错误对话框的函数
void ShowErrorDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(IDD_ERROR_DIALOG), 
             hwndParent, 
             ErrorDlgProc);
}

// 添加错误对话框处理函数
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            // 设置本地化的错误提示文本
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT, 
                GetLocalizedString(L"输入格式无效，请重新输入。", L"Invalid input format, please try again."));
            
            // 设置对话框标题
            SetWindowTextW(hwndDlg, GetLocalizedString(L"错误", L"Error"));
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief 输入对话框过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 处理倒计时输入对话框的：
 * 1. 控件初始化与焦点设置
 * 2. 背景/控件颜色管理
 * 3. 确定按钮点击处理
 * 4. 回车键响应
 * 5. 资源清理
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // 保存对话框ID到GWLP_USERDATA
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            // 保存对话框句柄
            g_hwndInputDialog = hwndDlg;
            
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            DWORD dlgId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
            
            // 应用多语言支持 - 通用化处理
            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            // 检查对话框ID是否为快捷倒计时选项对话框，如果是则设置标题 (这部分可能会被ApplyDialogLanguage覆盖，但保留以防万一)
            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) { 
                // SetWindowTextW(hwndDlg, GetLocalizedString(L"Countdown Presets", L"Countdown Presets"));
                // 上一行由 ApplyDialogLanguage 处理，如果 g_dialogTitles 包含 CLOCK_IDD_SHORTCUT_DIALOG
            }
            
            // 获取编辑框控件的句柄
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            // 子类化编辑框控件
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            // 确保输入框获得焦点 - 使用多种方法保证焦点设置
            SetFocus(hwndEdit);
            
            // 使用多个延迟消息，以不同的延迟时间确保焦点和文本选择能正确生效
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);
            
            // 全选编辑框中的文本
            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);
            
            // 设置默认按钮ID
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);

            // 设置一个特殊的定时器，在对话框完全显示后设置焦点
            SetTimer(hwndDlg, 9999, 50, NULL);
            
            // 强制重置所有修饰键状态（防止热键残留）
            // 这会解决使用热键打开对话框后第一个按键被忽略的问题
            PostMessage(hwndDlg, WM_APP+103, 0, 0);
            
            // 设置编译时间（优化后的宽字符处理）
            char month[4];
            int day, year, hour, min, sec;
            
            // 解析编译器生成的日期时间
            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            // 月份缩写转数字
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            // 格式化日期时间为YYYY/MM/DD HH:MM:SS 并添加 UTC+8 标识
            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                    year, month_num, day, hour, min, sec);

            // 设置控件文本
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return FALSE;  
        }

        // 添加处理WM_CLOSE消息的代码，当通过快捷键关闭对话框时不检查输入值合法性
        case WM_CLOSE: {
            // 直接关闭对话框，不进行输入验证
            g_hwndInputDialog = NULL;
            EndDialog(hwndDlg, 0);
            return TRUE;
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

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetBkColor(hdcEdit, RGB(0xFF, 0xFF, 0xFF));
            if (!hEditBrush) {
                hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            }
            return (INT_PTR)hEditBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));
                
                // 检查是否为空输入或只有空格
                BOOL isAllSpaces = TRUE;
                for (int i = 0; inputText[i]; i++) {
                    if (!isspace((unsigned char)inputText[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (inputText[0] == '\0' || isAllSpaces) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }
                
                int total_seconds;
                if (ParseInput(inputText, &total_seconds)) {
                    // 根据对话框ID调用不同的配置更新函数
                    int dialogId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
                    if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                        // 通用番茄钟时间设置，由调用者处理具体更新逻辑
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK); // 返回 IDOK 表示成功
                    } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                        // 番茄钟循环次数
                        WriteConfigPomodoroLoopCount(total_seconds);
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                        // 只有CLOCK_IDD_STARTUP_DIALOG（即"预设管理"->"启动设置"->"倒计时"）才会修改默认启动时间
                        WriteConfigDefaultStartTime(total_seconds);
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                        // 倒计时预设管理
                        WriteConfigDefaultStartTime(total_seconds);
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK);
                    } else {
                        // 对于其他对话框ID（包括CLOCK_IDD_DIALOG1，即普通倒计时），不修改默认启动时间配置
                        g_hwndInputDialog = NULL;
                        EndDialog(hwndDlg, IDOK); // 假设成功获取输入即可
                    }
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetWindowTextA(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT), "");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == 9999) {
                // 定时器用于在对话框完全显示后设置焦点
                KillTimer(hwndDlg, 9999);
                
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    // 确保窗口位于前台
                    SetForegroundWindow(hwndDlg);
                    // 确保编辑框获得焦点
                    SetFocus(hwndEdit);
                    // 全选文本
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                int dlgId = GetDlgCtrlID((HWND)lParam);
                if (dlgId == CLOCK_IDD_COLOR_DIALOG) {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                } else {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                }
                return TRUE;
            }
            break;

        case WM_APP+100:
        case WM_APP+101:
        case WM_APP+102:
            // 延迟执行的焦点和选择设置
            if (lParam) {
                HWND hwndEdit = (HWND)lParam;
                // 确保窗口有效并可见
                if (IsWindow(hwndEdit) && IsWindowVisible(hwndEdit)) {
                    // 确保窗口在前台
                    SetForegroundWindow(hwndDlg);
                    // 设置焦点到输入框
                    SetFocus(hwndEdit);
                    // 全选文本
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            return TRUE;
            
        case WM_APP+103:
            // 强制重置所有修饰键状态
            // 模拟所有可能的修饰键的释放
            // 这会帮助清除任何可能残留的热键状态
            {
                INPUT inputs[8] = {0};
                int inputCount = 0;
                
                // 左Shift键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 右Shift键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 左Ctrl键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 右Ctrl键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 左Alt键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 右Alt键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 左Win键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 右Win键释放
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // 发送所有按键释放事件
                SendInput(inputCount, inputs, sizeof(INPUT));
            }
            return TRUE;

        case WM_DESTROY:
            // 恢复原始编辑框过程
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // 释放资源
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            
            // 清除对话框句柄
            g_hwndInputDialog = NULL;
            }
            break;
    }
    return FALSE;
}

// 关于对话框处理过程
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hLargeIcon = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // 加载大图标，使用定义的尺寸
            hLargeIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_CATIME),
                IMAGE_ICON,
                ABOUT_ICON_SIZE,    // 使用定义的尺寸
                ABOUT_ICON_SIZE,    // 使用定义的尺寸
                LR_DEFAULTCOLOR);
            
            if (hLargeIcon) {
                // 设置静态控件的图标
                SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)hLargeIcon, 0);
            }
            
            // 应用多语言支持
            ApplyDialogLanguage(hwndDlg, IDD_ABOUT_DIALOG);
            
            // 设置版本信息（会覆盖ApplyDialogLanguage的版本设置）
            const wchar_t* versionFormat = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDC_VERSION_TEXT);
            if (versionFormat) {
                wchar_t versionText[256];
                StringCbPrintfW(versionText, sizeof(versionText), versionFormat, CATIME_VERSION);
                SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, versionText);
            }

            // 设置链接文本
            SetDlgItemTextW(hwndDlg, IDC_CREDIT_LINK, GetLocalizedString(L"特别感谢猫屋敷梨梨Official提供的图标", L"Special thanks to Neko House Lili Official for the icon"));
            SetDlgItemTextW(hwndDlg, IDC_CREDITS, GetLocalizedString(L"鸣谢", L"Credits"));
            SetDlgItemTextW(hwndDlg, IDC_BILIBILI_LINK, GetLocalizedString(L"BiliBili", L"BiliBili"));
            SetDlgItemTextW(hwndDlg, IDC_GITHUB_LINK, GetLocalizedString(L"GitHub", L"GitHub"));
            SetDlgItemTextW(hwndDlg, IDC_COPYRIGHT_LINK, GetLocalizedString(L"版权声明", L"Copyright Notice"));
            SetDlgItemTextW(hwndDlg, IDC_SUPPORT, GetLocalizedString(L"支持", L"Support"));

            // 设置编译时间（优化后的宽字符处理）
            char month[4];
            int day, year, hour, min, sec;
            
            // 解析编译器生成的日期时间
            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            // 月份缩写转数字
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            // 获取本地化的日期格式字符串
            const wchar_t* dateFormat = GetLocalizedString(L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                                                         L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)");
            
            // 格式化日期时间
            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), dateFormat,
                    year, month_num, day, hour, min, sec);

            // 设置控件文本
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return TRUE;
        }

        case WM_DESTROY:
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
                hLargeIcon = NULL;
            }
            g_hwndAboutDlg = NULL;  // 清除对话框句柄
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndAboutDlg = NULL;
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDIT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://space.bilibili.com/26087398", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_BILIBILI_LINK) {
                // 直接打开B站个人主页
                ShellExecuteW(NULL, L"open", URL_BILIBILI_SPACE, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_GITHUB_LINK) {
                ShellExecuteW(NULL, L"open", URL_GITHUB_REPO, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDITS) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/#thanks", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_SUPPORT) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support.html", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_COPYRIGHT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime#️copyright-notice", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            // 关闭所有子对话框
            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;  // 清除对话框句柄
            return TRUE;

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            
            if (GetDlgCtrlID(hwndCtl) == IDC_CREDIT_LINK || 
                GetDlgCtrlID(hwndCtl) == IDC_BILIBILI_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_GITHUB_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_CREDITS ||
                GetDlgCtrlID(hwndCtl) == IDC_COPYRIGHT_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_SUPPORT) {
                SetTextColor(hdc, 0x00D26919); // 保持相同的橙色（BGR格式）
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)GetStockObject(NULL_BRUSH);
            }
            break;
        }
    }
    return FALSE;
}

// 添加DPI感知相关类型定义（如果编译器没有提供）
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
// DPI_AWARENESS_CONTEXT是一个HANDLE
typedef HANDLE DPI_AWARENESS_CONTEXT;
// 相关DPI上下文常量定义
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

// 显示关于对话框
void ShowAboutDialog(HWND hwndParent) {
    // 如果已经存在关于对话框，先关闭它
    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    
    // 保存当前DPI感知上下文
    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        // 函数指针类型定义
        typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc = 
            (GetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "GetThreadDpiAwarenessContext");
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {
            // 保存当前DPI上下文
            hOldDpiContext = getThreadDpiAwarenessContextFunc();
            // 设置为每显示器DPI感知V2模式
            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    
    // 创建新的关于对话框
    g_hwndAboutDlg = CreateDialog(GetModuleHandle(NULL), 
                                 MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
                                 hwndParent, 
                                 AboutDlgProc);
    
    // 恢复原来的DPI感知上下文
    if (hUser32 && hOldDpiContext) {
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
    
    ShowWindow(g_hwndAboutDlg, SW_SHOW);
}

// 添加全局变量来跟踪番茄钟循环次数设置对话框句柄
static HWND g_hwndPomodoroLoopDialog = NULL;

void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (!g_hwndPomodoroLoopDialog) {
        g_hwndPomodoroLoopDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
            hwndParent,
            PomodoroLoopDlgProc
        );
        if (g_hwndPomodoroLoopDialog) {
            ShowWindow(g_hwndPomodoroLoopDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroLoopDialog);
    }
}

// 添加循环次数编辑框的子类化过程
LRESULT APIENTRY LoopEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {
            // 发送BM_CLICK消息给父窗口（对话框）
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwnd);
            return 0;
        }
        // 处理Ctrl+A全选
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        // 处理Ctrl+A的字符消息，防止发出提示音
        if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
            return 0;
        }
        // 阻止回车键产生的字符消息进一步处理，防止发出提示音
        if (wParam == VK_RETURN) { // VK_RETURN (0x0D) is the char code for Enter
            return 0;
        }
        break;
    }
    }
    return CallWindowProc(wpOrigLoopEditProc, hwnd, uMsg, wParam, lParam);
}

// 修改辅助函数来处理带空格的数字输入
BOOL IsValidNumberInput(const wchar_t* str) {
    // 检查是否为空
    if (!str || !*str) {
        return FALSE;
    }
    
    BOOL hasDigit = FALSE;  // 用于跟踪是否找到至少一个数字
    wchar_t cleanStr[16] = {0};  // 用于存储清理后的字符串
    int cleanIndex = 0;
    
    // 遍历字符串，忽略空格，只保留数字
    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            cleanStr[cleanIndex++] = str[i];
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {  // 如果不是空格也不是数字，则无效
            return FALSE;
        }
    }
    
    return hasDigit;  // 只要有数字就返回TRUE
}

// 修改 PomodoroLoopDlgProc 函数
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 应用多语言支持
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            
            // 设置静态文本
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(L"请输入循环次数（1-100）：", L"Please enter loop count (1-100):"));
            
            // 设置编辑框焦点
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetFocus(hwndEdit);
            
            // 子类化编辑框控件
            wpOrigLoopEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                          (LONG_PTR)LoopEditSubclassProc);
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
                // 检查是否为空输入或只有空格
                BOOL isAllSpaces = TRUE;
                for (int i = 0; input_str[i]; i++) {
                    if (!iswspace(input_str[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (input_str[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroLoopDialog = NULL;
                    return TRUE;
                }
                
                // 验证输入并处理空格
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                
                // 提取数字（忽略空格）
                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    // 更新配置文件和全局变量
                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroLoopDialog = NULL;
                return TRUE;
            }
            break;

        case WM_DESTROY:
            // 恢复原始编辑框过程
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigLoopEditProc);
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndPomodoroLoopDialog = NULL;
            return TRUE;
    }
    return FALSE;
}

// 添加全局变量跟踪网站URL对话框句柄
static HWND g_hwndWebsiteDialog = NULL;

// 网站URL输入对话框过程
INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // 设置对话框为模态
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            // 设置背景和控件颜色
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // 子类化编辑框以支持回车键提交
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // 如果已有URL，预填充到编辑框
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            
            // 应用多语言支持
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            
            // 设置焦点到编辑框并选中所有文本
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // 因为我们手动设置了焦点
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                char url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url));
                
                // 检查是否为空输入或只有空格
                BOOL isAllSpaces = TRUE;
                for (int i = 0; url[i]; i++) {
                    if (!isspace((unsigned char)url[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (url[0] == '\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                
                // 验证URL格式 - 简单检查，至少应该包含http://或https://
                if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
                    // 添加https://前缀
                    char tempUrl[MAX_PATH] = "https://";
                    StringCbCatA(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyA(url, sizeof(url), tempUrl);
                }
                
                // 更新配置
                WriteConfigTimeoutWebsite(url);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // 用户取消，不更改超时动作
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // 恢复原始编辑框过程
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // 释放资源
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndWebsiteDialog = NULL;
            return TRUE;
    }
    
    return FALSE;
}

// 显示网站URL输入对话框
void ShowWebsiteDialog(HWND hwndParent) {
    // 使用模态对话框代替非模态对话框，这样可以知道用户是确认还是取消
    INT_PTR result = DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );
    
    // 只有用户点击确定，且输入有效URL时才会返回IDOK，此时WebsiteDialogProc已设置CLOCK_TIMEOUT_ACTION
    // 如果用户取消或关闭对话框，不会更改超时动作
}

// 设置全局变量来跟踪番茄钟组合对话框句柄
static HWND g_hwndPomodoroComboDialog = NULL;

// 添加番茄钟组合对话框处理函数
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 设置背景和控件颜色
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // 子类化编辑框以支持回车键提交
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // 从配置中读取当前的番茄钟时间选项并格式化显示
            char currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                char timeStr[32];
                int seconds = POMODORO_TIMES[i];
                
                // 格式化时间，转换为人类可读格式
                if (seconds >= 3600) {
                    int hours = seconds / 3600;
                    int mins = (seconds % 3600) / 60;
                    int secs = seconds % 60;
                    if (mins == 0 && secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh ", hours);
                    else if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm ", hours, mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm%ds ", hours, mins, secs);
                } else if (seconds >= 60) {
                    int mins = seconds / 60;
                    int secs = seconds % 60;
                    if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm ", mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm%ds ", mins, secs);
                } else {
                    StringCbPrintfA(timeStr, sizeof(timeStr), "%ds ", seconds);
                }
                
                StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
            }
            
            // 去掉末尾的空格
            if (strlen(currentOptions) > 0 && currentOptions[strlen(currentOptions) - 1] == ' ') {
                currentOptions[strlen(currentOptions) - 1] = '\0';
            }
            
            // 设置编辑框文本
            SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
            // 应用多语言支持 - 移到这里确保覆盖所有默认文本
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            
            // 设置焦点到编辑框并选中所有文本
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // 因为我们手动设置了焦点
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || LOWORD(wParam) == IDOK) {
                char input[256] = {0};
                GetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, input, sizeof(input));
                
                // 解析输入的时间格式，转换为秒数数组
                char *token, *saveptr;
                char input_copy[256];
                StringCbCopyA(input_copy, sizeof(input_copy), input);
                
                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                
                token = strtok_r(input_copy, " ", &saveptr);
                while (token && times_count < MAX_POMODORO_TIMES) {
                    int seconds = 0;
                    if (ParseTimeInput(token, &seconds)) {
                        times[times_count++] = seconds;
                    }
                    token = strtok_r(NULL, " ", &saveptr);
                }
                
                if (times_count > 0) {
                    // 更新全局变量
                    POMODORO_TIMES_COUNT = times_count;
                    for (int i = 0; i < times_count; i++) {
                        POMODORO_TIMES[i] = times[i];
                    }
                    
                    // 更新基本的番茄钟时间
                    if (times_count > 0) POMODORO_WORK_TIME = times[0];
                    if (times_count > 1) POMODORO_SHORT_BREAK = times[1];
                    if (times_count > 2) POMODORO_LONG_BREAK = times[2];
                    
                    // 写入配置文件
                    WriteConfigPomodoroTimeOptions(times, times_count);
                }
                
                EndDialog(hwndDlg, IDOK);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // 恢复原始编辑框过程
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // 释放资源
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
    }
    
    return FALSE;
}

// 显示番茄钟组合对话框
void ShowPomodoroComboDialog(HWND hwndParent) {
    if (!g_hwndPomodoroComboDialog) {
        g_hwndPomodoroComboDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_COMBO_DIALOG), // 使用新的对话框资源
            hwndParent,
            PomodoroComboDialogProc
        );
        if (g_hwndPomodoroComboDialog) {
            ShowWindow(g_hwndPomodoroComboDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroComboDialog);
    }
}

// 解析时间输入 (如 "25m", "30s", "1h30m" 等)
BOOL ParseTimeInput(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;
    
    *seconds = 0;
    char* buffer = _strdup(input);
    if (!buffer) return FALSE;
    
    int len = strlen(buffer);
    char* pos = buffer;
    int value = 0;
    int tempSeconds = 0;
    
    while (*pos) {
        // 读取数字
        if (isdigit((unsigned char)*pos)) {
            value = 0;
            while (isdigit((unsigned char)*pos)) {
                value = value * 10 + (*pos - '0');
                pos++;
            }
            
            // 读取单位
            if (*pos == 'h' || *pos == 'H') {
                tempSeconds += value * 3600; // 小时转秒
                pos++;
            } else if (*pos == 'm' || *pos == 'M') {
                tempSeconds += value * 60;   // 分钟转秒
                pos++;
            } else if (*pos == 's' || *pos == 'S') {
                tempSeconds += value;        // 秒
                pos++;
            } else if (*pos == '\0') {
                // 没有单位，默认为分钟
                tempSeconds += value * 60;
            } else {
                // 无效字符
                free(buffer);
                return FALSE;
            }
        } else {
            // 非数字起始
            pos++;
        }
    }
    
    free(buffer);
    *seconds = tempSeconds;
    return TRUE;
}

// 添加全局变量来跟踪通知消息对话框句柄
static HWND g_hwndNotificationMessagesDialog = NULL;

// 添加通知消息对话框处理程序
INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 设置窗口置顶
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // 创建画刷
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            // 读取最新配置到全局变量
            ReadNotificationMessagesConfig();
            
            // 为了处理UTF-8中文，我们需要转换到Unicode
            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];
            
            // 第一个编辑框 - 倒计时超时提示
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // 第二个编辑框 - 番茄钟超时提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // 第三个编辑框 - 番茄钟循环完成提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // 本地化标签文本
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1, 
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));
            
            // 本地化按钮文本
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));
            
            // 子类化编辑框以支持Ctrl+A全选
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            // 保存原始的窗口过程
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // 对其他编辑框也应用相同的子类化过程
            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // 全选第一个编辑框文本
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            
            // 设置焦点到第一个编辑框
            SetFocus(GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1));
            
            return FALSE;  // 返回FALSE因为我们手动设置了焦点
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                // 获取编辑框中的文本（Unicode方式）
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // 获取Unicode文本
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // 转换为UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // 保存到配置文件并更新全局变量
                WriteConfigNotificationMessages(timeout_msg, pomodoro_msg, cycle_complete_msg);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // 恢复原始窗口过程
            {
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief 显示通知消息设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示通知消息设置对话框，用于修改各种通知提示文本。
 */
void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (!g_hwndNotificationMessagesDialog) {
        // 确保首先读取最新的配置值
        ReadNotificationMessagesConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG), 
                 hwndParent, 
                 NotificationMessagesDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationMessagesDialog);
    }
}

// 添加全局变量来跟踪通知显示设置对话框句柄
static HWND g_hwndNotificationDisplayDialog = NULL;

// 添加通知显示设置对话框处理程序
INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 设置窗口置顶
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // 创建画刷
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            // 读取最新配置
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            
            // 设置当前值到编辑框
            char buffer[32];
            
            // 显示时间（秒，支持小数点）- 毫秒转为秒
            StringCbPrintfA(buffer, sizeof(buffer), "%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            // 移除末尾的.0
            if (strlen(buffer) > 2 && buffer[strlen(buffer)-2] == '.' && buffer[strlen(buffer)-1] == '0') {
                buffer[strlen(buffer)-2] = '\0';
            }
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, buffer);
            
            // 透明度（百分比）
            StringCbPrintfA(buffer, sizeof(buffer), "%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, buffer);
            
            // 本地化标签文本
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            
            // 修改编辑框风格，移除ES_NUMBER以允许小数点
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            
            // 子类化编辑框以支持回车键提交和限制输入
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // 设置焦点到时间编辑框
            SetFocus(hEditTime);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char timeStr[32] = {0};
                char opacityStr[32] = {0};
                
                // 获取用户输入的值
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, timeStr, sizeof(timeStr));
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, opacityStr, sizeof(opacityStr));
                
                // 使用更健壮的方式替换中文句号
                // 首先获取Unicode格式的文本
                wchar_t wTimeStr[32] = {0};
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wTimeStr, sizeof(wTimeStr)/sizeof(wchar_t));
                
                // 在Unicode文本中替换中文句号
                for (int i = 0; wTimeStr[i] != L'\0'; i++) {
                    // 将多种标点符号都识别为小数点
                    if (wTimeStr[i] == L'。' ||  // 中文句号
                        wTimeStr[i] == L'，' ||  // 中文逗号
                        wTimeStr[i] == L',' ||   // 英文逗号
                        wTimeStr[i] == L'·' ||   // 中文间隔号
                        wTimeStr[i] == L'`' ||   // 反引号
                        wTimeStr[i] == L'：' ||  // 中文冒号
                        wTimeStr[i] == L':' ||   // 英文冒号
                        wTimeStr[i] == L'；' ||  // 中文分号
                        wTimeStr[i] == L';' ||   // 英文分号
                        wTimeStr[i] == L'/' ||   // 斜杠
                        wTimeStr[i] == L'\\' ||  // 反斜杠
                        wTimeStr[i] == L'~' ||   // 波浪号
                        wTimeStr[i] == L'～' ||  // 全角波浪号
                        wTimeStr[i] == L'、' ||  // 顿号
                        wTimeStr[i] == L'．') {  // 全角句点
                        wTimeStr[i] = L'.';      // 替换为英文小数点
                    }
                }
                
                // 将处理后的Unicode文本转回ASCII
                WideCharToMultiByte(CP_ACP, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                
                // 解析时间（秒）并转换为毫秒
                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                
                // 允许时间设置为0（不显示通知）或者至少为100毫秒
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                
                // 解析透明度
                int opacity = atoi(opacityStr);
                
                // 确保透明度在1-100范围内
                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;
                
                // 写入配置
                WriteConfigNotificationTimeout(timeInMs);
                WriteConfigNotificationOpacity(opacity);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            }
            break;
            
        // 添加对WM_CLOSE消息的处理
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationDisplayDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // 恢复原始窗口过程
            {
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEditOpacity, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief 显示通知显示设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示通知显示设置对话框，用于修改通知显示时间和透明度。
 */
void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (!g_hwndNotificationDisplayDialog) {
        // 确保首先读取最新的配置值
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG), 
                 hwndParent, 
                 NotificationDisplayDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationDisplayDialog);
    }
}

// 添加全局变量来跟踪整合后的通知设置对话框句柄
static HWND g_hwndNotificationSettingsDialog = NULL;

/**
 * @brief 音频播放完成回调函数
 * @param hwnd 窗口句柄
 * 
 * 当音频播放完成时，将"结束"按钮变回"测试"按钮
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        const wchar_t* testText = GetLocalizedString(L"Test", L"Test");
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, testText);
        
        // 获取对话框数据
        HWND hwndTestButton = GetDlgItem(hwnd, IDC_TEST_SOUND_BUTTON);
        
        // 发送WM_SETTEXT消息更新按钮文本
        if (hwndTestButton && IsWindow(hwndTestButton)) {
            SendMessageW(hwndTestButton, WM_SETTEXT, 0, (LPARAM)testText);
        }
        
        // 更新全局播放状态
        if (g_hwndNotificationSettingsDialog == hwnd) {
            // 发送消息给对话框，通知播放状态变更
            SendMessage(hwnd, WM_APP + 100, 0, 0);
        }
    }
}

/**
 * @brief 填充音频下拉框
 * @param hwndDlg 对话框句柄
 */
static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;

    // 清空下拉框
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    // 添加"无"选项
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"None", L"None"));
    
    // 添加"系统提示音"选项
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"System Beep", L"System Beep"));

    // 获取音频文件夹路径
    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    // 转换为宽字符路径
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);

    // 构建搜索路径
    wchar_t wSearchPath[MAX_PATH];
    StringCbPrintfW(wSearchPath, sizeof(wSearchPath), L"%s\\*.*", wAudioPath);

    // 查找音频文件 - 使用Unicode版本的API
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // 检查文件扩展名
            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {
                // 直接添加Unicode文件名到下拉框
                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    // 设置当前选中的音频文件
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // 检查是否是系统提示音特殊标记
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            // 选择"系统提示音"选项（索引为1）
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, NOTIFICATION_SOUND_FILE, -1, wSoundFile, MAX_PATH);
            
            // 获取文件名部分
            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            
            // 在下拉框中查找并选择该文件
            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // 选择"无"
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // 选择"无"
    }
}

/**
 * @brief 整合后的通知设置对话框处理程序
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 整合了通知内容和通知显示的统一设置界面
 */
INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE; // 添加一个静态变量来跟踪播放状态
    static int originalVolume = 0; // 添加一个静态变量保存原始音量
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 读取最新配置到全局变量
            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            
            // 保存原始音量值，用于取消操作时恢复
            originalVolume = NOTIFICATION_SOUND_VOLUME;
            
            // 应用多语言支持
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            
            // 设置通知消息文本 - 使用Unicode函数
            wchar_t wideText[256];
            
            // 第一个编辑框 - 倒计时超时提示
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // 第二个编辑框 - 番茄钟超时提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // 第三个编辑框 - 番茄钟循环完成提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // 设置通知显示时间
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            
            // 读取禁用通知设置
            ReadNotificationDisabledConfig();
            
            // 根据禁用状态设置复选框
            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, NOTIFICATION_DISABLED ? BST_CHECKED : BST_UNCHECKED);
            
            // 设置时间控件的启用/禁用状态
            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !NOTIFICATION_DISABLED);
            
            // 设置时间控件的值 - 无论是否禁用都显示实际配置的时间
            int totalSeconds = NOTIFICATION_TIMEOUT_MS / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            
            // 设置时间控件的初始值
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);

            // 设置通知透明度滑动条
            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, NOTIFICATION_MAX_OPACITY);
            
            // 更新透明度文本
            wchar_t opacityText[16];
            StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            
            // 设置通知类型单选按钮
            switch (NOTIFICATION_TYPE) {
                case NOTIFICATION_TYPE_CATIME:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_OS:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_SYSTEM_MODAL:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, BST_CHECKED);
                    break;
            }
            
            // 填充音频下拉框
            PopulateSoundComboBox(hwndDlg);
            
            // 设置音量滑块
            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, NOTIFICATION_SOUND_VOLUME);
            
            // 更新音量文本
            wchar_t volumeText[16];
            StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", NOTIFICATION_SOUND_VOLUME);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            
            // 在初始化时重置播放状态
            isPlaying = FALSE;
            
            // 设置音频播放完成回调函数
            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            
            // 保存对话框句柄
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            return TRUE;
        }
        
        case WM_HSCROLL: {
            // 处理滑块拖动事件
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                // 获取滑块当前位置
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // 更新音量百分比文本
                wchar_t volumeText[16];
                StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                
                // 实时应用音量设置
                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                // 获取滑块当前位置
                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // 更新透明度百分比文本
                wchar_t opacityText[16];
                StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            // 处理禁用通知复选框状态变化
            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                // 获取通知消息文本 - 使用Unicode函数
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // 获取Unicode文本
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // 转换为UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // 获取通知显示时间
                SYSTEMTIME st = {0};
                
                // 检查是否勾选了禁用通知复选框
                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                
                // 保存禁用状态
                NOTIFICATION_DISABLED = isDisabled;
                WriteConfigNotificationDisabled(isDisabled);
                
                // 获取通知时间设置
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    // 计算总秒数: 时*3600 + 分*60 + 秒
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {
                        // 如果时间为00:00:00，设置为0（表示禁用通知）
                        NOTIFICATION_TIMEOUT_MS = 0;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                        
                    } else if (!isDisabled) {
                        // 只有在不禁用的情况下才更新非零的通知时间
                        NOTIFICATION_TIMEOUT_MS = totalSeconds * 1000;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                    }
                }
                // 如果禁用通知，则不修改通知时间配置
                
                // 获取通知透明度（从滑动条获取）
                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    NOTIFICATION_MAX_OPACITY = opacity;
                }
                
                // 获取通知类型
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                
                // 获取选中的音频文件
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (index > 0) { // 0是"无"选项
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    
                    // 检查是否选择了"系统提示音"
                    const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {
                        // 使用特殊标记来表示系统提示音
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                    } else {
                        // 获取音频文件夹路径
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        // 转换为UTF-8路径
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        
                        // 构建完整的文件路径
                        memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                        StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                    }
                } else {
                    NOTIFICATION_SOUND_FILE[0] = '\0';
                }
                
                // 获取音量滑块位置
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                NOTIFICATION_SOUND_VOLUME = volume;
                
                // 保存所有设置
                WriteConfigNotificationMessages(
                    timeout_msg,
                    pomodoro_msg,
                    cycle_complete_msg
                );
                WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                WriteConfigNotificationOpacity(NOTIFICATION_MAX_OPACITY);
                WriteConfigNotificationType(NOTIFICATION_TYPE);
                WriteConfigNotificationSound(NOTIFICATION_SOUND_FILE);
                WriteConfigNotificationVolume(NOTIFICATION_SOUND_VOLUME);
                
                // 确保停止正在播放的音频
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // 关闭对话框前清理回调
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // 确保停止正在播放的音频
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // 恢复原始音量设置
                NOTIFICATION_SOUND_VOLUME = originalVolume;
                
                // 重新应用原始音量
                SetAudioVolume(originalVolume);
                
                // 关闭对话框前清理回调
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                if (!isPlaying) {
                    // 当前不在播放，开始播放并更改按钮文本为"结束"
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) { // 0是"无"选项
                        // 获取当前滑块音量并应用
                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        
                        // 临时保存当前音频设置
                        char tempSoundFile[MAX_PATH];
                        StringCbCopyA(tempSoundFile, sizeof(tempSoundFile), NOTIFICATION_SOUND_FILE);
                        
                        // 临时设置音频文件
                        const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {
                            // 使用特殊标记
                            StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                        } else {
                            // 获取音频文件夹路径
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            // 转换为UTF-8路径
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
                            // 构建完整的文件路径
                            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                            StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                        }
                        
                        // 播放音频
                        if (PlayNotificationSound(hwndDlg)) {
                            // 播放成功，更改按钮文本为"结束"
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Stop", L"Stop"));
                            isPlaying = TRUE;
                        }
                        
                        // 恢复之前的设置
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), tempSoundFile);
                    }
                } else {
                    // 当前正在播放，停止播放并恢复按钮文本
                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Test", L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                // 获取音频目录路径
                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                
                // 确保目录存在
                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                
                // 打开目录
                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                
                // 记录当前选择的音频文件
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // 重新填充音频下拉框
                PopulateSoundComboBox(hwndDlg);
                
                // 尝试恢复之前的选择
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {
                        // 如果找不到之前的选择，默认选择"无"
                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                // 下拉列表将要打开时，重新加载文件列表
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                
                // 记录当前选择的文件
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // 重新填充下拉框
                PopulateSoundComboBox(hwndDlg);
                
                // 恢复之前的选择
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            
        // 添加自定义消息处理，用于音频播放完成通知
        case WM_APP + 100:
            // 音频播放已完成，更新按钮状态
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            // 关闭对话框时确保停止播放
            if (isPlaying) {
                StopNotificationSound();
            }
            
            // 清理回调
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // 对话框销毁时清理回调
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

/**
 * @brief 显示整合后的通知设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示同时包含通知内容和通知显示设置的整合对话框
 */
void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        // 确保首先读取最新的配置值
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                 hwndParent, 
                 NotificationSettingsDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}