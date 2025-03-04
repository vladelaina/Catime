/*
pikaへ／|
　　/＼7　　　 ∠＿/
　 /　│　　 ／　／
　│　Z ＿,＜　／　　 /`ヽ
　│ヽ　　 /　　〉
　 Y`　 /　　/
　ｲ●　､　●　　⊂⊃〈　　/
　()　 へ　　　　|　＼〈    代码正在努力拆分中~
　　>ｰ ､_　 ィ　 │ ／／     The code is working hard to split ~
　 / へ　　 /　ﾉ＜| ＼＼
　 ヽ_ﾉ　　(_／　 │／／
　　7|／
　　＞―r￣￣`ｰ―＿6
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include "resource/resource.h"
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "include/language.h"
#include "include/font.h"
#include "include/color.h"
#include "include/tray.h"
#include "include/tray_menu.h"
#include "include/timer.h"
#include "include/window.h"  
#include "include/startup.h" 
#include "include/config.h"  
 
#ifndef CSIDL_STARTUP

#endif

#ifndef CLSID_ShellLink
EXTERN_C const CLSID CLSID_ShellLink;
#endif

#ifndef IID_IShellLinkW
EXTERN_C const IID IID_IShellLinkW;
#endif

COLORREF ShowColorDialog(HWND hwnd); 
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);
void WriteConfig(const char* config_path);
void ShowToastNotification(HWND hwnd, const char* message);

int default_countdown_time = 0;

void PauseMediaPlayback(void);

extern char PREVIEW_COLOR[10];
extern BOOL IS_COLOR_PREVIEWING;
extern char CLOCK_TEXT_COLOR[10];

int CLOCK_DEFAULT_START_TIME = 300;  // Default is 5 minutes (300 seconds)
static int elapsed_time = 0;
char inputText[256] = {0};

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

time_t last_config_time = 0;
int message_shown = 0;

extern char FONT_FILE_NAME[];
extern char FONT_INTERNAL_NAME[];

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void WriteConfigFont(const char* font_file_name);

void ExitProgram(HWND hwnd);

void ListAvailableFonts();

int isValidColor(const char* input);
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
int CALLBACK EnumFontFamExProc(
    const LOGFONT *lpelfe,
    const TEXTMETRIC *lpntme,
    DWORD FontType,
    LPARAM lParam
);

// Keep the global variable declarations
RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
int CLOCK_RECENT_FILES_COUNT = 0;

extern char PREVIEW_FONT_NAME[];
extern char PREVIEW_INTERNAL_NAME[];
extern BOOL IS_PREVIEWING;

// UTF8ToANSI函数已移至config.c中实现

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        MessageBox(NULL, "COM initialization failed!", "Error", MB_ICONERROR);
        return 1;
    }

    SetConsoleOutputCP(936);
    SetConsoleCP(936);
    
    InitializeDefaultLanguage();
    
    // 更新开机自启动快捷方式，确保正确的路径
    UpdateStartupShortcut();
    
    ReadConfig();

    int defaultFontIndex = -1;
    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
        if (strcmp(fontResources[i].fontName, FONT_FILE_NAME) == 0) {
            defaultFontIndex = i;
            break;
        }
    }
    
    if (defaultFontIndex != -1) {
        LoadFontFromResource(hInstance, fontResources[defaultFontIndex].resourceId);
    }

    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;

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

    EnableWindow(hwnd, TRUE);
    SetFocus(hwnd);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    SetBlurBehind(hwnd, FALSE);

    InitTrayIcon(hwnd, hInstance);

    if (SetTimer(hwnd, 1, 1000, NULL) == 0) {
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    FILE *file = fopen(config_path, "r");
    if (file) {
        char line[256];
        while (fgets(line, sizeof(line), file)) {
            if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                sscanf(line, "STARTUP_MODE=%19s", CLOCK_STARTUP_MODE);
                break;
            }
        }
        fclose(file);
    }

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

// GetConfigPath函数已移至config.c中实现

// CreateDefaultConfig函数已移至config.c中实现

// ReadConfig函数已移至config.c中实现

// WriteConfigTimeoutAction函数已移至config.c中实现







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
                EndDialog(hwndDlg, 0);
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
            break;
    }
    return FALSE;
}



void ExitProgram(HWND hwnd) {
    RemoveTrayIcon();

    PostQuitMessage(0);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static char time_text[50];
    UINT uID;
    UINT uMouseMsg;

    switch(msg)
    {
        case WM_CREATE: {
            HWND hwndParent = GetParent(hwnd);
            if (hwndParent != NULL) {
                EnableWindow(hwndParent, TRUE);
            }
            
            // 首先加载窗口设置
            LoadWindowSettings(hwnd);
            
            // 设置点击穿透，但不调整窗口位置
            SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
            
            // 不调用AdjustWindowPosition，让窗口保持在配置文件中的位置
            // AdjustWindowPosition(hwnd, TRUE);
            
            break;
        }

        case WM_LBUTTONDOWN: {
            if (CLOCK_EDIT_MODE) {
                CLOCK_IS_DRAGGING = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&CLOCK_LAST_MOUSE_POS);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
                CLOCK_IS_DRAGGING = FALSE;
                ReleaseCapture();
                // 编辑模式下不强制窗口在屏幕内，允许拖出
                AdjustWindowPosition(hwnd, FALSE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            if (CLOCK_EDIT_MODE) {
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                float old_scale = CLOCK_FONT_SCALE_FACTOR;
                
                // 移除原有的位置计算逻辑，直接使用窗口中心作为缩放基准点
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                int oldWidth = windowRect.right - windowRect.left;
                int oldHeight = windowRect.bottom - windowRect.top;
                
                // 使用窗口中心作为缩放基准
                float relativeX = 0.5f;
                float relativeY = 0.5f;
                
                float scaleFactor = 1.1f;
                if (delta > 0) {
                    CLOCK_FONT_SCALE_FACTOR *= scaleFactor;
                    CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
                } else {
                    CLOCK_FONT_SCALE_FACTOR /= scaleFactor;
                    CLOCK_WINDOW_SCALE = CLOCK_FONT_SCALE_FACTOR;
                }
                
                // 保持缩放范围限制
                if (CLOCK_FONT_SCALE_FACTOR < MIN_SCALE_FACTOR) {
                    CLOCK_FONT_SCALE_FACTOR = MIN_SCALE_FACTOR;
                    CLOCK_WINDOW_SCALE = MIN_SCALE_FACTOR;
                }
                if (CLOCK_FONT_SCALE_FACTOR > MAX_SCALE_FACTOR) {
                    CLOCK_FONT_SCALE_FACTOR = MAX_SCALE_FACTOR;
                    CLOCK_WINDOW_SCALE = MAX_SCALE_FACTOR;
                }
                
                if (old_scale != CLOCK_FONT_SCALE_FACTOR) {
                    // 计算新尺寸
                    int newWidth = (int)(oldWidth * (CLOCK_FONT_SCALE_FACTOR / old_scale));
                    int newHeight = (int)(oldHeight * (CLOCK_FONT_SCALE_FACTOR / old_scale));
                    
                    // 保持窗口中心位置不变
                    int newX = windowRect.left + (oldWidth - newWidth)/2;
                    int newY = windowRect.top + (oldHeight - newHeight)/2;
                    
                    SetWindowPos(hwnd, NULL, 
                        newX, newY,
                        newWidth, newHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
                    
                    // 触发重绘
                    InvalidateRect(hwnd, NULL, FALSE);
                    UpdateWindow(hwnd);
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (CLOCK_EDIT_MODE && CLOCK_IS_DRAGGING) {
                POINT currentPos;
                GetCursorPos(&currentPos);
                
                int deltaX = currentPos.x - CLOCK_LAST_MOUSE_POS.x;
                int deltaY = currentPos.y - CLOCK_LAST_MOUSE_POS.y;
                
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                
                SetWindowPos(hwnd, NULL,
                    windowRect.left + deltaX,
                    windowRect.top + deltaY,
                    windowRect.right - windowRect.left,   
                    windowRect.bottom - windowRect.top,   
                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW   
                );
                
                CLOCK_LAST_MOUSE_POS = currentPos;
                
                UpdateWindow(hwnd);
                
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            SetGraphicsMode(memDC, GM_ADVANCED);
            SetBkMode(memDC, TRANSPARENT);
            SetStretchBltMode(memDC, HALFTONE);
            SetBrushOrgEx(memDC, 0, 0, NULL);

            int remaining_time = CLOCK_TOTAL_TIME - elapsed_time;
            if (elapsed_time >= CLOCK_TOTAL_TIME) {
                if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                    time_text[0] = '\0';
                } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                    strncpy(time_text, CLOCK_TIMEOUT_TEXT, sizeof(time_text) - 1);
                    time_text[sizeof(time_text) - 1] = '\0';
                } else {
                    time_text[0] = '\0';
                }
            } else {
                FormatTime(remaining_time, time_text);
            }

            const char* fontToUse = IS_PREVIEWING ? PREVIEW_FONT_NAME : FONT_FILE_NAME;
            HFONT hFont = CreateFont(
                -CLOCK_BASE_FONT_SIZE * CLOCK_FONT_SCALE_FACTOR,
                0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_TT_PRECIS,
                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,   
                VARIABLE_PITCH | FF_SWISS,
                IS_PREVIEWING ? PREVIEW_INTERNAL_NAME : FONT_INTERNAL_NAME
            );
            HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

            SetTextAlign(memDC, TA_LEFT | TA_TOP);
            SetTextCharacterExtra(memDC, 0);
            SetMapMode(memDC, MM_TEXT);

            DWORD quality = SetICMMode(memDC, ICM_ON);
            SetLayout(memDC, 0);

            int r = 255, g = 255, b = 255;
            const char* colorToUse = IS_COLOR_PREVIEWING ? PREVIEW_COLOR : CLOCK_TEXT_COLOR;
            
            if (strlen(colorToUse) > 0) {
                if (colorToUse[0] == '#') {
                    if (strlen(colorToUse) == 7) {
                        sscanf(colorToUse + 1, "%02x%02x%02x", &r, &g, &b);
                    }
                } else {
                    sscanf(colorToUse, "%d,%d,%d", &r, &g, &b);
                }
            }
            SetTextColor(memDC, RGB(r, g, b));

            if (CLOCK_EDIT_MODE) {
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &rect, hBrush);
                DeleteObject(hBrush);
            } else {
                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &rect, hBrush);
                DeleteObject(hBrush);
            }

            if (strlen(time_text) > 0) {
                SIZE textSize;
                GetTextExtentPoint32(memDC, time_text, strlen(time_text), &textSize);

                if (textSize.cx != (rect.right - rect.left) || 
                    textSize.cy != (rect.bottom - rect.top)) {
                    RECT windowRect;
                    GetWindowRect(hwnd, &windowRect);
                    
                    SetWindowPos(hwnd, NULL,
                        windowRect.left, windowRect.top,
                        textSize.cx + WINDOW_HORIZONTAL_PADDING, 
                        textSize.cy + WINDOW_VERTICAL_PADDING, 
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    GetClientRect(hwnd, &rect);
                }

                
                int x = (rect.right - textSize.cx) / 2;
                int y = (rect.bottom - textSize.cy) / 2;

                SetTextColor(memDC, RGB(r, g, b));
                
                for (int i = 0; i < 8; i++) {
                    TextOutA(memDC, x, y, time_text, strlen(time_text));
                }
            }

            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldFont);
            DeleteObject(hFont);
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (wp == 1) {
                if (CLOCK_SHOW_CURRENT_TIME) {
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }

                if (CLOCK_COUNT_UP) {
                    if (!CLOCK_IS_PAUSED) {
                        countup_elapsed_time++;
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                } else {
                    if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                        countdown_elapsed_time++;
                        if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                            countdown_message_shown = TRUE;
                            
                            switch (CLOCK_TIMEOUT_ACTION) {
                                case TIMEOUT_ACTION_MESSAGE:
                                    ShowToastNotification(hwnd, "Time's up!");
                                    break;
                                case TIMEOUT_ACTION_LOCK:
                                    LockWorkStation();
                                    break;
                                case TIMEOUT_ACTION_SHUTDOWN:
                                    system("shutdown /s /t 0");
                                    break;
                                case TIMEOUT_ACTION_RESTART:
                                    system("shutdown /r /t 0");
                                    break;
                                case TIMEOUT_ACTION_OPEN_FILE: {
                                    if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                        wchar_t wPath[MAX_PATH];
                                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH);
                                        
                                        HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                                        
                                        if ((INT_PTR)result <= 32) {
                                            MessageBoxW(hwnd, 
                                                GetLocalizedString(L"无法打开文件", L"Failed to open file"),
                                                GetLocalizedString(L"错误", L"Error"),
                                                MB_ICONERROR);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
            }
            break;
        }
        case WM_DESTROY: {
            SaveWindowSettings(hwnd);  // Save window settings before closing
            KillTimer(hwnd, 1);
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
        case CLOCK_WM_TRAYICON: {
            uID = (UINT)wp;
            uMouseMsg = (UINT)lp;

            if (uMouseMsg == WM_RBUTTONUP) {
                ShowColorMenu(hwnd);
            }
            else if (uMouseMsg == WM_LBUTTONUP) {
                ShowContextMenu(hwnd);
            }
            break;
        }
        case WM_COMMAND: {
            WORD cmd = LOWORD(wp);
            switch (cmd) {
                case 101: {   
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        KillTimer(hwnd, 1);
                    }
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = total_seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                case CLOCK_IDC_MODIFY_TIME_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        char* token = strtok(inputText, " ");
                        char options[256] = {0};
                        int valid = 1;
                        int count = 0;
                        
                        while (token && count < MAX_TIME_OPTIONS) {
                            int num = atoi(token);
                            if (num <= 0) {
                                valid = 0;
                                break;
                            }
                            
                            if (count > 0) {
                                strcat(options, ",");
                            }
                            strcat(options, token);
                            count++;
                            token = strtok(NULL, " ");
                        }

                        if (valid && count > 0) {
                            WriteConfigTimeOptions(options);
                            ReadConfig();
                            break;
                        } else {
                            MessageBoxW(hwnd,
                                GetLocalizedString(
                                    L"请输入用空格分隔的数字\n"
                                    L"例如: 25 10 5",
                                    L"Enter numbers separated by spaces\n"
                                    L"Example: 25 10 5"),
                                GetLocalizedString(L"无效输入", L"Invalid Input"), 
                                MB_OK);
                        }
                    }
                    break;
                }
                case CLOCK_IDC_MODIFY_DEFAULT_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            ReadConfig();
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                case 200: {   
                    int current_elapsed = elapsed_time;
                    int current_total = CLOCK_TOTAL_TIME;
                    BOOL was_timing = (current_elapsed < current_total);
                    
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                    
                    AppLanguage defaultLanguage;
                    LANGID langId = GetUserDefaultUILanguage();
                    WORD primaryLangId = PRIMARYLANGID(langId);
                    WORD subLangId = SUBLANGID(langId);
                    
                    switch (primaryLangId) {
                        case LANG_CHINESE:
                            defaultLanguage = (subLangId == SUBLANG_CHINESE_SIMPLIFIED) ? 
                                             APP_LANG_CHINESE_SIMP : APP_LANG_CHINESE_TRAD;
                            break;
                        case LANG_SPANISH:
                            defaultLanguage = APP_LANG_SPANISH;
                            break;
                        case LANG_FRENCH:
                            defaultLanguage = APP_LANG_FRENCH;
                            break;
                        case LANG_GERMAN:
                            defaultLanguage = APP_LANG_GERMAN;
                            break;
                        case LANG_RUSSIAN:
                            defaultLanguage = APP_LANG_RUSSIAN;
                            break;
                        case LANG_PORTUGUESE:
                            defaultLanguage = APP_LANG_PORTUGUESE;
                            break;
                        case LANG_JAPANESE:
                            defaultLanguage = APP_LANG_JAPANESE;
                            break;
                        case LANG_KOREAN:
                            defaultLanguage = APP_LANG_KOREAN;
                            break;
                        default:
                            defaultLanguage = APP_LANG_ENGLISH;
                            break;
                    }
                    
                    if (CURRENT_LANGUAGE != defaultLanguage) {
                        CURRENT_LANGUAGE = defaultLanguage;
                    }
                    
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    remove(config_path);   
                    CreateDefaultConfig(config_path);   
                    
                    ReadConfig();   
                    
                    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
                        if (strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0) {  
                            LoadFontFromResource(hInstance, fontResources[i].resourceId);
                            break;
                        }
                    }
                    
                    if (was_timing) {
                        elapsed_time = current_elapsed;
                        CLOCK_TOTAL_TIME = current_total;
                    }
                    
                    CLOCK_WINDOW_SCALE = 1.0f;
                    CLOCK_FONT_SCALE_FACTOR = 1.0f;
                    
                    HDC hdc = GetDC(hwnd);
                    HFONT hFont = CreateFont(
                        -CLOCK_BASE_FONT_SIZE,   
                        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, FONT_INTERNAL_NAME
                    );
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    char time_text[50];
                    FormatTime(CLOCK_TOTAL_TIME, time_text);
                    SIZE textSize;
                    GetTextExtentPoint32(hdc, time_text, strlen(time_text), &textSize);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    
                    float defaultScale = (screenHeight * 0.03f) / 20.0f;
                    CLOCK_WINDOW_SCALE = defaultScale;
                    CLOCK_FONT_SCALE_FACTOR = defaultScale;
                    
                    
                    SetWindowPos(hwnd, NULL, 
                        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        textSize.cx * defaultScale, textSize.cy * defaultScale,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    );
                    
                    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
                    RedrawWindow(hwnd, NULL, NULL, 
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    
                    break;
                }
                case CLOCK_IDM_CHECK_UPDATE: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_UPDATE_GITHUB: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_UPDATE_123PAN: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_123PAN, NULL, NULL, SW_SHOWNORMAL);   
                    break;
                }
                case CLOCK_IDM_UPDATE_LANZOU: {
                    ShellExecuteA(NULL, "open", UPDATE_URL_LANZOU, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_FEEDBACK: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE: {
                    CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE_TRAD: {
                    CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_ENGLISH: {
                    CURRENT_LANGUAGE = APP_LANG_ENGLISH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_SPANISH: {
                    CURRENT_LANGUAGE = APP_LANG_SPANISH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_FRENCH: {
                    CURRENT_LANGUAGE = APP_LANG_FRENCH;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_GERMAN: {
                    CURRENT_LANGUAGE = APP_LANG_GERMAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_RUSSIAN: {
                    CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_PORTUGUESE: {
                    CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_JAPANESE: {
                    CURRENT_LANGUAGE = APP_LANG_JAPANESE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_LANG_KOREAN: {
                    CURRENT_LANGUAGE = APP_LANG_KOREAN;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                default: {
                    int cmd = LOWORD(wp);
                    if (cmd >= 102 && cmd < 102 + time_options_count) {
                        if (CLOCK_SHOW_CURRENT_TIME) {
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            CLOCK_LAST_TIME_UPDATE = 0;
                        }
                        
                        if (CLOCK_COUNT_UP) {
                            CLOCK_COUNT_UP = FALSE;
                        }
                        
                        ShowWindow(hwnd, SW_SHOW);
                        CLOCK_EDIT_MODE = FALSE;
                        WriteConfigEditMode("FALSE");
                        SetClickThrough(hwnd, TRUE);
                        
                        int index = cmd - 102;
                        CLOCK_TOTAL_TIME = time_options[index] * 60;
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        message_shown = 0;
                        countdown_message_shown = FALSE;
                        
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                    
                    if (cmd >= 201 && cmd < 201 + COLOR_OPTIONS_COUNT) {
                        int colorIndex = cmd - 201;
                        const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                        WriteConfigColor(hexColor);
                        goto refresh_window;
                    }

                    if (cmd == 109) {
                        ExitProgram(hwnd);
                        break;
                    }

                    if (cmd == CLOCK_IDM_SHOW_MESSAGE) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                        WriteConfigTimeoutAction("MESSAGE");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_LOCK_SCREEN) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
                        WriteConfigTimeoutAction("LOCK");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_SHUTDOWN) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
                        WriteConfigTimeoutAction("SHUTDOWN");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_RESTART) {
                        CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
                        WriteConfigTimeoutAction("RESTART");
                        break;
                    }
                    else if (cmd == CLOCK_IDM_OPEN_FILE) {
                        char filePath[MAX_PATH] = "";
                        if (OpenFileDialog(hwnd, filePath, MAX_PATH)) {
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            WriteConfigTimeoutAction("OPEN_FILE");
                        }
                        break;
                    }
                    else if (cmd == CLOCK_IDM_BROWSE_FILE) {
                        char filePath[MAX_PATH] = "";
                        if (OpenFileDialog(hwnd, filePath, MAX_PATH)) {
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, filePath, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            WriteConfigTimeoutAction("OPEN_FILE");
                            SaveRecentFile(filePath);
                        }
                        break;
                    }
                    else if (cmd >= CLOCK_IDM_RECENT_FILE_1 && cmd <= CLOCK_IDM_RECENT_FILE_3) {
                        int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                        if (index < CLOCK_RECENT_FILES_COUNT) {
                            wchar_t wPath[MAX_PATH];
                            MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                            
                            if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                                strncpy(CLOCK_TIMEOUT_FILE_PATH, CLOCK_RECENT_FILES[index].path, 
                                        sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                                CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                                
                                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                                
                                WriteConfigTimeoutAction("OPEN_FILE");
                                
                                SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                                
                                ReadConfig();
                            } else {
                                MessageBoxW(hwnd, 
                                    GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                    GetLocalizedString(L"错误", L"Error"),
                                    MB_ICONERROR);
                                
                                memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                                CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                                WriteConfigTimeoutAction("MESSAGE");
                                
                                for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                    CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                                }
                                CLOCK_RECENT_FILES_COUNT--;
                            }
                        }
                        break;
                    }
                }
                case CLOCK_IDC_EDIT_MODE: {
                    
                    if (!IS_PREVIEWING && !IS_COLOR_PREVIEWING) {
                        CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                        WriteConfigEditMode(CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
                        
                        if (CLOCK_EDIT_MODE) {
                            SetBlurBehind(hwnd, TRUE);
                        } else {
                            SetBlurBehind(hwnd, FALSE);
                            SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
                        }
                        
                        SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case CLOCK_IDC_CUSTOMIZE_LEFT: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        char hex_color[10];
                        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                                GetRValue(color), GetGValue(color), GetBValue(color));
                        WriteConfigColor(hex_color);
                        ReadConfig();
                    }
                    break;
                }
                case CLOCK_IDC_FONT_RECMONO: {
                    WriteConfigFont("RecMonoCasual Nerd Font Mono Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DEPARTURE: {
                    WriteConfigFont("DepartureMono Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_TERMINESS: {
                    WriteConfigFont("Terminess Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_GOHUFONT: {  
                    WriteConfigFont("GohuFont uni11 Nerd Font Mono.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_ARBUTUS: {
                    WriteConfigFont("Arbutus Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_BERKSHIRE: {
                    WriteConfigFont("Berkshire Swash Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_CAVEAT: {
                    WriteConfigFont("Caveat Brush Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_CREEPSTER: {
                    WriteConfigFont("Creepster Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DOTO: {  
                    WriteConfigFont("Doto ExtraBold Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FOLDIT: {
                    WriteConfigFont("Foldit SemiBold Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FREDERICKA: {
                    WriteConfigFont("Fredericka the Great Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FRIJOLE: {
                    WriteConfigFont("Frijole Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_GWENDOLYN: {
                    WriteConfigFont("Gwendolyn Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_HANDJET: {
                    WriteConfigFont("Handjet Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_INKNUT: {
                    WriteConfigFont("Inknut Antiqua Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_JACQUARD: {
                    WriteConfigFont("Jacquard 12 Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_JACQUARDA: {
                    WriteConfigFont("Jacquarda Bastarda 9 Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KAVOON: {
                    WriteConfigFont("Kavoon Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KUMAR_ONE_OUTLINE: {
                    WriteConfigFont("Kumar One Outline Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KUMAR_ONE: {
                    WriteConfigFont("Kumar One Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_LAKKI_REDDY: {
                    WriteConfigFont("Lakki Reddy Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_LICORICE: {
                    WriteConfigFont("Licorice Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MA_SHAN_ZHENG: {
                    WriteConfigFont("Ma Shan Zheng Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MOIRAI_ONE: {
                    WriteConfigFont("Moirai One Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MYSTERY_QUEST: {
                    WriteConfigFont("Mystery Quest Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_NOTO_NASTALIQ: {
                    WriteConfigFont("Noto Nastaliq Urdu Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PIEDRA: {
                    WriteConfigFont("Piedra Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PIXELIFY: {
                    WriteConfigFont("Pixelify Sans Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PRESS_START: {
                    WriteConfigFont("Press Start 2P Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_BUBBLES: {
                    WriteConfigFont("Rubik Bubbles Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_BURNED: {
                    WriteConfigFont("Rubik Burned Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_GLITCH_POP: {
                    WriteConfigFont("Rubik Glitch Pop Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_GLITCH: {
                    WriteConfigFont("Rubik Glitch Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_MARKER_HATCH: {
                    WriteConfigFont("Rubik Marker Hatch Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_PUDDLES: {
                    WriteConfigFont("Rubik Puddles Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_VINYL: {
                    WriteConfigFont("Rubik Vinyl Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_WET_PAINT: {
                    WriteConfigFont("Rubik Wet Paint Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUGE_BOOGIE: {
                    WriteConfigFont("Ruge Boogie Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_SEVILLANA: {
                    WriteConfigFont("Sevillana Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_SILKSCREEN: {
                    WriteConfigFont("Silkscreen Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_STICK: {
                    WriteConfigFont("Stick Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_UNDERDOG: {
                    WriteConfigFont("Underdog Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_WALLPOET: {
                    WriteConfigFont("Wallpoet Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_YESTERYEAR: {
                    WriteConfigFont("Yesteryear Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_ZCOOL_KUAILE: {
                    WriteConfigFont("ZCOOL KuaiLe Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        
                        ShowWindow(hwnd, SW_SHOW);  
                        
                        CLOCK_COUNT_UP = FALSE;
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, 1000, NULL);   
                    } else {
                        KillTimer(hwnd, 1);   
                        elapsed_time = CLOCK_TOTAL_TIME;   
                        message_shown = 1;   
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_24HOUR_FORMAT: {  
                    CLOCK_USE_24HOUR = !CLOCK_USE_24HOUR;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        FILE *fp = fopen(config_path, "r");
                        if (fp) {
                            char line[256];
                            while (fgets(line, sizeof(line), fp)) {
                                if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                                    sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                                    break;
                                }
                            }
                            fclose(fp);
                            
                            WriteConfig(config_path);
                            
                            WriteConfigStartupMode(currentStartupMode);
                        } else {
                            WriteConfig(config_path);
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_SHOW_SECONDS: {  
                    CLOCK_SHOW_SECONDS = !CLOCK_SHOW_SECONDS;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        FILE *fp = fopen(config_path, "r");
                        if (fp) {
                            char line[256];
                            while (fgets(line, sizeof(line), fp)) {
                                if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                                    sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                                    break;
                                }
                            }
                            fclose(fp);
                            
                            WriteConfig(config_path);
                            
                            WriteConfigStartupMode(currentStartupMode);
                        } else {
                            WriteConfig(config_path);
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_FEEDBACK_GITHUB: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL_GITHUB, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_FEEDBACK_BILIBILI: {
                    ShellExecuteA(NULL, "open", FEEDBACK_URL_BILIBILI, NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
                case CLOCK_IDM_RECENT_FILE_1:
                case CLOCK_IDM_RECENT_FILE_2:
                case CLOCK_IDM_RECENT_FILE_3: {
                    int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                    if (index < CLOCK_RECENT_FILES_COUNT) {
                        wchar_t wPath[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                        
                        if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                            strncpy(CLOCK_TIMEOUT_FILE_PATH, CLOCK_RECENT_FILES[index].path, 
                                    sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
                            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';
                            
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
                            
                            WriteConfigTimeoutAction("OPEN_FILE");
                            
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                            
                            ReadConfig();
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                            
                            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                            WriteConfigTimeoutAction("MESSAGE");
                            
                            for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                            }
                            CLOCK_RECENT_FILES_COUNT--;
                        }
                    }
                    break;
                }
                case CLOCK_IDC_TIMEOUT_BROWSE: {
                    OPENFILENAMEW ofn;
                    wchar_t szFile[MAX_PATH] = L"";
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, 
                                           CLOCK_TIMEOUT_FILE_PATH, 
                                           sizeof(CLOCK_TIMEOUT_FILE_PATH), 
                                           NULL, NULL);
                        
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        WriteConfigTimeoutAction("OPEN_FILE");
                        SaveRecentFile(CLOCK_TIMEOUT_FILE_PATH);
                    }
                    break;
                }
                case CLOCK_IDM_COUNT_UP: {
                    CLOCK_COUNT_UP = !CLOCK_COUNT_UP;
                    if (CLOCK_COUNT_UP) {
                        ShowWindow(hwnd, SW_SHOW);
                        
                        elapsed_time = 0;
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_START: {
                    if (!CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = TRUE;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_IS_PAUSED = FALSE;
                        
                        
                        elapsed_time = 0;
                        message_shown = FALSE;
                        countdown_message_shown = FALSE;
                        
                        countup_elapsed_time = 0;
                        countup_message_shown = FALSE;
                        
                        
                        ShowWindow(hwnd, SW_SHOW);
                        
                        
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                    } else {
                        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
                        if (CLOCK_IS_PAUSED) {
                            KillTimer(hwnd, 1);
                        } else {
                            SetTimer(hwnd, 1, 1000, NULL);
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_RESET: {
                    countup_elapsed_time = 0;  
                    CLOCK_IS_PAUSED = FALSE;
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 1000, NULL);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDC_SET_COUNTDOWN_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            
                            
                            CLOCK_DEFAULT_START_TIME = total_seconds;
                            
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                case CLOCK_IDC_START_SHOW_TIME: {
                    WriteConfigStartupMode("SHOW_TIME");
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_CHECKED);
                    break;
                }
                case CLOCK_IDC_START_COUNT_UP: {
                    WriteConfigStartupMode("COUNT_UP");
                    break;
                }
                case CLOCK_IDC_START_NO_DISPLAY: {
                    WriteConfigStartupMode("NO_DISPLAY");
                    
                    ShowWindow(hwnd, SW_HIDE);
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_CHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                    break;
                }
                case CLOCK_IDC_AUTO_START: {
                    BOOL isEnabled = IsAutoStartEnabled();
                    if (isEnabled) {
                        if (RemoveShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
                        }
                    } else {
                        if (CreateShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
                        }
                    }
                    break;
                }
                case CLOCK_IDC_COLOR_VALUE: {
                    DialogBox(GetModuleHandle(NULL), 
                             MAKEINTRESOURCE(CLOCK_IDD_DIALOG1), 
                             hwnd, 
                             (DLGPROC)ColorDlgProc);
                    break;
                }
                case CLOCK_IDC_COLOR_PANEL: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                case CLOCK_IDM_COUNTDOWN_START_PAUSE: {
                    if (!IsWindowVisible(hwnd)) {
                        
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    
                    if (CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = FALSE;
                        countdown_elapsed_time = 0;
                        CLOCK_IS_PAUSED = FALSE;
                    } else if (!CLOCK_SHOW_CURRENT_TIME) {
                        
                        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
                    } else {
                        
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        countdown_elapsed_time = 0;
                        CLOCK_IS_PAUSED = FALSE;
                    }
                    
                    
                    KillTimer(hwnd, 1); 
                    SetTimer(hwnd, 1, 1000, NULL);
                    
                    
                    elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNTDOWN_RESET: {
                    
                    ShowWindow(hwnd, SW_SHOW);
                    
                    
                    countdown_elapsed_time = 0;  
                    CLOCK_IS_PAUSED = FALSE;
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 1000, NULL);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDC_FONT_PROFONT: {
                    WriteConfigFont("ProFont IIx Nerd Font.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DADDYTIME: {
                    // 写入新字体
                    WriteConfigFont("DaddyTimeMono Nerd Font Propo Essence.ttf");
                    
                    goto refresh_window;
                }
            }
            break;

refresh_window:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        case WM_WINDOWPOSCHANGED: {
            if (CLOCK_EDIT_MODE) {
                SaveWindowSettings(hwnd);
            }
            break;
        }
        case WM_RBUTTONUP: {
            if (CLOCK_EDIT_MODE) {
                CLOCK_EDIT_MODE = FALSE;
                WriteConfigEditMode("FALSE");
                
                SetBlurBehind(hwnd, FALSE);
                SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
                
                SetClickThrough(hwnd, !CLOCK_EDIT_MODE);
                
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            break;
        }
        case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
            if (lpmis->CtlType == ODT_MENU) {
                lpmis->itemHeight = 25;
                lpmis->itemWidth = 100;
                return TRUE;
            }
            return FALSE;
        }
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
            if (lpdis->CtlType == ODT_MENU) {
                int colorIndex = lpdis->itemID - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                    int r, g, b;
                    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
                    
                    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                    
                    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
                    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
                    
                    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                             lpdis->rcItem.right, lpdis->rcItem.bottom);
                    
                    SelectObject(lpdis->hDC, oldPen);
                    SelectObject(lpdis->hDC, oldBrush);
                    DeleteObject(hPen);
                    DeleteObject(hBrush);
                    
                    if (lpdis->itemState & ODS_SELECTED) {
                        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
                    }
                    
                    return TRUE;
                }
            }
            return FALSE;
        }
        case WM_MENUSELECT: {
            UINT menuItem = LOWORD(wp);
            UINT flags = HIWORD(wp);
            HMENU hMenu = (HMENU)lp;

            if (!(flags & MF_POPUP) && hMenu != NULL) {
                int colorIndex = menuItem - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(PREVIEW_COLOR, COLOR_OPTIONS[colorIndex].hexColor, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }

                for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
                    if (fontResources[i].menuId == menuItem) {
                        strncpy(PREVIEW_FONT_NAME, fontResources[i].fontName, 99);
                        PREVIEW_FONT_NAME[99] = '\0';
                        
                        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, 99);
                        PREVIEW_INTERNAL_NAME[99] = '\0';
                        char* dot = strrchr(PREVIEW_INTERNAL_NAME, '.');
                        if (dot) *dot = '\0';
                        
                        LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
                                     fontResources[i].fontName);
                        
                        IS_PREVIEWING = TRUE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
                
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (flags & MF_POPUP) {
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        case WM_EXITMENULOOP: {
            if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                IS_PREVIEWING = FALSE;
                IS_COLOR_PREVIEWING = FALSE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case WM_RBUTTONDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                // Toggle edit mode
                CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                
                if (CLOCK_EDIT_MODE) {
                    // Entering edit mode
                    SetClickThrough(hwnd, FALSE);
                } else {
                    // Exiting edit mode
                    SetClickThrough(hwnd, TRUE);
                    SaveWindowSettings(hwnd);  // Save settings when exiting edit mode
                    WriteConfigColor(CLOCK_TEXT_COLOR);  // Save the current color to config
                }
                
                InvalidateRect(hwnd, NULL, TRUE);
                WriteConfigEditMode(CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            SaveWindowSettings(hwnd);  // Save window settings before closing
            DestroyWindow(hwnd);  // Close the window
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

void PauseMediaPlayback(void) {
     
    keybd_event(VK_MEDIA_STOP, 0, 0, 0);
    Sleep(50);   
    keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);   

     
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);   
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);   

     
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(100);   
}

void ShowToastNotification(HWND hwnd, const char* message) {
    const wchar_t* timeUpMsg = GetLocalizedString(L"时间到了!", L"Time's up!");
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, timeUpMsg, -1, NULL, 0, NULL, NULL);
    char* utf8Msg = (char*)malloc(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, timeUpMsg, -1, utf8Msg, size_needed, NULL, NULL);
    
    ShowTrayNotification(hwnd, utf8Msg);
    free(utf8Msg);
}

void WriteConfig(const char* config_path) {
    FILE* file = fopen(config_path, "w");
    if (!file) return;
    
    fprintf(file, "CLOCK_TEXT_COLOR=%s\n", CLOCK_TEXT_COLOR);
    fprintf(file, "CLOCK_BASE_FONT_SIZE=%d\n", CLOCK_BASE_FONT_SIZE);
    fprintf(file, "FONT_FILE_NAME=%s\n", FONT_FILE_NAME);
    fprintf(file, "CLOCK_DEFAULT_START_TIME=%d\n", CLOCK_DEFAULT_START_TIME);
    fprintf(file, "CLOCK_WINDOW_POS_X=%d\n", CLOCK_WINDOW_POS_X);
    fprintf(file, "CLOCK_WINDOW_POS_Y=%d\n", CLOCK_WINDOW_POS_Y);
    fprintf(file, "CLOCK_EDIT_MODE=%s\n", CLOCK_EDIT_MODE ? "TRUE" : "FALSE");
    fprintf(file, "WINDOW_SCALE=%.2f\n", CLOCK_WINDOW_SCALE);
    fprintf(file, "CLOCK_USE_24HOUR=%s\n", CLOCK_USE_24HOUR ? "TRUE" : "FALSE");
    fprintf(file, "CLOCK_SHOW_SECONDS=%s\n", CLOCK_SHOW_SECONDS ? "TRUE" : "FALSE");
    
    fprintf(file, "CLOCK_TIME_OPTIONS=");
    for (int i = 0; i < time_options_count; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%d", time_options[i]);
    }
    fprintf(file, "\n");
    
    fprintf(file, "CLOCK_TIMEOUT_TEXT=%s\n", CLOCK_TIMEOUT_TEXT);
    
    if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE && strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
        fprintf(file, "CLOCK_TIMEOUT_ACTION=OPEN_FILE\n");
        fprintf(file, "CLOCK_TIMEOUT_FILE=%s\n", CLOCK_TIMEOUT_FILE_PATH);
    } else {
        switch (CLOCK_TIMEOUT_ACTION) {
            case TIMEOUT_ACTION_MESSAGE:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=MESSAGE\n");
                break;
            case TIMEOUT_ACTION_LOCK:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=LOCK\n");
                break;
            case TIMEOUT_ACTION_SHUTDOWN:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=SHUTDOWN\n");
                break;
            case TIMEOUT_ACTION_RESTART:
                fprintf(file, "CLOCK_TIMEOUT_ACTION=RESTART\n");
                break;
        }
    }
    
    for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; i++) {
        fprintf(file, "CLOCK_RECENT_FILE=%s\n", CLOCK_RECENT_FILES[i].path);
    }
    
    fprintf(file, "COLOR_OPTIONS=");
    for (size_t i = 0; i < COLOR_OPTIONS_COUNT; i++) {
        if (i > 0) fprintf(file, ",");
        fprintf(file, "%s", COLOR_OPTIONS[i].hexColor);
    }
    fprintf(file, "\n");
    
    fclose(file);
}

