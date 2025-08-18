#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/config.h"
#include "../resource/resource.h"
#include <windowsx.h>

extern NotificationType NOTIFICATION_TYPE;

typedef enum {
    ANIM_FADE_IN,
    ANIM_VISIBLE,
    ANIM_FADE_OUT,
} AnimationState;

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius);

int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font) {
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
    SelectObject(hdc, oldFont);
    return textSize.cx;
}

void ShowNotification(HWND hwnd, const wchar_t* message) {
    ReadNotificationTypeConfig();
    ReadNotificationDisabledConfig();
    
    if (NOTIFICATION_DISABLED || NOTIFICATION_TIMEOUT_MS == 0) {
        return;
    }
    
    switch (NOTIFICATION_TYPE) {
        case NOTIFICATION_TYPE_CATIME:
            ShowToastNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            ShowModalNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_OS:
            int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
            if (len > 0) {
                char* ansiMessage = (char*)malloc(len);
                if (ansiMessage) {
                    WideCharToMultiByte(CP_UTF8, 0, message, -1, ansiMessage, len, NULL, NULL);
                    ShowTrayNotification(hwnd, ansiMessage);
                    free(ansiMessage);
                }
            }
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}

typedef struct {
    HWND hwnd;
    wchar_t message[512];
} DialogThreadParams;

DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    DialogThreadParams* params = (DialogThreadParams*)lpParam;
    
    MessageBoxW(params->hwnd, params->message, L"Catime", MB_OK);
    
    free(params);
    
    return 0;
}

void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    DialogThreadParams* params = (DialogThreadParams*)malloc(sizeof(DialogThreadParams));
    if (!params) return;
    
    params->hwnd = hwnd;
    wcsncpy(params->message, message, sizeof(params->message)/sizeof(wchar_t) - 1);
    params->message[sizeof(params->message)/sizeof(wchar_t) - 1] = L'\0';
    
    HANDLE hThread = CreateThread(
        NULL,
        0,
        ShowModalDialogThread,
        params,
        0,
        NULL
    );
    
    if (hThread == NULL) {
        free(params);
        MessageBeep(MB_OK);
        int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            char* ansiMessage = (char*)malloc(len);
            if (ansiMessage) {
                WideCharToMultiByte(CP_UTF8, 0, message, -1, ansiMessage, len, NULL, NULL);
                ShowTrayNotification(hwnd, ansiMessage);
                free(ansiMessage);
            }
        }
        return;
    }
    
    CloseHandle(hThread);
}

void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
    ReadNotificationDisabledConfig();
    
    if (NOTIFICATION_DISABLED || NOTIFICATION_TIMEOUT_MS == 0) {
        return;
    }
    
    if (!isClassRegistered) {
        RegisterNotificationClass(hInstance);
        isClassRegistered = TRUE;
    }
    
    size_t messageLen = wcslen(message) + 1;
    wchar_t* wmessage = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!wmessage) {
        int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            char* ansiMessage = (char*)malloc(len);
            if (ansiMessage) {
                WideCharToMultiByte(CP_UTF8, 0, message, -1, ansiMessage, len, NULL, NULL);
                ShowTrayNotification(hwnd, ansiMessage);
                free(ansiMessage);
            }
        }
        return;
    }
    wcscpy(wmessage, message);
    
    HDC hdc = GetDC(hwnd);
    HFONT contentFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    
    int textWidth = CalculateTextWidth(hdc, message, contentFont);
    int notificationWidth = textWidth + 40;
    
    if (notificationWidth < NOTIFICATION_MIN_WIDTH) 
        notificationWidth = NOTIFICATION_MIN_WIDTH;
    if (notificationWidth > NOTIFICATION_MAX_WIDTH) 
        notificationWidth = NOTIFICATION_MAX_WIDTH;
    
    DeleteObject(contentFont);
    ReleaseDC(hwnd, hdc);
    
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int x = workArea.right - notificationWidth - 20;
    int y = workArea.bottom - NOTIFICATION_HEIGHT - 20;
    
    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        NOTIFICATION_CLASS_NAME,
        L"Catime Notification",
        WS_POPUP,
        x, y,
        notificationWidth, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hNotification) {
        free(wmessage);
        int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            char* ansiMessage = (char*)malloc(len);
            if (ansiMessage) {
                WideCharToMultiByte(CP_UTF8, 0, message, -1, ansiMessage, len, NULL, NULL);
                ShowTrayNotification(hwnd, ansiMessage);
                free(ansiMessage);
            }
        }
        return;
    }
    
    SetPropW(hNotification, L"MessageText", (HANDLE)wmessage);
    SetPropW(hNotification, L"WindowWidth", (HANDLE)(LONG_PTR)notificationWidth);
    
    SetPropW(hNotification, L"AnimState", (HANDLE)ANIM_FADE_IN);
    SetPropW(hNotification, L"Opacity", (HANDLE)0);
    
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT_MS, NULL);
}

void RegisterNotificationClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = NotificationWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = NOTIFICATION_CLASS_NAME;
    
    RegisterClassExW(&wc);
}

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            DrawRoundedRectangle(memDC, clientRect, 0);
            
            SetBkMode(memDC, TRANSPARENT);
            
            HFONT titleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            HFONT contentFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            SelectObject(memDC, titleFont);
            SetTextColor(memDC, RGB(0, 0, 0));
            RECT titleRect = {15, 10, clientRect.right - 15, 35};
            DrawTextW(memDC, L"Catime", -1, &titleRect, DT_SINGLELINE);
            
            SelectObject(memDC, contentFont);
            SetTextColor(memDC, RGB(100, 100, 100));
            const wchar_t* message = (const wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                RECT textRect = {15, 35, clientRect.right - 15, clientRect.bottom - 10};
                DrawTextW(memDC, message, -1, &textRect, DT_SINGLELINE|DT_END_ELLIPSIS);
            }
            
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
            SelectObject(memDC, oldBitmap);
            DeleteObject(titleFont);
            DeleteObject(contentFont);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_TIMER:
            if (wParam == NOTIFICATION_TIMER_ID) {
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                
                AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
                if (currentState == ANIM_VISIBLE) {
                    SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
                    SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
                }
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                AnimationState state = (AnimationState)GetPropW(hwnd, L"AnimState");
                DWORD opacityVal = (DWORD)(DWORD_PTR)GetPropW(hwnd, L"Opacity");
                BYTE opacity = (BYTE)opacityVal;
                
                BYTE maxOpacity = (BYTE)((NOTIFICATION_MAX_OPACITY * 255) / 100);
                
                switch (state) {
                    case ANIM_FADE_IN:
                        if (opacity >= maxOpacity - ANIMATION_STEP) {
                            opacity = maxOpacity;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                            
                            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_VISIBLE);
                            KillTimer(hwnd, ANIMATION_TIMER_ID);
                        } else {
                            opacity += ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_FADE_OUT:
                        if (opacity <= ANIMATION_STEP) {
                            KillTimer(hwnd, ANIMATION_TIMER_ID);
                            DestroyWindow(hwnd);
                        } else {
                            opacity -= ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_VISIBLE:
                        KillTimer(hwnd, ANIMATION_TIMER_ID);
                        break;
                }
                return 0;
            }
            break;
            
        case WM_LBUTTONDOWN: {
            AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
            if (currentState != ANIM_VISIBLE) {
                return 0;
            }
            
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
            SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            return 0;
        }
        
        case WM_DESTROY: {
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            
            wchar_t* message = (wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                free(message);
            }
            
            RemovePropW(hwnd, L"MessageText");
            RemovePropW(hwnd, L"AnimState");
            RemovePropW(hwnd, L"Opacity");
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DrawRoundedRectangle(HDC hdc, RECT rect, int radius) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void CloseAllNotifications(void) {
    HWND hwnd = NULL;
    HWND hwndPrev = NULL;
    
    while ((hwnd = FindWindowExW(NULL, hwndPrev, NOTIFICATION_CLASS_NAME, NULL)) != NULL) {
        AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
        
        KillTimer(hwnd, NOTIFICATION_TIMER_ID);
        
        if (currentState != ANIM_FADE_OUT) {
            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
            SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
        }
        
        hwndPrev = hwnd;
    }
}