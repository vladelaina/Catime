/**
 * @file notification.c
 * @brief 通知功能实现
 * 
 * 本文件实现了应用程序的通知相关功能，
 * 包括系统托盘通知和自定义通知窗口。
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../resource/resource.h"
#include <windowsx.h>  // 用于GET_X_LPARAM和GET_Y_LPARAM宏

// 通知窗口相关常量
#define NOTIFICATION_WIDTH 300
#define NOTIFICATION_HEIGHT 120  // 增加高度以容纳更大的图标
#define NOTIFICATION_TIMEOUT 8000  // 8秒后自动消失
#define NOTIFICATION_TIMER_ID 1001
#define NOTIFICATION_CLASS_NAME "CatimeNotificationClass"
// 关闭按钮相关常量
#define CLOSE_BTN_SIZE 16       // 关闭按钮的大小
#define CLOSE_BTN_MARGIN 10     // 关闭按钮的边距

// 前向声明
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius);

/**
 * @brief 显示提示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 显示一个自定义样式的通知窗口，包含Catime图标、标题和消息内容。
 * 通知会在3秒后自动消失，或者用户点击时立即关闭。
 */
void ShowToastNotification(HWND hwnd, const char* message) {
    static BOOL isClassRegistered = FALSE;
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
    
    // 播放通知声音
    MessageBeep(MB_ICONINFORMATION);
    
    // 注册通知窗口类（如果还未注册）
    if (!isClassRegistered) {
        RegisterNotificationClass(hInstance);
        isClassRegistered = TRUE;
    }
    
    // 获取主窗口位置，计算通知窗口位置（右下角）
    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    
    int x = workArea.right - NOTIFICATION_WIDTH - 20;
    int y = workArea.bottom - NOTIFICATION_HEIGHT - 20;
    
    // 创建通知窗口
    HWND hNotification = CreateWindowEx(
        WS_EX_TOPMOST,
        NOTIFICATION_CLASS_NAME,
        "Catime 通知",
        WS_POPUP,
        x, y,
        NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hNotification) {
        // 创建失败时，回退到简单的托盘通知
        ShowTrayNotification(hwnd, message);
        return;
    }
    
    // 保存消息文本以便绘制
    SetProp(hNotification, "MessageText", (HANDLE)_strdup(message));
    
    // 加载应用程序图标 - 使用高DPI版本
    HICON hIcon = (HICON)LoadImage(hInstance, 
                                  MAKEINTRESOURCE(IDI_CATIME), 
                                  IMAGE_ICON, 
                                  64, 64,   // 指定尺寸
                                  LR_DEFAULTCOLOR);
    if (hIcon) {
        SetProp(hNotification, "NotificationIcon", hIcon);
    }
    
    // 显示窗口并设置定时器用于自动关闭
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT, NULL);
}

/**
 * @brief 注册通知窗口类
 * @param hInstance 应用程序实例句柄
 * 
 * 注册用于显示自定义通知的窗口类
 */
void RegisterNotificationClass(HINSTANCE hInstance) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = NotificationWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = NOTIFICATION_CLASS_NAME;
    
    RegisterClassEx(&wc);
}

