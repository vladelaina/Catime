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
#include <commctrl.h>
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
#include "../include/async_update_checker.h"
#include "../include/log.h"
#include "../include/dialog_language.h"
#include "../include/shortcut_checker.h"

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
#pragma comment(lib, "dbghelp.lib") // 用于异常处理功能
#pragma comment(lib, "comctl32.lib") // 用于Common Controls

// 来自log.c的函数声明
extern void CleanupLogSystem(void);

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
char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH] = "";
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
    LOG_INFO("设置启动模式: %s", CLOCK_STARTUP_MODE);
    
    if (strcmp(CLOCK_STARTUP_MODE, "COUNT_UP") == 0) {
        LOG_INFO("设置为正计时模式");
        CLOCK_COUNT_UP = TRUE;
        elapsed_time = 0;
    } else if (strcmp(CLOCK_STARTUP_MODE, "NO_DISPLAY") == 0) {
        LOG_INFO("设置为隐藏模式，窗口将被隐藏");
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
        LOG_INFO("设置为显示当前时间模式");
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        CLOCK_LAST_TIME_UPDATE = 0;
    } else {
        LOG_INFO("使用默认倒计时模式");
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
    // 初始化Common Controls
    InitCommonControls();
    
    // 初始化日志系统
    if (!InitializeLogSystem()) {
        // 如果日志系统初始化失败，仍然继续运行，但不记录日志
        MessageBox(NULL, "日志系统初始化失败，程序将继续运行但不记录日志。", "警告", MB_ICONWARNING);
    }

    // 设置异常处理器
    SetupExceptionHandler();

    LOG_INFO("Catime 正在启动...");
        // 初始化COM
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr)) {
            LOG_ERROR("COM 初始化失败，错误码: 0x%08X", hr);
            MessageBox(NULL, "COM initialization failed!", "Error", MB_ICONERROR);
            return 1;
        }
        LOG_INFO("COM 初始化成功");

        // 初始化应用程序
        LOG_INFO("开始初始化应用程序...");
        if (!InitializeApplication(hInstance)) {
            LOG_ERROR("应用程序初始化失败");
            MessageBox(NULL, "Application initialization failed!", "Error", MB_ICONERROR);
            return 1;
        }
        LOG_INFO("应用程序初始化成功");

        // 检查并创建桌面快捷方式（如有必要）
        LOG_INFO("检查桌面快捷方式...");
        char exe_path[MAX_PATH];
        GetModuleFileNameA(NULL, exe_path, MAX_PATH);
        LOG_INFO("当前程序路径: %s", exe_path);
        
        // 设置日志级别为DEBUG以显示详细信息
        WriteLog(LOG_LEVEL_DEBUG, "开始检测快捷方式，检查路径: %s", exe_path);
        
        // 检查路径中是否包含WinGet标识
        if (strstr(exe_path, "WinGet") != NULL) {
            WriteLog(LOG_LEVEL_DEBUG, "路径中包含WinGet关键字");
        }
        
        // 附加测试：直接测试文件是否存在
        char desktop_path[MAX_PATH];
        char shortcut_path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktop_path))) {
            sprintf(shortcut_path, "%s\\Catime.lnk", desktop_path);
            WriteLog(LOG_LEVEL_DEBUG, "检查桌面快捷方式是否存在: %s", shortcut_path);
            if (GetFileAttributesA(shortcut_path) == INVALID_FILE_ATTRIBUTES) {
                WriteLog(LOG_LEVEL_DEBUG, "桌面快捷方式不存在，需要创建");
            } else {
                WriteLog(LOG_LEVEL_DEBUG, "桌面快捷方式已存在");
            }
        }
        
        int shortcut_result = CheckAndCreateShortcut();
        if (shortcut_result == 0) {
            LOG_INFO("桌面快捷方式检查完成");
        } else {
            LOG_WARNING("桌面快捷方式创建失败，错误码: %d", shortcut_result);
        }

        // 初始化对话框多语言支持
        LOG_INFO("开始初始化对话框多语言支持...");
        if (!InitDialogLanguageSupport()) {
            LOG_WARNING("对话框多语言支持初始化失败，但程序将继续运行");
        }
        LOG_INFO("对话框多语言支持初始化成功");

        // 处理单实例
        LOG_INFO("检查是否有其他实例正在运行...");
        HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
        DWORD mutexError = GetLastError();
        
        if (mutexError == ERROR_ALREADY_EXISTS) {
            LOG_INFO("检测到已有实例正在运行，尝试关闭该实例");
            HWND hwndExisting = FindWindow("CatimeWindow", "Catime");
            if (hwndExisting) {
                // 关闭已存在的窗口实例
                LOG_INFO("向已有实例发送关闭消息");
                SendMessage(hwndExisting, WM_CLOSE, 0, 0);
                // 等待旧实例关闭
                Sleep(200);
            } else {
                LOG_WARNING("找不到已有实例的窗口句柄，但互斥锁已存在");
            }
            // 释放旧互斥锁
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            
            // 创建新的互斥锁
            LOG_INFO("创建新的互斥锁");
            hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                LOG_WARNING("创建新互斥锁后仍然有冲突，可能有竞争条件");
            }
        }
        Sleep(50);

        // 创建主窗口
        LOG_INFO("开始创建主窗口...");
        HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
        if (!hwnd) {
            LOG_ERROR("主窗口创建失败");
            MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }
        LOG_INFO("主窗口创建成功，句柄: 0x%p", hwnd);

        // 设置定时器
        LOG_INFO("设置主定时器...");
        if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
            DWORD timerError = GetLastError();
            LOG_ERROR("定时器创建失败，错误码: %lu", timerError);
            MessageBox(NULL, "Timer Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
            return 0;
        }
        LOG_INFO("定时器设置成功");

        // 处理启动模式
        LOG_INFO("处理启动模式: %s", CLOCK_STARTUP_MODE);
        HandleStartupMode(hwnd);
        
        // 已移除自动检查更新代码

        // 消息循环
        LOG_INFO("进入主消息循环");
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 清理资源
        LOG_INFO("程序准备退出，开始清理资源");
        
        // 清理更新检查线程资源
        LOG_INFO("准备清理更新检查线程资源");
        CleanupUpdateThread();
        
        CloseHandle(hMutex);
        CoUninitialize();
        
        // 关闭日志系统
        CleanupLogSystem();
        
        return (int)msg.wParam;
    // 如果执行到这里，说明程序正常退出
}
