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
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"
#include "../include/language.h"
#include <commctrl.h>
#include "../include/config.h"
#include <windowsx.h>
#include <shellapi.h>

// 函数声明
void ParseContributorInfo(const wchar_t* contributor, wchar_t* name, size_t nameSize, wchar_t* url, size_t urlSize);

// 从main.c引入的变量
extern char inputText[256];

// 存储旧的编辑框过程
WNDPROC wpOrigEditProc;

// 添加全局变量来跟踪关于对话框句柄
static HWND g_hwndAboutDlg = NULL;

// 添加全局变量来跟踪鸣谢对话框句柄
static HWND g_hwndCreditsDialog = NULL;

// 添加全局变量来跟踪支持对话框句柄
static HWND g_hwndSupportDialog = NULL;

// 添加全局变量来跟踪许可证对话框句柄
static HWND g_hwndLicenseDialog = NULL;

// 添加循环次数编辑框的子类化过程
static WNDPROC wpOrigLoopEditProc;  // 存储原始的编辑框过程

// 贡献者链接定义
static const wchar_t* CONTRIBUTOR_LINKS[] = {
    L"[MAX°孟兆](https://github.com/MadMaxChow)",              // CONTRIBUTOR_1
    L"[XuJilong](https://github.com/sumruler)",                // CONTRIBUTOR_2
    L"[zggsong](https://github.com/ZGGSONG)",                  // CONTRIBUTOR_3
    L"[猫屋敷梨梨Official](https://space.bilibili.com/26087398)", // CONTRIBUTOR_4
    L"[MOJIもら](https://space.bilibili.com/6189012)",         // CONTRIBUTOR_5
    L"[李康](https://space.bilibili.com/475437261)",           // CONTRIBUTOR_6
    L"[我是无名吖](https://space.bilibili.com/1708573954)",     // CONTRIBUTOR_7
    L"[flying-hilichurl](https://github.com/flying-hilichurl)", // CONTRIBUTOR_8
    L"[双脚猫](https://space.bilibili.com/161061562)",         // CONTRIBUTOR_9
    L"[rsyqvthv](https://github.com/rsyqvthv)",                // CONTRIBUTOR_10
    L"[洋仓鼠](https://space.bilibili.com/297146893)",         // CONTRIBUTOR_11
    L"[学习马楼](https://space.bilibili.com/3546380188519387)", // CONTRIBUTOR_12
    L"[睡着的火山](https://space.bilibili.com/8010065)",        // CONTRIBUTOR_13
    L"[星空下数羊](https://space.bilibili.com/5549978)",        // CONTRIBUTOR_14
    L"[青阳忘川](https://space.bilibili.com/13129221)",         // CONTRIBUTOR_15
    L"[William](https://github.com/llfWilliam)",               // CONTRIBUTOR_16
    L"[王野](https://github.com/wangye99)",                    // CONTRIBUTOR_17
    L"[风增](https://space.bilibili.com/470931145)",           // CONTRIBUTOR_18
    L"[煮酒论科技](https://space.bilibili.com/572042200)",       // CONTRIBUTOR_19
    L"[田春](https://space.bilibili.com/266931550)"            // CONTRIBUTOR_20 - 新增
};