/**
 * @brief 通知窗口消息处理过程
 * @param hwnd 窗口句柄
 * @param msg 消息ID
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return LRESULT 消息处理结果
 * 
 * 处理通知窗口的绘制、鼠标点击、定时器等消息
 */
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isCloseButtonHovered = FALSE;  // 鼠标是否悬停在关闭按钮上
    
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口客户区大小
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // 创建兼容DC和位图用于双缓冲
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            // 填充白色背景
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            // 绘制圆角矩形边框
            DrawRoundedRectangle(memDC, clientRect, 10);
            
            // 绘制图标 - 使用更合适的尺寸，与窗口高度匹配
            HICON hIcon = (HICON)GetProp(hwnd, "NotificationIcon");
            if (hIcon) {
                int iconSize = 64; // 将图标大小改为更合适的尺寸
                DrawIconEx(memDC, 15, (clientRect.bottom - iconSize) / 2, 
                          hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
            }
            
            // 设置文本属性
            SetBkMode(memDC, TRANSPARENT);
            
            // 创建消息内容字体
            HFONT contentFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Microsoft YaHei");
            HFONT oldFont = (HFONT)SelectObject(memDC, contentFont);
            SetTextColor(memDC, RGB(100, 100, 100));
            
            // 绘制消息内容 - 由于没有标题，可以占用更多空间
            const char* message = (const char*)GetProp(hwnd, "MessageText");
            if (message) {
                RECT textRect = {90, 20, clientRect.right - 15, clientRect.bottom - 15};
                DrawText(memDC, message, -1, &textRect, DT_WORDBREAK);
            }
            
            // 绘制关闭按钮 (X)
            RECT closeButtonRect;
            closeButtonRect.right = clientRect.right - CLOSE_BTN_MARGIN;
            closeButtonRect.left = closeButtonRect.right - CLOSE_BTN_SIZE;
            closeButtonRect.top = CLOSE_BTN_MARGIN;
            closeButtonRect.bottom = closeButtonRect.top + CLOSE_BTN_SIZE;
            
            // 设置关闭按钮颜色 - 悬停时为红色，否则为灰色
            COLORREF btnColor = isCloseButtonHovered ? RGB(255, 0, 0) : RGB(150, 150, 150);
            HPEN closePen = CreatePen(PS_SOLID, 2, btnColor);
            HPEN oldClosePen = (HPEN)SelectObject(memDC, closePen);
            
            // 绘制 X
            MoveToEx(memDC, closeButtonRect.left, closeButtonRect.top, NULL);
            LineTo(memDC, closeButtonRect.right, closeButtonRect.bottom);
            MoveToEx(memDC, closeButtonRect.right, closeButtonRect.top, NULL);
            LineTo(memDC, closeButtonRect.left, closeButtonRect.bottom);
            
            // 恢复原来的画笔
            SelectObject(memDC, oldClosePen);
            DeleteObject(closePen);
            
            // 复制到屏幕
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
            // 清理资源
            SelectObject(memDC, oldFont);
            DeleteObject(contentFont);
            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            // 获取鼠标坐标
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            
            // 计算关闭按钮区域
            RECT closeButtonRect;
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            closeButtonRect.right = clientRect.right - CLOSE_BTN_MARGIN;
            closeButtonRect.left = closeButtonRect.right - CLOSE_BTN_SIZE;
            closeButtonRect.top = CLOSE_BTN_MARGIN;
            closeButtonRect.bottom = closeButtonRect.top + CLOSE_BTN_SIZE;
            
            // 检查鼠标是否在关闭按钮上
            BOOL newHoverState = (xPos >= closeButtonRect.left && xPos <= closeButtonRect.right &&
                                  yPos >= closeButtonRect.top && yPos <= closeButtonRect.bottom);
            
            // 如果悬停状态改变，重绘窗口
            if (newHoverState != isCloseButtonHovered) {
                isCloseButtonHovered = newHoverState;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            
            // 设置鼠标跟踪，确保收到WM_MOUSELEAVE消息
            if (!isCloseButtonHovered) {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
            
            return 0;
        }
        
        case WM_MOUSELEAVE:
            // 鼠标离开窗口时重置悬停状态
            if (isCloseButtonHovered) {
                isCloseButtonHovered = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
            
        case WM_LBUTTONDOWN: {
            // 获取鼠标坐标
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);
            
            // 计算关闭按钮区域
            RECT closeButtonRect;
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            closeButtonRect.right = clientRect.right - CLOSE_BTN_MARGIN;
            closeButtonRect.left = closeButtonRect.right - CLOSE_BTN_SIZE;
            closeButtonRect.top = CLOSE_BTN_MARGIN;
            closeButtonRect.bottom = closeButtonRect.top + CLOSE_BTN_SIZE;
            
            // 如果点击在关闭按钮上，关闭窗口
            if (xPos >= closeButtonRect.left && xPos <= closeButtonRect.right &&
                yPos >= closeButtonRect.top && yPos <= closeButtonRect.bottom) {
                DestroyWindow(hwnd);
                return 0;
            }
            
            // 否则，普通点击任意位置也关闭窗口(保持原有行为)
            DestroyWindow(hwnd);
            return 0;
        }
        
        case WM_TIMER:
            if (wParam == NOTIFICATION_TIMER_ID) {
                // 定时器到期，关闭通知
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                DestroyWindow(hwnd);
            }
            return 0;
            
        case WM_DESTROY: {
            // 清理资源
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            
            // 释放字符串内存
            char* message = (char*)GetProp(hwnd, "MessageText");
            if (message) {
                free(message);
            }
            
            // 移除属性
            RemoveProp(hwnd, "MessageText");
            RemoveProp(hwnd, "NotificationIcon");
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/**
 * @brief 绘制圆角矩形
 * @param hdc 设备上下文
 * @param rect 矩形区域
 * @param radius 圆角半径
 * 
 * 在指定的设备上下文中绘制带圆角的矩形
 */
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius) {
    // 创建圆角矩形路径
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // 使用圆角矩形API
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
    
    // 清理资源
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
