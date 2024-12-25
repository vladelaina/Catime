#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  // 添加文件操作相关的头文件
#include <io.h>     // 添加文件操作相关的头文件
#include <sys/stat.h> // 添加文件状态相关的头文件

// 全局变量
char CLOCK_TEXT_COLOR[10];  // 时钟文本颜色
int CLOCK_TEXT_LAYER_COUNT;  // 时钟文本层数
int CLOCK_BASE_WINDOW_WIDTH;  // 基础窗口宽度
int CLOCK_BASE_WINDOW_HEIGHT;  // 基础窗口高度
float CLOCK_WINDOW_SCALE;  // 窗口尺寸缩放因子
int CLOCK_BASE_FONT_SIZE;  // 基准字体大小
float CLOCK_FONT_SCALE_FACTOR;  // 字体缩放因子
int CLOCK_DEFAULT_START_TIME;  // 默认启动时的倒计时时长
int CLOCK_WINDOW_POS_X;  // 窗口 X 坐标
int CLOCK_WINDOW_POS_Y;  // 窗口 Y 坐标
int CLOCK_IDC_EDIT;  // 编辑框控件ID
int CLOCK_IDC_BUTTON_OK;  // 确定按钮控件ID
int CLOCK_IDD_DIALOG1;  // 对话框ID
int CLOCK_ID_TRAY_APP_ICON;  // 托盘图标ID

// 自定义消息 ID
#define CLOCK_WM_TRAYICON        (WM_USER + 2)  // 自定义消息 ID

// 定义最大时间选项数量
#define MAX_TIME_OPTIONS 10  // 定义最大时间选项数量
int time_options[MAX_TIME_OPTIONS];  // 时间选项数组
int time_options_count = 0;  // 初始化时间选项数量

// 全局变量用于保存输入内容
char inputText[256] = {0};  // 设置全局变量
static int elapsed_time = 0;  // 已经过的时间，全局变量
static int CLOCK_TOTAL_TIME = 0;  // 全局倒计时总时间
NOTIFYICONDATA nid;  // 托盘图标数据

// 函数声明
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void ReadConfig();

// 读取配置文件的函数
void ReadConfig() {
    const char *config_path = "./asset/config.txt"; // 配置文件路径
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "无法打开配置文件: %s\n", config_path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // 跳过空行和注释行
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        // 读取时间选项
        if (sscanf(line, "CLOCK_TIME_OPTIONS=%[^\n]", line) == 1) {
            char *token = strtok(line, ",");
            while (token != NULL && time_options_count < MAX_TIME_OPTIONS) {
                time_options[time_options_count++] = atoi(token);  // 将字符串转换为整数并存储
                token = strtok(NULL, ",");
            }
            continue;
        }
        if (sscanf(line, "CLOCK_TEXT_COLOR=%s", CLOCK_TEXT_COLOR) == 1) continue;
        if (sscanf(line, "CLOCK_TEXT_LAYER_COUNT=%d", &CLOCK_TEXT_LAYER_COUNT) == 1) continue;
        if (sscanf(line, "CLOCK_BASE_WINDOW_WIDTH=%d", &CLOCK_BASE_WINDOW_WIDTH) == 1) continue;
        if (sscanf(line, "CLOCK_BASE_WINDOW_HEIGHT=%d", &CLOCK_BASE_WINDOW_HEIGHT) == 1) continue;
        if (sscanf(line, "CLOCK_WINDOW_SCALE=%f", &CLOCK_WINDOW_SCALE) == 1) continue;
        if (sscanf(line, "CLOCK_BASE_FONT_SIZE=%d", &CLOCK_BASE_FONT_SIZE) == 1) continue;
        if (sscanf(line, "CLOCK_FONT_SCALE_FACTOR=%f", &CLOCK_FONT_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "CLOCK_DEFAULT_START_TIME=%d", &CLOCK_DEFAULT_START_TIME) == 1) continue;
        if (sscanf(line, "CLOCK_WINDOW_POS_X=%d", &CLOCK_WINDOW_POS_X) == 1) continue;
        if (sscanf(line, "CLOCK_WINDOW_POS_Y=%d", &CLOCK_WINDOW_POS_Y) == 1) continue;
        if (sscanf(line, "CLOCK_IDC_EDIT=%d", &CLOCK_IDC_EDIT) == 1) continue;
        if (sscanf(line, "CLOCK_IDC_BUTTON_OK=%d", &CLOCK_IDC_BUTTON_OK) == 1) continue;
        if (sscanf(line, "CLOCK_IDD_DIALOG1=%d", &CLOCK_IDD_DIALOG1) == 1) continue;
        if (sscanf(line, "CLOCK_ID_TRAY_APP_ICON=%d", &CLOCK_ID_TRAY_APP_ICON) == 1) continue;
    }

    fclose(file);
}

