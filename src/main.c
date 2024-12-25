#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 窗口和显示相关的定义
#define TEXT_COLOR "#F5DAE3"   

// 窗口尺寸定义
#define BASE_WINDOW_WIDTH 200    // 基础窗口宽度
#define BASE_WINDOW_HEIGHT 65   // 基础窗口高度
#define WINDOW_SCALE 1.0       // 窗口尺寸缩放因子

// 计算实际窗口尺寸
#define WINDOW_WIDTH (int)(BASE_WINDOW_WIDTH * WINDOW_SCALE)
#define WINDOW_HEIGHT (int)(BASE_WINDOW_HEIGHT * WINDOW_SCALE)

// 窗口位置定义
#define WINDOW_POS_X 1
#define WINDOW_POS_Y 15

// 字体相关定义
#define BASE_FONT_SIZE 48      // 基准字体大小
#define FONT_SCALE_FACTOR 1.0  // 字体缩放因子（相对于窗口大小）
#define FONT_SIZE (int)(BASE_FONT_SIZE * FONT_SCALE_FACTOR * WINDOW_SCALE)  // 实际使用的字体大小

#define DEFAULT_START_TIME 1500  // 默认启动时的倒计时时长为 25 分钟 = 1500 秒
#define ID_TRAY_APP_ICON  1001
#define WM_TRAYICON        (WM_USER + 1)  // 自定义消息 ID
#define WINDOW_CLASS_NAME  "CatimeWindow"  // 确保窗口类名唯一
// 定义控件ID
#define IDC_EDIT 108
#define IDC_BUTTON_OK 109
#define IDD_DIALOG1 1002  // 确保与 .rc 文件中的 ID 一致

// 定义时间选项
#define TIME_OPTIONS {8, 10, 25}  // 定义时间选项为数组

// 全局变量用于保存输入内容
char inputText[256] = {0};  // 设置全局变量

static int elapsed_time = 0;  // 已经过的时间，全局变量
static int TOTAL_TIME = DEFAULT_START_TIME;  // 全局倒计时总时间
NOTIFYICONDATA nid;  // 托盘图标数据

// 用于存储时间选项
int time_options[] = TIME_OPTIONS;  // 使用定义的时间选项
int time_options_count = sizeof(time_options) / sizeof(time_options[0]);  // 计算时间选项的数量

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);  // 函数声明

// 对话框过程函数
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SendMessage(hwndDlg, DM_SETDEFID, IDC_BUTTON_OK, 0);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, IDC_EDIT, inputText, sizeof(inputText));
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, IDC_BUTTON_OK, 0);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

// 格式化倒计时文本的函数
void FormatTime(int remaining_time, char* time_text) {
    int minutes = remaining_time / 60;
    int seconds = remaining_time % 60;
    if (minutes == 0 && seconds < 10) {
        sprintf(time_text, "%d", seconds);  // 只显示秒数
    } else if (minutes == 0) {
        sprintf(time_text, "%d", seconds);
    } else {
        sprintf(time_text, "%d:%02d", minutes, seconds);
    }
}

// 退出程序的函数
void ExitProgram(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    PostQuitMessage(0);
}

