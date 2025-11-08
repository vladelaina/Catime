/**
 * @file notification.c
 * @brief Multi-modal notifications with fade animations
 */
#include <windows.h>
#include <stdlib.h>
#include "tray/tray.h"
#include "language.h"
#include "notification.h"
#include "config.h"
#include "../resource/resource.h"
#include <windowsx.h>

/** Notification config now in g_AppConfig.notification */

/** Type-safe replacement for SetPropW/GetPropW */
typedef struct {
    wchar_t* messageText;
    int windowWidth;
    AnimationState animState;
    BYTE opacity;
} NotificationData;

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);

static void FallbackToTrayNotification(HWND hwnd, const wchar_t* message);
static void LoadNotificationConfigs(void);
static HFONT CreateNotificationFont(int size, int weight);
static int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font);
static void CalculateNotificationPosition(int width, int height, int* x, int* y);
static void DrawNotificationBorder(HDC hdc, RECT rect);
static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect, HFONT font, COLORREF color, DWORD flags);
static BYTE UpdateAnimationOpacity(AnimationState state, BYTE currentOpacity, BYTE maxOpacity, BOOL* shouldDestroy);
static NotificationData* GetNotificationData(HWND hwnd);
static void SetNotificationData(HWND hwnd, NotificationData* data);
static void DestroyAllNotifications(void);

typedef struct {
    HWND hwnd;
    wchar_t* message;
} DialogThreadParams;

/** Universal fallback for all notification failures */
static void FallbackToTrayNotification(HWND hwnd, const wchar_t* message) {
    int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
    if (len > 0) {
        char* ansiMessage = (char*)malloc(len);
        if (ansiMessage) {
            WideCharToMultiByte(CP_UTF8, 0, message, -1, ansiMessage, len, NULL, NULL);
            ShowTrayNotification(hwnd, ansiMessage);
            free(ansiMessage);
        }
    }
}

static void LoadNotificationConfigs(void) {
    ReadNotificationTypeConfig();
    ReadNotificationDisabledConfig();
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
}

static HFONT CreateNotificationFont(int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                      NOTIFICATION_FONT_NAME);
}

static int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font) {
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
    SelectObject(hdc, oldFont);
    return textSize.cx;
}

/** Position in bottom-right of work area */
static void CalculateNotificationPosition(int width, int height, int* x, int* y) {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    *x = workArea.right - width - NOTIFICATION_RIGHT_MARGIN;
    *y = workArea.bottom - height - NOTIFICATION_BOTTOM_MARGIN;
}

