/**
 * @file window_procedure.c
 * @brief 窗口消息处理过程实现
 * 
 * 本文件实现了应用程序主窗口的消息处理回调函数，
 * 处理窗口的所有消息事件。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
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
#include "../include/window_events.h"
#include "../include/drag_scale.h"
#include "../include/drawing.h"
#include "../include/timer_events.h"
#include "../include/tray_events.h"
#include "../include/dialog_procedure.h"
#include "../include/pomodoro.h"

// 从main.c引入的变量
extern char inputText[256];
extern int elapsed_time;
extern int message_shown;

// 从main.c引入的函数声明
extern void ShowToastNotification(HWND hwnd, const char* message);
extern void PauseMediaPlayback(void);

// 在文件开头添加
typedef struct {
    const wchar_t* title;
    const wchar_t* prompt;
    const wchar_t* defaultText;
    wchar_t* result;
    size_t maxLen;
} INPUTBOX_PARAMS;

/**
 * @brief 输入对话框回调函数
 * @param hwndDlg 对话框句柄
 * @param uMsg 消息
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return 处理结果
 */
INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            // 获取传递的参数
            INPUTBOX_PARAMS* params = (INPUTBOX_PARAMS*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            // 设置对话框标题
            SetWindowTextW(hwndDlg, params->title);
            
            // 设置提示消息
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            
            // 设置默认文本
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            
            // 选中文本
            SendDlgItemMessageW(hwndDlg, IDC_EDIT_INPUT, EM_SETSEL, 0, -1);
            
            // 设置焦点
            SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_INPUT));
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    // 获取输入文本
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                }
                
                case IDCANCEL:
                    // 取消操作
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief 显示输入对话框
 * @param hwndParent 父窗口句柄
 * @param title 对话框标题
 * @param prompt 提示信息
 * @param defaultText 默认文本
 * @param result 结果缓冲区
 * @param maxLen 缓冲区最大长度
 * @return 成功返回TRUE，取消返回FALSE
 */
BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    // 准备传递给对话框的参数
    INPUTBOX_PARAMS params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
    // 显示模态对话框
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}

void ExitProgram(HWND hwnd) {
    RemoveTrayIcon();

    PostQuitMessage(0);
}