// 对话框过程函数
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));
                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
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

    // 添加选项：根据时间选项动态生成菜单项
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
            int remaining_time = CLOCK_TOTAL_TIME - elapsed_time;

            if (elapsed_time >= CLOCK_TOTAL_TIME) {
                sprintf(time_text, "Time's up!");
            } else {
                FormatTime(remaining_time, time_text);
            }

            // 创建字体，使用等比例缩放
            HFONT hFont = CreateFont(
                -CLOCK_BASE_FONT_SIZE * CLOCK_FONT_SCALE_FACTOR,                 
                0,                          
                0,                          
                0,                          
                FW_BOLD,                    
                FALSE,                      
                FALSE,                      
                FALSE,                      
                DEFAULT_CHARSET,            
                OUT_TT_PRECIS,             
                CLIP_DEFAULT_PRECIS,        
                ANTIALIASED_QUALITY,        
                FF_DONTCARE | DEFAULT_PITCH,
                "Arial"                     
            );
            
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            // 解析颜色字符串
            int r, g, b;
            sscanf(CLOCK_TEXT_COLOR + 1, "%02x%02x%02x", &r, &g, &b); 
            
            COLORREF color = RGB(r, g, b);
            SetTextColor(hdc, color);
            SetBkMode(hdc, TRANSPARENT);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            
            // 计算文本位置以居中显示
            SIZE textSize;
            GetTextExtentPoint32(hdc, time_text, strlen(time_text), &textSize);
            int x = (CLOCK_BASE_WINDOW_WIDTH - textSize.cx) / 2;
            int y = (CLOCK_BASE_WINDOW_HEIGHT - textSize.cy) / 2;

            // 绘制多层文本
            for (int i = 0; i < CLOCK_TEXT_LAYER_COUNT; i++) {
                TextOutA(hdc, x, y, time_text, strlen(time_text));
            }
            
            // 清理资源
            SelectObject(hdc, hOldFont);
            DeleteObject(hFont);
            
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (elapsed_time < CLOCK_TOTAL_TIME) {
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
        case CLOCK_WM_TRAYICON: {
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
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);  

                    int input_time = 0;

                    if (inputText[0] == '\0') {
                        CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;  // 使用定义的默认启动时间
                    }
                    else if (inputText[strlen(inputText) - 1] == 's') {
                        inputText[strlen(inputText) - 1] = '\0';
                        input_time = atoi(inputText);
                        CLOCK_TOTAL_TIME = input_time;
                    }
                    else {
                        input_time = atoi(inputText);
                        if (input_time == 0) {
                            CLOCK_TOTAL_TIME = 0;
                        } else {
                            CLOCK_TOTAL_TIME = input_time * 60;
                        }
                    }
                    elapsed_time = 0;  
                    break;
                // 根据菜单项的索引设置 CLOCK_TOTAL_TIME
                default:
                    if (LOWORD(wp) >= 102 && LOWORD(wp) < 102 + time_options_count) {
                        int index = LOWORD(wp) - 102;  // 计算选中的菜单项索引
                        CLOCK_TOTAL_TIME = time_options[index] * 60;  // 将分钟转换为秒
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
    ReadConfig();  // 读取配置文件

    // 设置默认倒计时时长
    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;

    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwnd = FindWindow("CatimeWindow", "Catime");
        if (hwnd) {
            COPYDATASTRUCT cds;
            cds.dwData = 1;
            char time_str[10];
            sprintf(time_str, "%d", CLOCK_TOTAL_TIME);
            cds.lpData = time_str;
            cds.cbData = strlen(time_str) + 1;
            SendMessage(hwnd, WM_COPYDATA, (WPARAM)hwnd, (LPARAM)&cds);
        }
        return 0;
    }

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProcedure;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CatimeWindow";
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "CatimeWindow",
        "Catime",
        WS_POPUP,
        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
        CLOCK_BASE_WINDOW_WIDTH, CLOCK_BASE_WINDOW_HEIGHT,
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
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.hIcon = (HICON)LoadImage(NULL, "asset/images/catime.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    nid.hWnd = hwnd;
    nid.uCallbackMessage = CLOCK_WM_TRAYICON;
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