static void DrawNotificationBorder(HDC hdc, RECT rect) {
    HPEN pen = CreatePen(PS_SOLID, 1, NOTIFICATION_BORDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect, 
                                 HFONT font, COLORREF color, DWORD flags) {
    HFONT oldFont = (HFONT)SelectObject(memDC, font);
    SetTextColor(memDC, color);
    DrawTextW(memDC, text, -1, &rect, flags);
    SelectObject(memDC, oldFont);
}

/** Centralized opacity calculation for fade animations */
static BYTE UpdateAnimationOpacity(AnimationState state, BYTE currentOpacity, 
                                   BYTE maxOpacity, BOOL* shouldDestroy) {
    *shouldDestroy = FALSE;
    
    switch (state) {
        case ANIM_FADE_IN:
            if (currentOpacity >= maxOpacity - ANIMATION_STEP) {
                return maxOpacity;
            }
            return currentOpacity + ANIMATION_STEP;
            
        case ANIM_FADE_OUT:
            if (currentOpacity <= ANIMATION_STEP) {
                *shouldDestroy = TRUE;
                return 0;
            }
            return currentOpacity - ANIMATION_STEP;
            
        case ANIM_VISIBLE:
        default:
            return currentOpacity;
    }
}

static NotificationData* GetNotificationData(HWND hwnd) {
    return (NotificationData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void SetNotificationData(HWND hwnd, NotificationData* data) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
}

/** Fallback chain: toast → modal → tray */
void ShowNotification(HWND hwnd, const wchar_t* message) {
    LoadNotificationConfigs();
    
    if (g_AppConfig.notification.display.disabled || g_AppConfig.notification.display.timeout_ms == 0) {
        return;
    }
    
    switch (g_AppConfig.notification.display.type) {
        case NOTIFICATION_TYPE_CATIME:
            ShowToastNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            ShowModalNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_OS:
            FallbackToTrayNotification(hwnd, message);
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}

static DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    DialogThreadParams* params = (DialogThreadParams*)lpParam;
    
    MessageBoxW(params->hwnd, params->message, L"Catime", MB_OK);
    
    free(params->message);
    free(params);
    
    return 0;
}

/** Background thread avoids blocking main UI */
void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    DialogThreadParams* params = (DialogThreadParams*)malloc(sizeof(DialogThreadParams));
    if (!params) {
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    size_t messageLen = wcslen(message) + 1;
    params->message = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!params->message) {
        free(params);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    params->hwnd = hwnd;
    wcscpy(params->message, message);
    
    HANDLE hThread = CreateThread(NULL, 0, ShowModalDialogThread, params, 0, NULL);
    
    if (hThread == NULL) {
        free(params->message);
        free(params);
        MessageBeep(MB_OK);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    CloseHandle(hThread);
}

/** Auto-sizes based on text, positioned in bottom-right */
void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    LoadNotificationConfigs();
    
    if (g_AppConfig.notification.display.disabled || g_AppConfig.notification.display.timeout_ms == 0) {
        return;
    }
    
    DestroyAllNotifications();
    
    if (!isClassRegistered) {
        RegisterNotificationClass(hInstance);
        isClassRegistered = TRUE;
    }
    
    NotificationData* notifData = (NotificationData*)malloc(sizeof(NotificationData));
    if (!notifData) {
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    size_t messageLen = wcslen(message) + 1;
    notifData->messageText = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!notifData->messageText) {
        free(notifData);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    wcscpy(notifData->messageText, message);
    
    HDC hdc = GetDC(hwnd);
    HFONT contentFont = CreateNotificationFont(NOTIFICATION_CONTENT_FONT_SIZE, FW_NORMAL);
    
    int textWidth = CalculateTextWidth(hdc, message, contentFont);
    int notificationWidth = textWidth + NOTIFICATION_TEXT_PADDING;
    
    if (notificationWidth < NOTIFICATION_MIN_WIDTH) 
        notificationWidth = NOTIFICATION_MIN_WIDTH;
    if (notificationWidth > NOTIFICATION_MAX_WIDTH) 
        notificationWidth = NOTIFICATION_MAX_WIDTH;
    
    DeleteObject(contentFont);
    ReleaseDC(hwnd, hdc);
    
    notifData->windowWidth = notificationWidth;
    
    int x, y;
    CalculateNotificationPosition(notificationWidth, NOTIFICATION_HEIGHT, &x, &y);
    
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
        free(notifData->messageText);
        free(notifData);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    notifData->animState = ANIM_FADE_IN;
    notifData->opacity = 0;
    
    SetNotificationData(hNotification, notifData);
    
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, g_AppConfig.notification.display.timeout_ms, NULL);
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
            
            HBRUSH whiteBrush = CreateSolidBrush(NOTIFICATION_BG_COLOR);
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            DrawNotificationBorder(memDC, clientRect);
            
            SetBkMode(memDC, TRANSPARENT);
            
            HFONT titleFont = CreateNotificationFont(NOTIFICATION_TITLE_FONT_SIZE, FW_BOLD);
            HFONT contentFont = CreateNotificationFont(NOTIFICATION_CONTENT_FONT_SIZE, FW_NORMAL);
            
            RECT titleRect = {
                NOTIFICATION_PADDING_H, 
                NOTIFICATION_PADDING_V, 
                clientRect.right - NOTIFICATION_PADDING_H, 
                NOTIFICATION_PADDING_V + NOTIFICATION_TITLE_HEIGHT
            };
            DrawNotificationText(memDC, L"Catime", titleRect, titleFont, 
                               NOTIFICATION_TITLE_COLOR, DT_SINGLELINE);
            
            NotificationData* data = GetNotificationData(hwnd);
            if (data && data->messageText) {
                RECT textRect = {
                    NOTIFICATION_PADDING_H, 
                    NOTIFICATION_CONTENT_SPACING, 
                    clientRect.right - NOTIFICATION_PADDING_H, 
                    clientRect.bottom - NOTIFICATION_PADDING_V
                };
                DrawNotificationText(memDC, data->messageText, textRect, contentFont,
                                   NOTIFICATION_CONTENT_COLOR, DT_SINGLELINE | DT_END_ELLIPSIS);
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
        
        case WM_TIMER: {
            NotificationData* data = GetNotificationData(hwnd);
            if (!data) break;
            
            if (wParam == NOTIFICATION_TIMER_ID) {
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                
                if (data->animState == ANIM_VISIBLE) {
                    data->animState = ANIM_FADE_OUT;
                    SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
                }
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                BYTE maxOpacity = (BYTE)((g_AppConfig.notification.display.max_opacity * 255) / 100);
                BOOL shouldDestroy = FALSE;
                
                BYTE newOpacity = UpdateAnimationOpacity(data->animState, data->opacity, 
                                                        maxOpacity, &shouldDestroy);
                
                if (shouldDestroy) {
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                    DestroyWindow(hwnd);
                    return 0;
                }
                
                data->opacity = newOpacity;
                SetLayeredWindowAttributes(hwnd, 0, newOpacity, LWA_ALPHA);
                
                if (data->animState == ANIM_FADE_IN && newOpacity >= maxOpacity) {
                    data->animState = ANIM_VISIBLE;
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                }
                
                return 0;
            }
            break;
        }
        
        case WM_NCHITTEST: {
            return HTCLIENT;
        }
        
        case WM_LBUTTONDOWN: {
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                data->animState = ANIM_FADE_OUT;
                SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            }
            return 0;
        }
        
        case WM_DESTROY: {
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                if (data->messageText) {
                    free(data->messageText);
                }
                free(data);
            }
            
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/** Immediately destroy all notification windows */
static void DestroyAllNotifications(void) {
    HWND hwnd = NULL;
    
    while ((hwnd = FindWindowExW(NULL, NULL, NOTIFICATION_CLASS_NAME, NULL)) != NULL) {
        DestroyWindow(hwnd);
    }
}

/** Trigger fade-out for all visible notifications */
void CloseAllNotifications(void) {
    HWND hwnd = NULL;
    HWND hwndPrev = NULL;
    
    while ((hwnd = FindWindowExW(NULL, hwndPrev, NOTIFICATION_CLASS_NAME, NULL)) != NULL) {
        NotificationData* data = GetNotificationData(hwnd);
        
        if (data) {
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            
            if (data->animState != ANIM_FADE_OUT) {
                data->animState = ANIM_FADE_OUT;
                SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            }
        }
        
        hwndPrev = hwnd;
    }
}