/**
 * @brief 主窗口消息处理回调函数
 * @param hwnd 窗口句柄
 * @param msg 消息类型
 * @param wp 消息参数(具体含义取决于消息类型)
 * @param lp 消息参数(具体含义取决于消息类型)
 * @return LRESULT 消息处理结果
 * 
 * 处理主窗口的所有消息事件，包括：
 * - 窗口创建/销毁
 * - 鼠标事件(拖动、滚轮缩放)
 * - 定时器事件
 * - 系统托盘交互
 * - 绘图事件
 * - 菜单命令处理
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static char time_text[50];
    UINT uID;
    UINT uMouseMsg;

    switch(msg)
    {
        case WM_CREATE: {
            HandleWindowCreate(hwnd);
            break;
        }

        case WM_LBUTTONDOWN: {
            StartDragWindow(hwnd);
            break;
        }

        case WM_LBUTTONUP: {
            EndDragWindow(hwnd);
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            HandleScaleWindow(hwnd, delta);
            break;
        }

        case WM_MOUSEMOVE: {
            if (HandleDragWindow(hwnd)) {
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            HandleWindowPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (HandleTimerEvent(hwnd, wp)) {
                break;
            }
            break;
        }
        case WM_DESTROY: {
            HandleWindowDestroy(hwnd);
            return 0;
        }
        case CLOCK_WM_TRAYICON: {
            HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
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

                        // 检查是否只有空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
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
                // 处理便捷时间选项 (102-102+MAX_TIME_OPTIONS)
                case 102: case 103: case 104: case 105: case 106:
                case 107: case 108: {
                    int index = cmd - 102;
                    if (index >= 0 && index < time_options_count) {
                        int minutes = time_options[index];
                        if (minutes > 0) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = minutes * 60; // 转换为秒
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
                        }
                    }
                    break;
                }
                // 处理退出选项
                case 109: {
                    ExitProgram(hwnd);
                    break;
                }
                case CLOCK_IDC_MODIFY_TIME_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_SHORTCUT_DIALOG), NULL, DlgProc);

                        // 检查输入是否为空或只包含空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        
                        // 如果输入为空或只包含空格，直接退出循环
                        if (inputText[0] == '\0' || isAllSpaces) {
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
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_STARTUP_DIALOG), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        // 检查是否只有空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
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
                case CLOCK_IDM_TIMER_PAUSE_RESUME: {
                    PauseResumeTimer(hwnd);
                    break;
                }
                case CLOCK_IDM_TIMER_RESTART: {
                    RestartTimer(hwnd);
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
                case CLOCK_IDM_ABOUT:
                    ShowAboutDialog(hwnd);
                    return 0;
                case CLOCK_IDM_TOPMOST: {
                    // Toggle the topmost state in configuration
                    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
                    
                    // If in edit mode, just update the stored state but don't apply it yet
                    if (CLOCK_EDIT_MODE) {
                        // Update the configuration and saved state only
                        PREVIOUS_TOPMOST_STATE = newTopmost;
                        CLOCK_WINDOW_TOPMOST = newTopmost;
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                    } else {
                        // Not in edit mode, apply it immediately
                        SetWindowTopmost(hwnd, newTopmost);
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                    }
                    break;
                }
                case CLOCK_IDM_COUNTDOWN_RESET: {
                    // 现有的重置代码
                    // ...
                    
                    // 添加窗口置顶重置
                    HandleWindowReset(hwnd);
                    return 0;
                    
                    // ... 其他命令处理 ...
                }
                case CLOCK_IDC_EDIT_MODE: {
                    if (CLOCK_EDIT_MODE) {
                        EndEditMode(hwnd);
                    } else {
                        StartEditMode(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
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
                case CLOCK_IDC_FONT_PROFONT: {
                    WriteConfigFont("ProFont IIx Nerd Font.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DADDYTIME: {
                    WriteConfigFont("DaddyTimeMono Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        
                        ShowWindow(hwnd, SW_SHOW);  
                        
                        CLOCK_COUNT_UP = FALSE;
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0; // 确保总时间被重置
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
                        countdown_elapsed_time = 0;
                        CLOCK_IS_PAUSED = FALSE;
                        
                        // 确保每次切换到正计时模式时，计时器从0开始
                        countup_elapsed_time = 0;
                        elapsed_time = 0;
                        message_shown = FALSE;
                        countdown_message_shown = FALSE;
                        CLOCK_TOTAL_TIME = 0; // 重置总时间，确保番茄钟状态完全清除
                        countup_message_shown = FALSE;
                        
                        // 设置番茄钟状态为空闲
                        extern POMODORO_PHASE current_pomodoro_phase;
                        extern void InitializePomodoro(void);
                        InitializePomodoro();
                        
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
                    
                    // 确保重置后窗口置顶
                    HandleWindowReset(hwnd);
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
                             MAKEINTRESOURCE(CLOCK_IDD_COLOR_DIALOG), 
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
                case CLOCK_IDM_POMODORO_START: {
                    if (!IsWindowVisible(hwnd)) {
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    
                    // 重置计时器状态
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    countdown_elapsed_time = 0;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    // 设置工作时间
                    CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                    
                    // 初始化番茄钟阶段
                    extern void InitializePomodoro(void);
                    InitializePomodoro();
                    
                    // 保存原始超时动作
                    TimeoutActionType originalAction = CLOCK_TIMEOUT_ACTION;
                    
                    // 强制设置为显示消息
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                    
                    // 启动定时器
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 1000, NULL);
                    
                    // 重置消息状态
                    elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_POMODORO_WORK: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_POMODORO_WORK_DIALOG), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        // 检查是否只有空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            // 更新番茄钟工作时间配置
                            WriteConfigPomodoroTimes(total_seconds, POMODORO_SHORT_BREAK, POMODORO_LONG_BREAK);
                            POMODORO_WORK_TIME = total_seconds;
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
                case CLOCK_IDM_POMODORO_BREAK: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_POMODORO_BREAK_DIALOG), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        // 检查是否只有空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            // 更新番茄钟短暂休息时间配置
                            WriteConfigPomodoroTimes(POMODORO_WORK_TIME, total_seconds, POMODORO_LONG_BREAK);
                            POMODORO_SHORT_BREAK = total_seconds;
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
                case CLOCK_IDM_POMODORO_LBREAK: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LBREAK_DIALOG), NULL, DlgProc);

                        if (inputText[0] == '\0') {
                            break;
                        }

                        // 检查是否只有空格
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!isspace((unsigned char)inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
                            break;
                        }

                        int total_seconds = 0;
                        if (ParseInput(inputText, &total_seconds)) {
                            // 更新番茄钟长休息时间配置
                            WriteConfigPomodoroTimes(POMODORO_WORK_TIME, POMODORO_SHORT_BREAK, total_seconds);
                            POMODORO_LONG_BREAK = total_seconds;
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
                case CLOCK_IDM_POMODORO_LOOP_COUNT:
                    ShowPomodoroLoopDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_RESET:
                    // 如果当前是番茄钟模式，重置计时器
                    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME || 
                        CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK || 
                        CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK) {
                        // 重置已用时间
                        countdown_elapsed_time = 0;
                        // 取消暂停状态
                        CLOCK_IS_PAUSED = FALSE;
                        // 重新开始计时
                        SetTimer(hwnd, 1, 1000, NULL);
                        // 强制重绘窗口
                        InvalidateRect(hwnd, NULL, TRUE);
                        
                        // 确保重置后窗口置顶
                        HandleWindowReset(hwnd);
                    }
                    break;
                case CLOCK_IDM_TIMEOUT_SHOW_TIME: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
                    WriteConfigTimeoutAction("SHOW_TIME");
                    break;
                }
                case CLOCK_IDM_TIMEOUT_COUNT_UP: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
                    WriteConfigTimeoutAction("COUNT_UP");
                    break;
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
                EndEditMode(hwnd);
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
        case WM_LBUTTONDBLCLK: {
            if (!CLOCK_EDIT_MODE) {
                // 开启编辑模式
                StartEditMode(hwnd);
                return 0;
            }
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}
