#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dwmapi.h>
#include "../resource/resource.h"
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
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

#ifndef CSIDL_STARTUP

#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

int default_countdown_time = 0;

extern char CLOCK_TEXT_COLOR[10];

int CLOCK_DEFAULT_START_TIME = 300;
int elapsed_time = 0;
char inputText[256] = {0};
int message_shown = 0;

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

time_t last_config_time = 0;

extern char FONT_FILE_NAME[];
extern char FONT_INTERNAL_NAME[];

INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ExitProgram(HWND hwnd);



RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;

extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        MessageBox(NULL, "COM initialization failed!", "Error", MB_ICONERROR);
        return 1;
    }

    if (!InitializeApplication(hInstance)) {
        MessageBox(NULL, "Application initialization failed!", "Error", MB_ICONERROR);
        return 1;
    }

    HANDLE hMutex = CreateMutex(NULL, TRUE, "CatimeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwndExisting = FindWindow("CatimeWindow", "Catime");
        if (hwndExisting) {
            SendMessage(hwndExisting, WM_CLOSE, 0, 0);
        }
        Sleep(50);
    }

    HWND hwnd = CreateMainWindow(hInstance, nCmdShow);
    if (!hwnd) {
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
        MessageBox(NULL, "Timer Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // 处理启动模式
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

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    CoUninitialize();
    return (int)msg.wParam;
}

// PauseMediaPlayback和ShowToastNotification函数已在media.h和notification.h中声明，
// 实现在media.c和notification.c中，此处不需要重复定义