// 托盘图标的右键菜单响应函数
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    // 将 "Customize" 选项放在最上面
    AppendMenu(hMenu, MF_STRING, 101, "Customize");

    // 添加选项：根据 TIME_OPTIONS 的值动态生成菜单项
    for (int i = 0; i < time_options_count; i++) {
        char menu_item[10];
        sprintf(menu_item, "%d", time_options[i]);
        AppendMenu(hMenu, MF_STRING, 102 + i, menu_item);  // 动态添加菜单项
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // 确保菜单显示在应用程序的窗口上
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// 处理消息的窗口过程函数
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static char time_text[50];
    UINT uID;
    UINT uMouseMsg;

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            int remaining_time = TOTAL_TIME - elapsed_time;

            if (elapsed_time >= TOTAL_TIME) {
                sprintf(time_text, "Time's up!");
            } else {
                FormatTime(remaining_time, time_text);
            }

            // 创建字体，使用等比例缩放
            HFONT hFont = CreateFont(
                -FONT_SIZE,                 // 高度（负值表示使用字符的实际高度）
                0,                          // 宽度（0表示自动按照高度等比例缩放）
                0,                          // 文本角度
                0,                          // 基线角度
                FW_BOLD,                    // 字体粗细
                FALSE,                      // 斜体
                FALSE,                      // 下划线
                FALSE,                      // 删除线
                DEFAULT_CHARSET,            // 字符集
                OUT_TT_PRECIS,             // 输出精度
                CLIP_DEFAULT_PRECIS,        // 裁剪精度
                ANTIALIASED_QUALITY,        // 输出质量
                FF_DONTCARE | DEFAULT_PITCH,// 字体族
                "Arial"                     // 字体名称
            );
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            // 解析颜色字符串
            int r, g, b;
            sscanf(TEXT_COLOR + 1, "%02x%02x%02x", &r, &g, &b); // 跳过 '#' 并解析 RGB 值
            
            COLORREF color = RGB(r, g, b);
            SetTextColor(hdc, color);
            SetBkMode(hdc, TRANSPARENT);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            // 计算文本位置以居中显示
            SIZE textSize;
            GetTextExtentPoint32(hdc, time_text, strlen(time_text), &textSize);
            int x = (WINDOW_WIDTH - textSize.cx) / 2;
            int y = (WINDOW_HEIGHT - textSize.cy) / 2;
            
            TextOutA(hdc, x, y, time_text, strlen(time_text));
            
            // 清理资源
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (elapsed_time < TOTAL_TIME) {
                elapsed_time++;
                InvalidateRect(hwnd, NULL, TRUE);
            } else {
                InvalidateRect(hwnd, NULL, TRUE);
                MessageBox(hwnd, "Time's up! The specified time has passed.", "Catime", MB_OK);
                KillTimer(hwnd, 1);
            }
            break;
        }
        case WM_DESTROY: {
            ExitProgram(hwnd);
            break;
        }
        case WM_TRAYICON: {
            uID = (UINT)wp;
            uMouseMsg = (UINT)lp;

            if (uMouseMsg == WM_LBUTTONDOWN) {
                ExitProgram(hwnd);
            } else if (uMouseMsg == WM_RBUTTONUP) {
                ShowContextMenu(hwnd);
            }
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case 101:   // Customize  
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);  

                    int input_time = 0;

                    if (inputText[0] == '\0') {
                        TOTAL_TIME = DEFAULT_START_TIME;  // 使用定义的默认启动时间
                    }
                    else if (inputText[strlen(inputText) - 1] == 's') {
                        inputText[strlen(inputText) - 1] = '\0';
                        input_time = atoi(inputText);
                        TOTAL_TIME = input_time;
                    }
                    else {
                        input_time = atoi(inputText);
                        if (input_time == 0) {
                            TOTAL_TIME = 0;
                        } else {
                            TOTAL_TIME = input_time * 60;
                        }
                    }
                    elapsed_time = 0;  
                    break;
                // 根据菜单项的索引设置 TOTAL_TIME
                default:
                    if (LOWORD(wp) >= 102 && LOWORD(wp) < 102 + time_options_count) {
                        int index = LOWORD(wp) - 102;  // 计算选中的菜单项索引
                        TOTAL_TIME = time_options[index] * 60;  // 将分钟转换为秒
                        elapsed_time = 0;
                    }
                    break;
            }
            if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
                MessageBox(hwnd, "Failed to set timer!", "Error", MB_OK);
            }
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// GUI 程序的入口点 WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (lpCmdLine != NULL && strlen(lpCmdLine) > 0) {
        char* time_str = lpCmdLine;
        int length = strlen(time_str);
        if (length > 1 && time_str[length - 1] == 's') {
            time_str[length - 1] = '\0';
            TOTAL_TIME = atoi(time_str);
            if (TOTAL_TIME <= 0) {
                TOTAL_TIME = DEFAULT_START_TIME;  // 使用定义的默认启动时间
            }
        } else {
            TOTAL_TIME = atoi(time_str) * 60;
            if (TOTAL_TIME <= 0) {
                TOTAL_TIME = DEFAULT_START_TIME;  // 使用定义的默认启动时间
            }
        }
    }

    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwnd = FindWindow(WINDOW_CLASS_NAME, "Catime");
        if (hwnd) {
            COPYDATASTRUCT cds;
            cds.dwData = 1;
            char time_str[10];
            sprintf(time_str, "%d", TOTAL_TIME);
            cds.lpData = time_str;
            cds.cbData = strlen(time_str) + 1;
            SendMessage(hwnd, WM_COPYDATA, (WPARAM)hwnd, (LPARAM)&cds);
        }
        return 0;
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        WINDOW_CLASS_NAME,
        "Catime",
        WS_POPUP,
        WINDOW_POS_X, WINDOW_POS_Y,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    nid.cbSize = sizeof(nid);
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = (HICON)LoadImage(NULL, "asset/images/catime.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    nid.hWnd = hwnd;
    nid.uCallbackMessage = WM_TRAYICON;
    strcpy(nid.szTip, "Catime");
    Shell_NotifyIcon(NIM_ADD, &nid);

    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
        MessageBox(hwnd, "Failed to set timer!", "Error", MB_OK);
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    return (int)msg.wParam;
}
