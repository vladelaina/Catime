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

// 添加番茄钟相关的外部变量声明
#define MAX_POMODORO_TIMES 10
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
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_KEYDOWN:
        // 回车键处理
        if (wParam == VK_RETURN) {
            // 发送BM_CLICK消息给父窗口的OK按钮
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(GetParent(hwnd), IDOK));
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
                    if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                        // 通用番茄钟时间设置，由调用者处理具体更新逻辑
                        EndDialog(hwndDlg, 0);
                    } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                        // 番茄钟循环次数
                        WriteConfigPomodoroLoopCount(total_seconds);
                        EndDialog(hwndDlg, 0);
                    } else if (dialogId == CLOCK_IDD_DIALOG1 || dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                        // 默认倒计时时间
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

// 设置全局变量来跟踪番茄钟组合对话框句柄
static HWND g_hwndPomodoroComboDialog = NULL;

// 添加番茄钟组合对话框处理函数
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 应用本地化文本（使用对话框已有文本作为默认值）
            WCHAR caption[256] = {0};
            GetWindowTextW(hwndDlg, caption, 256);
            SetWindowTextW(hwndDlg, GetLocalizedString(caption, L"Set Pomodoro Time Combination"));
            
            // 获取静态文本控件当前文本并应用本地化
            HWND hStatic = GetDlgItem(hwndDlg, CLOCK_IDC_STATIC);
            WCHAR staticText[512] = {0};
            GetWindowTextW(hStatic, staticText, 512);
            SetWindowTextW(hStatic, GetLocalizedString(staticText, 
                L"Enter pomodoro time sequence, separated by spaces:\n\n"
                L"25m = 25 minutes\n"
                L"30s = 30 seconds\n"
                L"1h30m = 1 hour 30 minutes\n"
                L"Example: 25m 5m 15m - work 25min, short break 5min, long break 15min"));
            
            // 本地化按钮文本
            HWND hButton = GetDlgItem(hwndDlg, CLOCK_IDC_BUTTON_OK);
            WCHAR buttonText[50] = {0};
            GetWindowTextW(hButton, buttonText, 50);
            SetWindowTextW(hButton, GetLocalizedString(buttonText, L"OK"));
            
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
                        sprintf(timeStr, "%dh ", hours);
                    else if (secs == 0)
                        sprintf(timeStr, "%dh%dm ", hours, mins);
                    else
                        sprintf(timeStr, "%dh%dm%ds ", hours, mins, secs);
                } else if (seconds >= 60) {
                    int mins = seconds / 60;
                    int secs = seconds % 60;
                    if (secs == 0)
                        sprintf(timeStr, "%dm ", mins);
                    else
                        sprintf(timeStr, "%dm%ds ", mins, secs);
                } else {
                    sprintf(timeStr, "%ds ", seconds);
                }
                
                strcat(currentOptions, timeStr);
            }
            
            // 去掉末尾的空格
            if (strlen(currentOptions) > 0 && currentOptions[strlen(currentOptions) - 1] == ' ') {
                currentOptions[strlen(currentOptions) - 1] = '\0';
            }
            
            // 设置编辑框文本
            SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
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
                strncpy(input_copy, input, sizeof(input_copy) - 1);
                
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
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // 释放资源
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            if (hButtonBrush) DeleteObject(hButtonBrush);
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
                           GetLocalizedString(L"倒计时超时提示:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"番茄钟超时提示:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"番茄钟循环完成提示:", L"Pomodoro cycle complete message:"));
            
            // 本地化按钮文本
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"确定", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"取消", L"Cancel"));
            
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
            sprintf(buffer, "%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            // 移除末尾的.0
            if (strlen(buffer) > 2 && buffer[strlen(buffer)-2] == '.' && buffer[strlen(buffer)-1] == '0') {
                buffer[strlen(buffer)-2] = '\0';
            }
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, buffer);
            
            // 透明度（百分比）
            sprintf(buffer, "%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, buffer);
            
            // 本地化标签文本
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"通知显示时间(秒):", L"Notification display time (sec):"));
            
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
                
                // 确保时间至少为3000毫秒（3秒）
                if (timeInMs < 100) timeInMs = 3000;
                
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
            }
            break;
            
        case WM_DESTROY:
            // 恢复原始窗口过程
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEditOpacity, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
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