// 子类化编辑框过程
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {
            // 发送BM_CLICK消息给父窗口（对话框）
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)hwnd);
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
        break;
    }
    }
    return CallWindowProc(wpOrigEditProc, hwnd, uMsg, wParam, lParam);
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
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            // 获取编辑框控件的句柄
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            // 子类化编辑框控件
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            // 全选编辑框中的文本
            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);

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

            // 格式化日期时间为YYYY/MM/DD HH:MM:SS
            wchar_t timeStr[50];
            swprintf(timeStr, 50, L"最后编译日期：%04d/%02d/%02d %02d:%02d:%02d",
                    year, month_num, day, hour, min, sec);

            // 设置控件文本
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

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
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }
                
                int total_seconds;
                if (ParseInput(inputText, &total_seconds)) {
                    // 根据对话框ID调用不同的配置更新函数
                    int dialogId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
                    if (dialogId == CLOCK_IDD_POMODORO_WORK_DIALOG) {
                        WriteConfigPomodoroTimes(total_seconds, POMODORO_SHORT_BREAK, POMODORO_LONG_BREAK);
                        EndDialog(hwndDlg, 0);
                    } else if (dialogId == CLOCK_IDD_POMODORO_BREAK_DIALOG) {
                        WriteConfigPomodoroTimes(POMODORO_WORK_TIME, total_seconds, POMODORO_LONG_BREAK);
                        EndDialog(hwndDlg, 0);
                    } else if (dialogId == CLOCK_IDD_POMODORO_LBREAK_DIALOG) {
                        WriteConfigPomodoroTimes(POMODORO_WORK_TIME, POMODORO_SHORT_BREAK, total_seconds);
                        EndDialog(hwndDlg, 0);
                    } else if (dialogId == CLOCK_IDD_DIALOG1 || dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                        WriteConfigDefaultStartTime(total_seconds);
                        EndDialog(hwndDlg, 0);
                    } else {
                        EndDialog(hwndDlg, 0);
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

        case WM_DESTROY:
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
            // 恢复原始编辑框过程
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
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
            
            // 设置程序名称和版本信息
            SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, IDC_ABOUT_VERSION CATIME_VERSION);

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

            // 格式化日期时间为YYYY/MM/DD HH:MM:SS
            wchar_t timeStr[50];
            swprintf(timeStr, 50, L"最后编译日期：%04d/%02d/%02d %02d:%02d:%02d",
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
            // 关闭所有子对话框
            if (g_hwndCreditsDialog && IsWindow(g_hwndCreditsDialog)) {
                EndDialog(g_hwndCreditsDialog, 0);
                g_hwndCreditsDialog = NULL;
            }
            if (g_hwndSupportDialog && IsWindow(g_hwndSupportDialog)) {
                EndDialog(g_hwndSupportDialog, 0);
                g_hwndSupportDialog = NULL;
            }
            if (g_hwndLicenseDialog && IsWindow(g_hwndLicenseDialog)) {
                EndDialog(g_hwndLicenseDialog, 0);
                g_hwndLicenseDialog = NULL;
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
                wchar_t name[256] = {0}, url[512] = {0};
                ParseContributorInfo(CONTRIBUTOR_LINKS[3], name, 256, url, 512);
                ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_FEEDBACK_LINK) {
                ShellExecuteW(NULL, L"open", URL_FEEDBACK, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_GITHUB_LINK) {
                ShellExecuteW(NULL, L"open", URL_GITHUB_REPO, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDITS) {
                ShowCreditsDialog(hwndDlg);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_SUPPORT) {
                ShowSupportDialog(hwndDlg);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_COPYRIGHT_LINK) {
                ShowLicenseDialog(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            // 关闭所有子对话框
            if (g_hwndCreditsDialog && IsWindow(g_hwndCreditsDialog)) {
                EndDialog(g_hwndCreditsDialog, 0);
                g_hwndCreditsDialog = NULL;
            }
            if (g_hwndSupportDialog && IsWindow(g_hwndSupportDialog)) {
                EndDialog(g_hwndSupportDialog, 0);
                g_hwndSupportDialog = NULL;
            }
            if (g_hwndLicenseDialog && IsWindow(g_hwndLicenseDialog)) {
                EndDialog(g_hwndLicenseDialog, 0);
                g_hwndLicenseDialog = NULL;
            }
            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;  // 清除对话框句柄
            return TRUE;

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            
            if (GetDlgCtrlID(hwndCtl) == IDC_CREDIT_LINK || 
                GetDlgCtrlID(hwndCtl) == IDC_FEEDBACK_LINK ||
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
        typedef DPI_AWARENESS_CONTEXT (WINAPI* GetThreadDpiAwarenessContextFunc)();
        typedef DPI_AWARENESS_CONTEXT (WINAPI* SetThreadDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
        
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
        typedef DPI_AWARENESS_CONTEXT (WINAPI* SetThreadDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
    
    ShowWindow(g_hwndAboutDlg, SW_SHOW);
}

// 添加辅助函数来解析贡献者信息
void ParseContributorInfo(const wchar_t* contributor, wchar_t* name, size_t nameSize, wchar_t* url, size_t urlSize) {
    const wchar_t *start = wcschr(contributor, L'[');
    const wchar_t *middle = wcschr(contributor, L']');
    const wchar_t *urlStart = wcschr(contributor, L'(');
    const wchar_t *urlEnd = wcschr(contributor, L')');
    
    if (start && middle && urlStart && urlEnd) {
        // 提取名称 (不包含方括号)
        size_t nameLen = middle - (start + 1);
        if (nameLen < nameSize) {
            wcsncpy(name, start + 1, nameLen);
            name[nameLen] = L'\0';
        }
        
        // 提取URL (不包含圆括号)
        size_t urlLen = urlEnd - (urlStart + 1);
        if (urlLen < urlSize) {
            wcsncpy(url, urlStart + 1, urlLen);
            url[urlLen] = L'\0';
        }
    }
}

// 修改鸣谢对话框处理过程
INT_PTR CALLBACK CreditsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            // 处理确定按钮
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            
            // 处理贡献者链接点击
            switch (LOWORD(wParam))
            {
                case IDC_CREDITS_MAX: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[0], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_XUJILONG: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[1], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_ZGGSONG: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[2], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_NEKO: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[3], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_MOJI: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[4], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_LIKANG: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[5], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_WUMING: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[6], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_FLYING: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[7], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_CAT: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[8], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_RSYQVTHV: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[9], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_HAMSTER: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[10], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_MALOU: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[11], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_VOLCANO: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[12], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_SHEEP: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[13], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_QINGYANG: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[14], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_WILLIAM: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[15], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_WANGYE: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[16], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_FENGZENG: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[17], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_ZHUJIU: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[18], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
                case IDC_CREDITS_TIANCHUN: {
                    wchar_t name[256] = {0}, url[512] = {0};
                    ParseContributorInfo(CONTRIBUTOR_LINKS[19], name, 256, url, 512);
                    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
                    return TRUE;
                }
            }
            break;

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            int ctrlId = GetDlgCtrlID(hwndCtl);
            
            // 为所有贡献者链接设置橙色
            if (ctrlId >= IDC_CREDITS_MAX && ctrlId <= IDC_CREDITS_TIANCHUN) {
                SetTextColor(hdc, 0x00D26919); // 橙色 (BGR格式)
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)GetStockObject(NULL_BRUSH);
            }
            break;
        }
    }
    return FALSE;
}

// 显示鸣谢对话框
void ShowCreditsDialog(HWND hwndParent) {
    // 如果已经存在鸣谢对话框，先关闭它
    if (g_hwndCreditsDialog != NULL && IsWindow(g_hwndCreditsDialog)) {
        EndDialog(g_hwndCreditsDialog, 0);
        g_hwndCreditsDialog = NULL;
    }
    
    // 创建新的鸣谢对话框
    g_hwndCreditsDialog = CreateDialog(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCE(IDD_CREDITS_DIALOG), 
                                     hwndParent, 
                                     CreditsDlgProc);
    ShowWindow(g_hwndCreditsDialog, SW_SHOW);
}

// 支持对话框处理过程
INT_PTR CALLBACK SupportDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HICON hWechatIcon = NULL;
    static HICON hAlipayIcon = NULL;

    switch (msg)
    {
        case WM_INITDIALOG:
            // 加载大尺寸的支付图标
            hWechatIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_WECHAT),
                IMAGE_ICON,
                228,    // 宽度
                228,    // 高度
                LR_DEFAULTCOLOR);
            
            hAlipayIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_ALIPAY),
                IMAGE_ICON,
                228,    // 宽度
                228,    // 高度
                LR_DEFAULTCOLOR);
            
            // 设置图标到Static控件
            if (hWechatIcon) {
                SendDlgItemMessage(hwndDlg, IDC_SUPPORT_WECHAT, STM_SETICON, (WPARAM)hWechatIcon, 0);
            }
            
            if (hAlipayIcon) {
                SendDlgItemMessage(hwndDlg, IDC_SUPPORT_ALIPAY, STM_SETICON, (WPARAM)hAlipayIcon, 0);
            }
            
            return TRUE;

        case WM_DESTROY:
            // 清理图标资源
            if (hWechatIcon) {
                DestroyIcon(hWechatIcon);
                hWechatIcon = NULL;
            }
            if (hAlipayIcon) {
                DestroyIcon(hAlipayIcon);
                hAlipayIcon = NULL;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndSupportDialog = NULL;
                return TRUE;
            }
            break;
    }
    return FALSE;
}

