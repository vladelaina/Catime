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
#define NOTIFICATION_WIDTH 350
#define NOTIFICATION_HEIGHT 80  // 调整高度
#define NOTIFICATION_TIMEOUT 3000  // 3秒后自动消失
#define NOTIFICATION_TIMER_ID 1001
#define NOTIFICATION_CLASS_NAME L"CatimeNotificationClass"  // 修改为宽字符串
// 关闭按钮相关常量
#define CLOSE_BTN_SIZE 16       // 关闭按钮的大小
#define CLOSE_BTN_MARGIN 10     // 关闭按钮的边距

// 动画相关常量
#define ANIMATION_TIMER_ID 1002  // 动画定时器ID
#define ANIMATION_STEP 5         // 每步透明度变化量减小 (0-255)
#define ANIMATION_INTERVAL 15    // 动画步骤间隔(毫秒)

// 动画状态枚举
typedef enum {
    ANIM_FADE_IN,   // 渐入动画
    ANIM_VISIBLE,   // 完全可见
    ANIM_FADE_OUT,  // 渐出动画
} AnimationState;

// 前向声明
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius);

/**
 * @brief 显示提示通知
 * @param hwnd 与通知关联的窗口句柄
 * @param message 要在通知中显示的文本消息
 * 
 * 显示一个自定义样式的通知窗口，包含标题和消息内容。
 * 通知会在8秒后自动消失，或者用户点击时立即关闭。
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
    
    // 将消息转换为宽字符
    int wlen = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
    wchar_t* wmessage = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (wmessage) {
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, wlen);
    }
    
    // 创建通知窗口 - 添加WS_EX_LAYERED样式以支持透明度
    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,  // 添加分层窗口样式以实现动画
        NOTIFICATION_CLASS_NAME,
        L"Catime 通知",  // 改为宽字符
        WS_POPUP,
        x, y,
        NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hNotification) {
        // 创建失败时，回退到简单的托盘通知
        if (wmessage) free(wmessage);
        ShowTrayNotification(hwnd, message);
        return;
    }
    
    // 保存消息文本以便绘制
    SetPropW(hNotification, L"MessageText", (HANDLE)wmessage);
    
    // 设置初始动画状态
    SetPropW(hNotification, L"AnimState", (HANDLE)ANIM_FADE_IN);
    SetPropW(hNotification, L"Opacity", (HANDLE)0);  // 起始透明度为0
    
    // 设置初始透明度为0（完全透明）
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    // 显示窗口
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    // 启动淡入动画
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    // 设置自动关闭定时器
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT, NULL);
}

/**
 * @brief 注册通知窗口类
 * @param hInstance 应用程序实例句柄
 * 
 * 注册用于显示自定义通知的窗口类
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
            DrawRoundedRectangle(memDC, clientRect, 0);
            
            // 设置文本属性
            SetBkMode(memDC, TRANSPARENT);
            
            // 创建标题字体 - 粗体
            HFONT titleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            // 创建消息内容字体
            HFONT contentFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            
            // 绘制标题 "Catime"
            SelectObject(memDC, titleFont);
            SetTextColor(memDC, RGB(0, 0, 0));
            RECT titleRect = {15, 10, clientRect.right - 15, 35};
            DrawTextW(memDC, L"Catime", -1, &titleRect, DT_SINGLELINE);
            
            // 绘制消息内容 - 放在标题下方
            SelectObject(memDC, contentFont);
            SetTextColor(memDC, RGB(100, 100, 100));
            const wchar_t* message = (const wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                RECT textRect = {15, 35, clientRect.right - 15, clientRect.bottom - 10};
                DrawTextW(memDC, message, -1, &textRect, DT_WORDBREAK);
            }
            
            // 复制到屏幕
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);
            
            // 清理资源
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
                // 主定时器到期，开始淡出动画
                KillTimer(hwnd, NOTIFICATION_TIMER_ID);
                
                // 检查当前状态 - 只有在完全可见时才开始淡出
                AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
                if (currentState == ANIM_VISIBLE) {
                    // 设置为淡出状态
                    SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
                    // 启动动画定时器
                    SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
                }
                return 0;
            }
            else if (wParam == ANIMATION_TIMER_ID) {
                // 处理动画定时器
                AnimationState state = (AnimationState)GetPropW(hwnd, L"AnimState");
                DWORD opacityVal = (DWORD)(DWORD_PTR)GetPropW(hwnd, L"Opacity");
                BYTE opacity = (BYTE)opacityVal;
                
                switch (state) {
                    case ANIM_FADE_IN:
                        // 淡入动画
                        if (opacity >= 242 - ANIMATION_STEP) {
                            // 达到最大透明度，完成淡入
                            opacity = 242;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                            
                            // 切换到可见状态并停止动画
                            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_VISIBLE);
                            KillTimer(hwnd, ANIMATION_TIMER_ID);
                        } else {
                            // 正常淡入
                            opacity += ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_FADE_OUT:
                        // 淡出动画
                        if (opacity <= ANIMATION_STEP) {
                            // 完全透明，销毁窗口
                            KillTimer(hwnd, ANIMATION_TIMER_ID);  // 确保先停止定时器
                            DestroyWindow(hwnd);
                        } else {
                            // 正常淡出
                            opacity -= ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_VISIBLE:
                        // 已完全可见状态不需要动画，停止定时器
                        KillTimer(hwnd, ANIMATION_TIMER_ID);
                        break;
                }
                return 0;
            }
            break;
            
        case WM_LBUTTONDOWN: {
            // 获取当前状态 - 只有在完全可见或淡入完成后才响应点击
            AnimationState currentState = (AnimationState)GetPropW(hwnd, L"AnimState");
            if (currentState != ANIM_VISIBLE) {
                return 0;  // 忽略点击，避免动画中途干扰
            }
            
            // 点击任何区域，开始淡出动画
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);  // 停止自动关闭定时器
            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_FADE_OUT);
            SetTimer(hwnd, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
            return 0;
        }
        
        case WM_DESTROY: {
            // 清理资源
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            
            // 释放字符串内存
            wchar_t* message = (wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                free(message);
            }
            
            // 移除所有属性
            RemovePropW(hwnd, L"MessageText");
            RemovePropW(hwnd, L"AnimState");
            RemovePropW(hwnd, L"Opacity");
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
    // 创建矩形路径
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // 使用普通矩形替代圆角矩形
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    // 清理资源
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
