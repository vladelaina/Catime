/**
 * @file main.c
 * @brief 应用程序主入口模块实现文件
 * 
 * 本文件实现了应用程序的主要入口点和初始化流程，包括窗口创建、
 * 消息循环处理、启动模式管理等核心功能。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dwmapi.h>
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "../resource/resource.h"
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/tray.h"
#include "../include/tray_menu.h"
#include "../include/timer.h"
#include "../include/window.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/media.h"
#include "../include/notification.h"

// 较旧的Windows SDK所需
#ifndef CSIDL_STARTUP
#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

// 编译器指令
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

/// @name 全局变量
/// @{
int default_countdown_time = 0;          ///< 默认倒计时时间
int CLOCK_DEFAULT_START_TIME = 300;      ///< 默认启动时间(秒)
int elapsed_time = 0;                    ///< 已经过时间
char inputText[256] = {0};              ///< 输入文本缓冲区
int message_shown = 0;                   ///< 消息显示标志
time_t last_config_time = 0;             ///< 最后配置时间
RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];  ///< 最近文件列表
int CLOCK_RECENT_FILES_COUNT = 0;        ///< 最近文件数量
/// @}

/// @name 外部变量声明
/// @{
extern char CLOCK_TEXT_COLOR[10];        ///< 时钟文本颜色
extern char FONT_FILE_NAME[];            ///< 当前字体文件名
extern char FONT_INTERNAL_NAME[];        ///< 字体内部名称
extern char PREVIEW_FONT_NAME[];         ///< 预览字体文件名
extern char PREVIEW_INTERNAL_NAME[];     ///< 预览字体内部名称
extern BOOL IS_PREVIEWING;               ///< 是否正在预览字体
/// @}

/// @name 函数声明
/// @{
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void ExitProgram(HWND hwnd);
/// @}

// 功能原型
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void ExitProgram(HWND hwnd);

// Helper function to handle startup mode
/**
 * @brief 处理应用程序启动模式
 * @param hwnd 主窗口句柄
 * 
 * 根据配置的启动模式(CLOCK_STARTUP_MODE)设置相应的应用程序状态，
 * 包括计时模式、显示状态等。
 */
static void HandleStartupMode(HWND hwnd) {
    if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
        CLOCK_COUNT_UP = TRUE;
        elapsed_time = 0;
    } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
        ShowWindow(hwnd, SW_HIDE);
        KillTimer(hwnd, 1);
        elapsed_time = CLOCK_TOTAL_TIME;
        CLOCK_IS_PAUSED = TRUE;
        message_shown = TRUE;
        countdown_message_shown = TRUE;
        countup_message_shown = TRUE;
        countdown_elapsed_time = 0;
        countup_elapsed_time = 0;
    } else if (strcmp(CLOCK_STARTUP_MODE, "SHOW_TIME") == 0) {
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        CLOCK_LAST_TIME_UPDATE = 0;
    }
}

/**
 * @brief 应用程序主入口点
 * @param hInstance 当前实例句柄
 * @param hPrevInstance 前一个实例句柄(总是NULL)
 * @param lpCmdLine 命令行参数
 * @param nCmdShow 窗口显示方式
 * @return int 程序退出码
 * 
 * 初始化应用程序环境，创建主窗口，并进入消息循环。
 * 处理单实例检查，确保只有一个程序实例在运行。
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化COM
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        MessageBox(NULL, "COM initialization failed!", "Error", MB_ICONERROR);
        return 1;
    }

    // 初始化应用程序
    if (!InitializeApplication(hInstance)) {
        MessageBox(NULL, "Application initialization failed!", "Error", MB_ICONERROR);
        return 1;
    }

    // 处理单实例
    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwndExisting = FindWindow("CatimeWindow", "Catime");
        if (hwndExisting) {
            // 尝试激活已存在的窗口
            ShowWindow(hwndExisting, SW_SHOW);
            SetForegroundWindow(hwndExisting);
        }
        // 释放互斥锁并退出程序
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }
    Sleep(50);

    // 创建主窗口
    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // 设置定时器
    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
        MessageBox(NULL, "Timer Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // 处理启动模式
    HandleStartupMode(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    CloseHandle(hMutex);
    CoUninitialize();
    return (int)msg.wParam;
}