// 显示支持对话框
void ShowSupportDialog(HWND hwndParent) {
    // 如果已经存在支持对话框，先关闭它
    if (g_hwndSupportDialog != NULL && IsWindow(g_hwndSupportDialog)) {
        EndDialog(g_hwndSupportDialog, 0);
        g_hwndSupportDialog = NULL;
    }
    
    // 创建新的支持对话框
    g_hwndSupportDialog = CreateDialog(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCE(IDD_SUPPORT_DIALOG), 
                                     hwndParent, 
                                     SupportDlgProc);
    ShowWindow(g_hwndSupportDialog, SW_SHOW);
}

// 许可证对话框处理过程
INT_PTR CALLBACK LicenseDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndLicenseDialog = NULL;
                return TRUE;
            }
            break;
    }
    return FALSE;
}

// 显示许可证对话框
void ShowLicenseDialog(HWND hwndParent) {
    // 如果已经存在许可证对话框，先关闭它
    if (g_hwndLicenseDialog != NULL && IsWindow(g_hwndLicenseDialog)) {
        EndDialog(g_hwndLicenseDialog, 0);
        g_hwndLicenseDialog = NULL;
    }
    
    // 创建新的许可证对话框
    g_hwndLicenseDialog = CreateDialog(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCE(IDD_LICENSE_DIALOG), 
                                     hwndParent, 
                                     LicenseDlgProc);
    ShowWindow(g_hwndLicenseDialog, SW_SHOW);
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
                if (new_loop_count >= 1 && new_loop_count <= 99) {
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
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigLoopEditProc);
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
                    strncat(tempUrl, url, MAX_PATH - 9);
                    strncpy(url, tempUrl, MAX_PATH - 1);
                }
                
                // 更新配置
                WriteConfigTimeoutWebsite(url);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // 恢复原始编辑框过程
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
    if (!g_hwndWebsiteDialog) {
        g_hwndWebsiteDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
            hwndParent,
            WebsiteDialogProc
        );
        if (g_hwndWebsiteDialog) {
            ShowWindow(g_hwndWebsiteDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndWebsiteDialog);
    }
}