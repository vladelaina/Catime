// x86_64-w64-mingw32-windres resource.rc -o resource.o
// x86_64-w64-mingw32-gcc -o /mnt/c/Users/vladelaina/Desktop/name.exe test.c resource.o -mwindows
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_TIME 1500  // 默认倒计时时长为 25 分钟 = 1500 秒
#define ID_TRAY_APP_ICON  1001
#define WM_TRAYICON        (WM_USER + 1)  // 自定义消息 ID
#define WINDOW_CLASS_NAME  "CatimeWindow"  // 确保窗口类名唯一
// 定义控件ID
#define IDC_EDIT 108
#define IDC_BUTTON_OK 109
#define IDD_DIALOG1 1002  // 确保与 .rc 文件中的 ID 一致
// 全局变量用于保存输入内容
char inputText[256] = {0};  // 设置全局变量

static int elapsed_time = 0;  // 已经过的时间，全局变量
static int TOTAL_TIME = DEFAULT_TIME;  // 全局倒计时总时间
NOTIFYICONDATA nid;  // 托盘图标数据


LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);  // 函数声明

// 对话框过程函数
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            // 设置 "OK" 按钮为默认按钮
            SendMessage(hwndDlg, DM_SETDEFID, IDC_BUTTON_OK, 0);
            return TRUE;

        case WM_COMMAND:
            // 检查按钮点击或回车键按下
            if (LOWORD(wParam) == IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                // 获取输入框中的内容
                GetDlgItemText(hwndDlg, IDC_EDIT, inputText, sizeof(inputText));

                // 关闭对话框
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            // 检查回车键
            if (wParam == VK_RETURN) {
                // 模拟点击 "OK" 按钮
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
    // 删除托盘图标
    Shell_NotifyIcon(NIM_DELETE, &nid);  // 使用 Shell_NotifyIcon 删除托盘图标
    // 退出消息循环
    PostQuitMessage(0);
}

// 托盘图标的右键菜单响应函数
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    // 添加选项：5, 10, 25 分钟
    AppendMenu(hMenu, MF_STRING, 101, "Customize");
    AppendMenu(hMenu, MF_STRING, 102, "5");
    AppendMenu(hMenu, MF_STRING, 103, "10");
    AppendMenu(hMenu, MF_STRING, 104, "25");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // 确保菜单显示在应用程序的窗口上
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// 处理消息的窗口过程函数
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static char time_text[50];  // 存储倒计时文本
    UINT uID;
    UINT uMouseMsg;

    switch (msg) {
        case WM_PAINT: {
            // 获取绘图上下文
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            int remaining_time = TOTAL_TIME - elapsed_time;

            // 如果倒计时已经结束，不显示时间
            if (elapsed_time >= TOTAL_TIME) {
                sprintf(time_text, "Time's up!");  // 显示“时间到！”
            } else {
                FormatTime(remaining_time, time_text);
            }

            SetTextColor(hdc, RGB(255, 255, 255));  // 设置文本颜色
            SetBkMode(hdc, TRANSPARENT);  // 设置背景透明
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            TextOutA(hdc, 10, 10, time_text, strlen(time_text));
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            // 每秒更新倒计时
            if (elapsed_time < TOTAL_TIME) {
                elapsed_time++;
                InvalidateRect(hwnd, NULL, TRUE);  // 强制重绘窗口
            } else {
                InvalidateRect(hwnd, NULL, TRUE);  // 最后一次更新
                MessageBox(hwnd, "Time's up! The specified time has passed.", "Catime", MB_OK);
                KillTimer(hwnd, 1);  // 停止定时器
            }
            break;
        }
        case WM_DESTROY: {
            ExitProgram(hwnd);  // 删除托盘图标并退出
            break;
        }
        case WM_TRAYICON: {
            uID = (UINT)wp;
            uMouseMsg = (UINT)lp;

            if (uMouseMsg == WM_LBUTTONDOWN) {
                // 左键点击托盘图标时，直接退出程序
                ExitProgram(hwnd);  // 调用 ExitProgram 删除托盘图标并退出程序
            } else if (uMouseMsg == WM_RBUTTONUP) {
                ShowContextMenu(hwnd);  // 显示右键菜单
            }
            break;

        }
        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case 101:   // Customize  
                    // 弹出对话框以获取用户输入
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);  

                    // 获取输入框中的内容
                    int input_time = 0;

                    // 如果输入为空（空字符串），则默认设置为 25 分钟
                    if (inputText[0] == '\0') {
                        TOTAL_TIME = 25 * 60;  // 默认设置为 25 分钟（25 * 60 秒）
                    }
                    // 判断输入内容是否包含 's'（秒）
                    else if (inputText[strlen(inputText) - 1] == 's') {
                        // 如果最后一个字符是 's'，去掉 's' 后再转为整数
                        inputText[strlen(inputText) - 1] = '\0';  // 去掉 's'
                        input_time = atoi(inputText);  // 转换为整数

                        // 直接设置为输入的秒数
                        TOTAL_TIME = input_time;  // 设置为秒数
                    }
                    // 如果没有 's' 后缀，表示是分钟数
                    else {
                        input_time = atoi(inputText);  // 转换为整数

                        if (input_time == 0) {
                            // 如果输入的时间为 0，防止直接结束计时
                            TOTAL_TIME = 0;  // 可以选择结束计时或设置为 0
                        } else {
                            TOTAL_TIME = input_time * 60;  // 将输入的分钟数转换为秒
                        }
                    }

                    // 重置已用时间
                    elapsed_time = 0;  

                    break;
                case 102:  // 5 minutes
                    TOTAL_TIME = 5 * 60;  // 设置为5分钟
                    elapsed_time = 0;  // 重置计时器
                    break;
                case 103:  // 10 minutes
                    TOTAL_TIME = 10 * 60;  // 设置为10分钟
                    elapsed_time = 0;  // 重置计时器
                    break;
                case 104:  // 25 minutes
                    TOTAL_TIME = 25 * 60;  // 设置为25分钟
                    elapsed_time = 0;  // 重置计时器
                    break;
            }
            // 重启定时器
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
    // 解析命令行参数
    if (lpCmdLine != NULL && strlen(lpCmdLine) > 0) {
        char* time_str = lpCmdLine;
        int length = strlen(time_str);
        if (length > 1 && time_str[length - 1] == 's') {
            time_str[length - 1] = '\0';  // 去掉 's'
            TOTAL_TIME = atoi(time_str);
            if (TOTAL_TIME <= 0) {
                TOTAL_TIME = DEFAULT_TIME;
            }
        } else {
            TOTAL_TIME = atoi(time_str) * 60;
            if (TOTAL_TIME <= 0) {
                TOTAL_TIME = DEFAULT_TIME;
            }
        }
    }

    // 创建互斥体，确保只有一个实例
    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // 如果已有实例运行，则将参数发送给该实例
        HWND hwnd = FindWindow(WINDOW_CLASS_NAME, "Catime");  // 确保正确传递窗口类名
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

    // 注册窗口类
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;  // 窗口过程函数
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;  // 设置唯一的窗口类名
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // 创建无边框透明窗口，使用 WS_EX_TOOLWINDOW 隐藏任务栏图标
    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,  // 添加 WS_EX_TOOLWINDOW 隐藏任务栏
        WINDOW_CLASS_NAME,  // 使用唯一的窗口类名
        "Catime",  // 窗口标题
        WS_POPUP,
        1, 1,
        90, 65,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // 设置窗口的透明度
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // 托盘图标设置
    nid.cbSize = sizeof(nid);
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = (HICON)LoadImage(NULL, "asset/images/catime.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    nid.hWnd = hwnd;
    nid.uCallbackMessage = WM_TRAYICON;  // 使用自定义消息
    strcpy(nid.szTip, "Catime");
    Shell_NotifyIcon(NIM_ADD, &nid);  // 添加托盘图标

    // 启动计时器
    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
        MessageBox(hwnd, "Failed to set timer!", "Error", MB_OK);
    }

    // 显示窗口
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    CloseHandle(hMutex);
    return (int)msg.wParam;
}

