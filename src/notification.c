/**
 * @file notification.c
 * @brief Multi-modal notification system with animations and fallback mechanisms
 * @version 2.0 - Refactored for better maintainability and reduced code duplication
 */
#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/config.h"
#include "../resource/resource.h"
#include <windowsx.h>

extern NotificationType NOTIFICATION_TYPE;

/* ============================================================================
 * Custom Window Data Structure (replaces Window Properties)
 * ============================================================================ */

/**
 * @brief Notification window instance data
 * Type-safe replacement for SetPropW/GetPropW pattern
 */
typedef struct {
    wchar_t* messageText;        /**< Dynamically allocated message text */
    int windowWidth;             /**< Calculated window width */
    AnimationState animState;    /**< Current animation state */
    BYTE opacity;                /**< Current opacity value (0-255) */
} NotificationData;

/* ============================================================================
 * Internal Helper Functions - Declarations
 * ============================================================================ */

LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);

/** Helper functions for reducing code duplication */
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

/* ============================================================================
 * Thread Parameters for Modal Dialogs
 * ============================================================================ */

/**
 * @brief Thread parameters for non-blocking modal dialogs
 * Uses dynamic allocation for flexible message length
 */
typedef struct {
    HWND hwnd;
    wchar_t* message;  /**< Dynamically allocated message */
} DialogThreadParams;

/* ============================================================================
 * Helper Function Implementations
 * ============================================================================ */

/**
 * @brief Fallback mechanism to show tray notification when other methods fail
 * @param hwnd Parent window handle
 * @param message Wide-char notification message
 * 
 * Converts message to UTF-8 and delegates to tray notification system.
 * This is the universal fallback for all notification failures.
 */
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

/**
 * @brief Batch load all notification-related configuration values
 * Centralizes configuration loading for consistency
 */
static void LoadNotificationConfigs(void) {
    ReadNotificationTypeConfig();
    ReadNotificationDisabledConfig();
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
}

/**
 * @brief Create notification font with standardized parameters
 * @param size Font size in logical units
 * @param weight Font weight (FW_NORMAL, FW_BOLD, etc.)
 * @return Font handle (caller must DeleteObject)
 */
static HFONT CreateNotificationFont(int size, int weight) {
    return CreateFontW(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                      NOTIFICATION_FONT_NAME);
}

/**
 * @brief Calculate pixel width of text for dynamic notification sizing
 * @param hdc Device context for text measurement
 * @param text Wide string to measure
 * @param font Font to use for measurement
 * @return Text width in pixels
 */
static int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font) {
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
    SelectObject(hdc, oldFont);
    return textSize.cx;
}

/**
 * @brief Calculate notification window position in bottom-right of work area
 * @param width Notification window width
 * @param height Notification window height
 * @param x Output: X coordinate
 * @param y Output: Y coordinate
 */
static void CalculateNotificationPosition(int width, int height, int* x, int* y) {
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    *x = workArea.right - width - NOTIFICATION_RIGHT_MARGIN;
    *y = workArea.bottom - height - NOTIFICATION_BOTTOM_MARGIN;
}

/**
 * @brief Draw notification border with consistent styling
 * @param hdc Device context for drawing
 * @param rect Rectangle bounds
 */
