/**
 * @file notification.c
 * @brief Application notification system implementation
 * 
 * This module implements various notification functions of the application, including:
 * 1. Custom styled popup notification windows with fade-in/fade-out animation effects
 * 2. System tray notification message integration
 * 3. Creation, display and lifecycle management of notification windows
 * 
 * The notification system supports UTF-8 encoded Chinese text to ensure correct display in multilingual environments.
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/config.h"
#include "../resource/resource.h"  // Contains definitions of all IDs and constants
#include <windowsx.h>  // For GET_X_LPARAM and GET_Y_LPARAM macros

// Imported from config.h
// New: notification type configuration
extern NotificationType NOTIFICATION_TYPE;

/**
 * Notification window animation state enumeration
 * Tracks the current animation phase of the notification window
 */
typedef enum {
    ANIM_FADE_IN,    // Fade-in phase - opacity increases from 0 to target value
    ANIM_VISIBLE,    // Fully visible phase - maintains maximum opacity
    ANIM_FADE_OUT,   // Fade-out phase - opacity decreases from maximum to 0
} AnimationState;

// Forward declarations
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius);



/**
 * @brief Calculate the width required for text rendering
 * @param hdc Device context
 * @param text Text to measure
 * @param font Font to use
 * @return int Width required for text rendering (pixels)
 */
int CalculateTextWidth(HDC hdc, const wchar_t* text, HFONT font) {
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
    SelectObject(hdc, oldFont);
    return textSize.cx;
}

/**
 * @brief Show notification (based on configured notification type)
 * @param hwnd Parent window handle, used to get application instance and calculate position
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays different styles of notifications based on the configured notification type
 */
void ShowNotification(HWND hwnd, const wchar_t* message) {
    // Read the latest notification type configuration
    ReadNotificationTypeConfig();
    ReadNotificationDisabledConfig();
    
    // If notifications are disabled or timeout is 0, return directly
    if (NOTIFICATION_DISABLED || NOTIFICATION_TIMEOUT_MS == 0) {
        return;
    }
    
    // Choose the corresponding notification method based on notification type
    switch (NOTIFICATION_TYPE) {
        case NOTIFICATION_TYPE_CATIME:
            ShowToastNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_SYSTEM_MODAL:
            ShowModalNotification(hwnd, message);
            break;
        case NOTIFICATION_TYPE_OS:
            // Convert Unicode to UTF-8 for ShowTrayNotification (legacy function)
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
            // Default to using Catime notification window
            ShowToastNotification(hwnd, message);
            break;
    }
}

/**
 * Modal dialog thread parameter structure
 */
typedef struct {
    HWND hwnd;               // Parent window handle
    wchar_t message[512];    // Message content
} DialogThreadParams;

/**
 * @brief Thread function to display modal dialog
 * @param lpParam Thread parameter, pointer to DialogThreadParams structure
 * @return DWORD Thread return value
 */
DWORD WINAPI ShowModalDialogThread(LPVOID lpParam) {
    DialogThreadParams* params = (DialogThreadParams*)lpParam;
    
    // Display modal dialog with fixed title "Catime" - no conversion needed as message is already Unicode
    MessageBoxW(params->hwnd, params->message, L"Catime", MB_OK);
    
    // Free allocated memory
    free(params);
    
    return 0;
}

/**
 * @brief Display system modal dialog notification
 * @param hwnd Parent window handle
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays a modal dialog in a separate thread, which won't block the main program
 */
