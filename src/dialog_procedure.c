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

// 从main.c引入的变量
extern char inputText[256];

// 存储旧的编辑框过程
WNDPROC wpOrigEditProc;

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
        break;
    }
    }
    return CallWindowProc(wpOrigEditProc, hwnd, uMsg, wParam, lParam);
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
            wchar_t timeStr[20];
            swprintf(timeStr, 20, L"%04d/%02d/%02d %02d:%02d:%02d",
                    year, month_num, day, hour, min, sec);

            // 设置控件文本
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, L"最后编译日期：");
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE+1, timeStr);

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
                int total_seconds;
                if (ParseInput(inputText, &total_seconds)) {
                    WriteConfigDefaultStartTime(total_seconds);
                    EndDialog(hwndDlg, 0);
                } else {
                    MessageBox(hwndDlg, "Invalid time format. Please use MM:SS or HH:MM:SS.", "Error", MB_OK | MB_ICONERROR);
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
            wchar_t timeStr[20];
            swprintf(timeStr, 20, L"%04d/%02d/%02d %02d:%02d:%02d",
                    year, month_num, day, hour, min, sec);

            // 设置控件文本
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, L"最后编译日期：");
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE+1, timeStr);

            return TRUE;
        }

        case WM_DESTROY:
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
                hLargeIcon = NULL;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;
    }
    return FALSE;
}

// 显示关于对话框
void ShowAboutDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
             hwndParent, 
             AboutDlgProc);
}