static void DrawNotificationBorder(HDC hdc, RECT rect) {
    HPEN pen = CreatePen(PS_SOLID, 1, NOTIFICATION_BORDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/**
 * @brief Draw text with automatic font selection and color management
 * @param memDC Memory device context for drawing
 * @param text Text to render
 * @param rect Drawing rectangle
 * @param font Font to use (will be selected and cleaned up)
 * @param color Text color
 * @param flags DrawText flags (DT_SINGLELINE, etc.)
 */
static void DrawNotificationText(HDC memDC, const wchar_t* text, RECT rect, 
                                 HFONT font, COLORREF color, DWORD flags) {
    HFONT oldFont = (HFONT)SelectObject(memDC, font);
    SetTextColor(memDC, color);
    DrawTextW(memDC, text, -1, &rect, flags);
    SelectObject(memDC, oldFont);
}

/**
 * @brief Update opacity value based on animation state
 * @param state Current animation state
 * @param currentOpacity Current opacity value
 * @param maxOpacity Maximum opacity to reach
 * @param shouldDestroy Output: whether window should be destroyed
 * @return New opacity value
 * 
 * Centralizes opacity calculation logic for fade-in/fade-out animations
 */
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

/**
 * @brief Get notification data from window
 * @param hwnd Window handle
 * @return Notification data pointer or NULL
 */
static NotificationData* GetNotificationData(HWND hwnd) {
    return (NotificationData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/**
 * @brief Store notification data in window
 * @param hwnd Window handle
 * @param data Notification data pointer
 */
static void SetNotificationData(HWND hwnd, NotificationData* data) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Dispatch notifications to appropriate handler based on configuration
 * @param hwnd Parent window handle
 * @param message Notification message text
 * 
 * Provides graceful fallback chain: toast -> modal -> tray
 */
void ShowNotification(HWND hwnd, const wchar_t* message) {
    LoadNotificationConfigs();
    
    /** Early exit if notifications are disabled */
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
            FallbackToTrayNotification(hwnd, message);
            break;
        default:
            ShowToastNotification(hwnd, message);
            break;
    }
}

/**
 * @brief Thread entry point for modal notification dialog
 * @param lpParam DialogThreadParams containing message and parent window
 * @return Thread exit code (always 0)
 * 
 * Cleans up allocated parameters after dialog closes
 */
static DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    DialogThreadParams* params = (DialogThreadParams*)lpParam;
    
    MessageBoxW(params->hwnd, params->message, L"Catime", MB_OK);
    
    free(params->message);
    free(params);
    
    return 0;
}

/**
 * @brief Show non-blocking modal notification with tray fallback
 * @param hwnd Parent window handle
 * @param message Notification message text
 * 
 * Creates background thread to avoid blocking main UI thread
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    DialogThreadParams* params = (DialogThreadParams*)malloc(sizeof(DialogThreadParams));
    if (!params) {
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    /** Allocate dynamic message buffer */
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
    
    /** Fallback to tray notification if thread creation fails */
    if (hThread == NULL) {
        free(params->message);
        free(params);
        MessageBeep(MB_OK);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    /** Detach thread to prevent resource leaks */
    CloseHandle(hThread);
}

/**
 * @brief Create animated toast notification with auto-sizing and positioning
 * @param hwnd Parent window handle
 * @param message Notification message text
 * 
 * Falls back to tray notification on any failure during creation
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    LoadNotificationConfigs();
    
    if (NOTIFICATION_DISABLED || NOTIFICATION_TIMEOUT_MS == 0) {
        return;
    }
    
    /** One-time window class registration */
    if (!isClassRegistered) {
        RegisterNotificationClass(hInstance);
        isClassRegistered = TRUE;
    }
    
    /** Allocate notification data structure */
    NotificationData* notifData = (NotificationData*)malloc(sizeof(NotificationData));
    if (!notifData) {
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    /** Allocate persistent message copy */
    size_t messageLen = wcslen(message) + 1;
    notifData->messageText = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!notifData->messageText) {
        free(notifData);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    wcscpy(notifData->messageText, message);
    
    /** Calculate dynamic notification width based on text content */
    HDC hdc = GetDC(hwnd);
    HFONT contentFont = CreateNotificationFont(NOTIFICATION_CONTENT_FONT_SIZE, FW_NORMAL);
    
    int textWidth = CalculateTextWidth(hdc, message, contentFont);
    int notificationWidth = textWidth + NOTIFICATION_TEXT_PADDING;
    
    /** Enforce minimum and maximum width constraints */
    if (notificationWidth < NOTIFICATION_MIN_WIDTH) 
        notificationWidth = NOTIFICATION_MIN_WIDTH;
    if (notificationWidth > NOTIFICATION_MAX_WIDTH) 
        notificationWidth = NOTIFICATION_MAX_WIDTH;
    
    DeleteObject(contentFont);
    ReleaseDC(hwnd, hdc);
    
    notifData->windowWidth = notificationWidth;
    
    /** Position notification in bottom-right corner of work area */
    int x, y;
    CalculateNotificationPosition(notificationWidth, NOTIFICATION_HEIGHT, &x, &y);
    
    /** Create layered popup window for transparency support */
    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        NOTIFICATION_CLASS_NAME,
        L"Catime Notification",
        WS_POPUP,
        x, y,
        notificationWidth, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    /** Fallback to tray if window creation fails */
    if (!hNotification) {
        free(notifData->messageText);
        free(notifData);
        FallbackToTrayNotification(hwnd, message);
        return;
    }
    
    /** Initialize animation state */
    notifData->animState = ANIM_FADE_IN;
    notifData->opacity = 0;
    
    /** Store notification data in window */
    SetNotificationData(hNotification, notifData);
    
    /** Start completely transparent */
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    /** Start fade-in animation */
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    /** Set auto-dismiss timer */
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT_MS, NULL);
}

/**
 * @brief Register window class for toast notifications
 * @param hInstance Application instance handle
 * 
 * Sets up basic window class with standard cursor and background
 */
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

/**
 * @brief Window procedure for toast notification windows
 * @param hwnd Notification window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * 
 * Handles painting, animation timers, click-to-dismiss, and cleanup
 */
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            /** Use double buffering to prevent flicker */
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            /** Fill background with white */
            HBRUSH whiteBrush = CreateSolidBrush(NOTIFICATION_BG_COLOR);
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            DrawNotificationBorder(memDC, clientRect);
            
            SetBkMode(memDC, TRANSPARENT);
            
            /** Create fonts for title and content */
            HFONT titleFont = CreateNotificationFont(NOTIFICATION_TITLE_FONT_SIZE, FW_BOLD);
            HFONT contentFont = CreateNotificationFont(NOTIFICATION_CONTENT_FONT_SIZE, FW_NORMAL);
            
            /** Draw title "Catime" */
            RECT titleRect = {
                NOTIFICATION_PADDING_H, 
                NOTIFICATION_PADDING_V, 
                clientRect.right - NOTIFICATION_PADDING_H, 
                NOTIFICATION_TITLE_HEIGHT
            };
            DrawNotificationText(memDC, L"Catime", titleRect, titleFont, 
                               NOTIFICATION_TITLE_COLOR, DT_SINGLELINE);
            
            /** Draw message content with ellipsis if too long */
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
            
            /** Copy buffer to screen */
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
            /** Cleanup resources */
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
            
            /** Auto-dismiss timer expired, start fade-out */
            if (wParam == NOTIFICATION_TIMER_ID) {
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                
                if (data->animState == ANIM_VISIBLE) {
                    data->animState = ANIM_FADE_OUT;
                    SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
                }
                return 0;
            }
            /** Animation frame timer for fade effects */
            else if (wParam == ANIMATION_TIMER_ID) {
                BYTE maxOpacity = (BYTE)((NOTIFICATION_MAX_OPACITY * 255) / 100);
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
                
                /** Stop animation when fade-in completes */
                if (data->animState == ANIM_FADE_IN && newOpacity >= maxOpacity) {
                    data->animState = ANIM_VISIBLE;
                    KillTimer(hwnd, ANIMATION_TIMER_ID);
                }
                
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONDOWN: {
            /** Click-to-dismiss: always start fade-out on click */
            NotificationData* data = GetNotificationData(hwnd);
            if (data) {
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                data->animState = ANIM_FADE_OUT;
                SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            }
            return 0;
        }
        
        case WM_DESTROY: {
            /** Cleanup: stop timers and free allocated data */
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

/**
 * @brief Force close all active toast notifications
 * 
 * Triggers fade-out animation for all visible notifications
 */
void CloseAllNotifications(void) {
    HWND hwnd = NULL;
    HWND hwndPrev = NULL;
    
    /** Enumerate all notification windows and start fade-out */
    while ((hwnd = FindWindowExW(NULL, hwndPrev, NOTIFICATION_CLASS_NAME, NULL)) != NULL) {
        NotificationData* data = GetNotificationData(hwnd);
        
        if (data) {
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            
            /** Only start fade-out if not already fading out */
            if (data->animState != ANIM_FADE_OUT) {
                data->animState = ANIM_FADE_OUT;
                SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            }
        }
        
        hwndPrev = hwnd;
    }
}