void ShowModalNotification(HWND hwnd, const wchar_t* message) {
    // Create thread parameter structure
    DialogThreadParams* params = (DialogThreadParams*)malloc(sizeof(DialogThreadParams));
    if (!params) return;
    
    // Copy parameters
    params->hwnd = hwnd;
    wcsncpy(params->message, message, sizeof(params->message)/sizeof(wchar_t) - 1);
    params->message[sizeof(params->message)/sizeof(wchar_t) - 1] = L'\0';
    
    // Create new thread to display dialog
    HANDLE hThread = CreateThread(
        NULL,               // Default security attributes
        0,                  // Default stack size
        ShowModalDialogThread, // Thread function
        params,             // Thread parameter
        0,                  // Run thread immediately
        NULL                // Don't receive thread ID
    );
    
    // If thread creation fails, free resources
    if (hThread == NULL) {
        free(params);
        // Fall back to non-blocking notification method
        MessageBeep(MB_OK);
        // Convert Unicode to UTF-8 for ShowTrayNotification (legacy function)
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
    
    // Close thread handle, let system clean up automatically
    CloseHandle(hThread);
}

/**
 * @brief Display custom styled toast notification
 * @param hwnd Parent window handle, used to get application instance and calculate position
 * @param message Notification message text to display (Unicode string)
 * 
 * Displays a custom notification window with animation effects in the bottom right corner of the screen:
 * 1. Register notification window class (if needed)
 * 2. Calculate notification display position (bottom right of work area)
 * 3. Create notification window with fade-in/fade-out effects
 * 4. Set auto-close timer
 * 
 * Note: If creating custom notification window fails, will fall back to using system tray notification
 */
void ShowToastNotification(HWND hwnd, const wchar_t* message) {
    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    // Dynamically read the latest notification settings before displaying notification
    ReadNotificationTimeoutConfig();
    ReadNotificationOpacityConfig();
    ReadNotificationDisabledConfig();
    
    // If notifications are disabled or timeout is 0, return directly
    if (NOTIFICATION_DISABLED || NOTIFICATION_TIMEOUT_MS == 0) {
        return;
    }
    
    // Register notification window class (if not already registered)
    if (!isClassRegistered) {
        RegisterNotificationClass(hInstance);
        isClassRegistered = TRUE;
    }
    
    // Make a copy of the message for window properties (message is already Unicode)
    size_t messageLen = wcslen(message) + 1;
    wchar_t* wmessage = (wchar_t*)malloc(messageLen * sizeof(wchar_t));
    if (!wmessage) {
        // Memory allocation failed, fall back to system tray notification
        // Convert Unicode to UTF-8 for ShowTrayNotification (legacy function)
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
    
    // Calculate width needed for text
    HDC hdc = GetDC(hwnd);
    HFONT contentFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    
    // Calculate text width and add margins
    int textWidth = CalculateTextWidth(hdc, message, contentFont);
    int notificationWidth = textWidth + 40; // 20 pixel margin on each side
    
    // Ensure width is within allowed range
    if (notificationWidth < NOTIFICATION_MIN_WIDTH) 
        notificationWidth = NOTIFICATION_MIN_WIDTH;
    if (notificationWidth > NOTIFICATION_MAX_WIDTH) 
        notificationWidth = NOTIFICATION_MAX_WIDTH;
    
    DeleteObject(contentFont);
    ReleaseDC(hwnd, hdc);
    
    // Get work area size, calculate notification window position (bottom right)
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int x = workArea.right - notificationWidth - 20;
    int y = workArea.bottom - NOTIFICATION_HEIGHT - 20;
    
    // Create notification window, add layered support for transparency effects
    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,  // Keep on top and hide from taskbar
        NOTIFICATION_CLASS_NAME,
        L"Catime Notification",  // Window title (not visible)
        WS_POPUP,        // Borderless popup window style
        x, y,            // Bottom right screen position
        notificationWidth, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    // Fall back to system tray notification if creation fails
    if (!hNotification) {
        free(wmessage);
        // Convert Unicode to UTF-8 for ShowTrayNotification (legacy function)
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
    
    // Save message text and window width to window properties
    SetPropW(hNotification, L"MessageText", (HANDLE)wmessage);
    SetPropW(hNotification, L"WindowWidth", (HANDLE)(LONG_PTR)notificationWidth);
    
    // Set initial animation state to fade-in
    SetPropW(hNotification, L"AnimState", (HANDLE)ANIM_FADE_IN);
    SetPropW(hNotification, L"Opacity", (HANDLE)0);  // Initial opacity is 0
    
    // Set initial window to completely transparent
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    // Show window but don't activate (don't steal focus)
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    // Start fade-in animation
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    // Set auto-close timer, using globally configured timeout
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT_MS, NULL);
}

/**
 * @brief Register notification window class
 * @param hInstance Application instance handle
 * 
 * Registers custom notification window class with Windows, defining basic behavior and appearance:
 * 1. Set window procedure function
 * 2. Define default cursor and background
 * 3. Specify unique window class name
 * 
 * This function is only called the first time a notification is displayed, subsequent calls reuse the registered class.
 */
void RegisterNotificationClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = NotificationWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = NOTIFICATION_CLASS_NAME;
    
    RegisterClassExW(&wc);
}

/**
 * @brief Notification window message processing procedure
 * @param hwnd Window handle
 * @param msg Message ID
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return LRESULT Message processing result
 * 
 * Handles all Windows messages for the notification window, including:
 * - WM_PAINT: Draw notification window content (title, message text)
 * - WM_TIMER: Handle auto-close and animation effects
 * - WM_LBUTTONDOWN: Handle user click to close
 * - WM_DESTROY: Release window resources
 * 
 * Specifically handles animation state transition logic to ensure smooth fade-in/fade-out effects.
 */
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Get window client area size
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // Create compatible DC and bitmap for double-buffered drawing to avoid flickering
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            // Fill white background
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            // Draw rectangle with border
            DrawRoundedRectangle(memDC, clientRect, 0);
            
            // Set text drawing mode to transparent background
            SetBkMode(memDC, TRANSPARENT);
            
            // Create title font - bold
            HFONT titleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            // Create message content font
            HFONT contentFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            // Draw title "Catime"
            SelectObject(memDC, titleFont);
            SetTextColor(memDC, RGB(0, 0, 0));
            RECT titleRect = {15, 10, clientRect.right - 15, 35};
            DrawTextW(memDC, L"Catime", -1, &titleRect, DT_SINGLELINE);
            
            // Draw message content - placed below title, using single line mode
            SelectObject(memDC, contentFont);
            SetTextColor(memDC, RGB(100, 100, 100));
            const wchar_t* message = (const wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                RECT textRect = {15, 35, clientRect.right - 15, clientRect.bottom - 10};
                // Use DT_SINGLELINE|DT_END_ELLIPSIS to ensure text is displayed in one line, with ellipsis for long text
                DrawTextW(memDC, message, -1, &textRect, DT_SINGLELINE|DT_END_ELLIPSIS);
            }
            
            // Copy content from memory DC to window DC
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
            // Clean up resources
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
                // Auto-close timer triggered, start fade-out animation
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                
                // Check current state - only start fade-out when fully visible
                AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
                if (currentState == ANIM_VISIBLE) {
                    // Set to fade-out state
                    SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
                    // Start animation timer
                    SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
                }
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                // Handle animation effect timer
                AnimationState state = (AnimationState)GetPropW(hwnd, L"AnimState");
                DWORD opacityVal = (DWORD)(DWORD_PTR)GetPropW(hwnd, L"Opacity");
                BYTE opacity = (BYTE)opacityVal;
                
                // Calculate maximum opacity value (percentage converted to 0-255 range)
                BYTE maxOpacity = (BYTE)((NOTIFICATION_MAX_OPACITY * 255) / 100);
                
                switch (state) {
                    case ANIM_FADE_IN:
                        // Fade-in animation - gradually increase opacity
                        if (opacity >= maxOpacity - ANIMATION_STEP) {
                            // Reached maximum opacity, completed fade-in
                            opacity = maxOpacity;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                            
                            // Switch to visible state and stop animation
                            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_VISIBLE);
                            KillTimer(hwnd, ANIMATION_TIMER_ID);
                        } else {
                            // Normal fade-in - increase by one step each time
                            opacity += ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_FADE_OUT:
                        // Fade-out animation - gradually decrease opacity
                        if (opacity <= ANIMATION_STEP) {
                            // Completely transparent, destroy window
                            KillTimer(hwnd, ANIMATION_TIMER_ID);  // Make sure to stop timer first
                            DestroyWindow(hwnd);
                        } else {
                            // Normal fade-out - decrease by one step each time
                            opacity -= ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_VISIBLE:
                        // Fully visible state doesn't need animation, stop timer
                        KillTimer(hwnd, ANIMATION_TIMER_ID);
                        break;
                }
                return 0;
            }
            break;
            
        case WM_LBUTTONDOWN: {
            // Handle left mouse button click event (close notification early)
            
            // Get current state - only respond to clicks when fully visible or after fade-in completes
            AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
            if (currentState != ANIM_VISIBLE) {
                return 0;  // Ignore click, avoid interference during animation
            }
            
            // Click anywhere, start fade-out animation
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);  // Stop auto-close timer
            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
            SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            return 0;
        }
        
        case WM_DESTROY: {
            // Cleanup work when window is destroyed
            
            // Stop all timers
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            
            // Free message text memory
            wchar_t* message = (wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                free(message);
            }
            
            // Remove all window properties
            RemovePropW(hwnd, L"MessageText");
            RemovePropW(hwnd, L"AnimState");
            RemovePropW(hwnd, L"Opacity");
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/**
 * @brief Draw rectangle with border
 * @param hdc Device context
 * @param rect Rectangle area
 * @param radius Corner radius (unused)
 * 
 * Draws a rectangle with light gray border in the specified device context.
 * This function reserves the corner radius parameter, but current implementation uses standard rectangle.
 * Can be extended to support true rounded rectangles in future versions.
 */
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius) {
    // Create light gray border pen
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // Use normal rectangle instead of rounded rectangle
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    // Clean up resources
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

/**
 * @brief Close all currently displayed Catime notification windows
 * 
 * Find and close all notification windows created by Catime, ignoring their current display time settings,
 * directly start fade-out animation. Usually called when switching timer modes to ensure notifications don't continue to display.
 */
void CloseAllNotifications(void) {
    // Find all notification windows created by Catime
    HWND hwnd = NULL;
    HWND hwndPrev = NULL;
    
    // Use FindWindowExW to find each matching window one by one
    // First call with hwndPrev as NULL finds the first window
    // Subsequent calls pass the previously found window handle to find the next window
    while ((hwnd = FindWindowExW(NULL, hwndPrev, NOTIFICATION_CLASS_NAME, NULL)) != NULL) {
        // Check current state
        AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
        
        // Stop current auto-close timer
        KillTimer(hwnd, NOTIFICATION_TIMER_ID);
        
        // If window hasn't started fading out yet, start fade-out animation
        if (currentState != ANIM_FADE_OUT) {
            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
            // Start fade-out animation
            SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
        }
        
        // Save current window handle for next search
        hwndPrev = hwnd;
    }
}
