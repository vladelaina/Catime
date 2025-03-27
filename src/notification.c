/**
 * @file notification.c
 * @brief 应用通知系统实现
 * 
 * 本模块实现了应用程序的各类通知功能，包括：
 * 1. 自定义样式的弹出通知窗口，支持淡入淡出动画效果
 * 2. 系统托盘通知消息集成
 * 3. 通知窗口的创建、显示和生命周期管理
 * 
 * 通知系统支持UTF-8编码的中文文本，确保多语言环境下的正确显示。
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/tray.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../resource/resource.h"
#include <windowsx.h>  // 用于GET_X_LPARAM和GET_Y_LPARAM宏

/**
 * 通知窗口相关常量定义
 */
// 通知窗口尺寸与布局常量
#define NOTIFICATION_WIDTH 350       // 通知窗口宽度(像素)
#define NOTIFICATION_HEIGHT 80       // 通知窗口高度(像素)
#define NOTIFICATION_TIMER_ID 1001   // 通知超时计时器ID
#define NOTIFICATION_CLASS_NAME L"CatimeNotificationClass"  // 通知窗口类名
// 窗口界面元素常量
#define CLOSE_BTN_SIZE 16            // 关闭按钮的大小(像素)
#define CLOSE_BTN_MARGIN 10          // 关闭按钮边距(像素)

// 动画效果相关常量
#define ANIMATION_TIMER_ID 1002      // 动画计时器ID
#define ANIMATION_STEP 5             // 每步透明度变化量(0-255)
#define ANIMATION_INTERVAL 15        // 动画帧间隔(毫秒)

// 从config.h引入
extern int NOTIFICATION_TIMEOUT_MS; // 修改：使用extern关键字声明，不再定义

/**
 * 通知窗口动画状态枚举
 * 跟踪当前通知窗口所处的动画阶段
 */
typedef enum {
    ANIM_FADE_IN,    // 淡入阶段 - 透明度从0增加到目标值
    ANIM_VISIBLE,    // 完全可见阶段 - 保持最大透明度显示
    ANIM_FADE_OUT,   // 淡出阶段 - 透明度从最大值降低到0
} AnimationState;

// 前向声明
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RegisterNotificationClass(HINSTANCE hInstance);
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius);

/**
 * @brief 显示自定义样式的提示通知
 * @param hwnd 父窗口句柄，用于获取应用实例和计算位置
 * @param message 要显示的通知消息文本(UTF-8编码)
 * 
 * 在屏幕右下角显示一个带动画效果的自定义通知窗口：
 * 1. 注册通知窗口类(如需)
 * 2. 计算通知显示位置(工作区右下角)
 * 3. 创建带淡入淡出效果的通知窗口
 * 4. 设置自动关闭计时器
 * 
 * 注意：如果创建自定义通知窗口失败，将回退到使用系统托盘通知
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
    
    // 将消息转换为宽字符以支持Unicode显示
    int wlen = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
    wchar_t* wmessage = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    if (wmessage) {
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wmessage, wlen);
    }
    
    // 创建通知窗口，添加图层支持以实现透明效果
    HWND hNotification = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,  // 保持置顶并从任务栏隐藏
        NOTIFICATION_CLASS_NAME,
        L"Catime 通知",  // 窗口标题(不可见)
        WS_POPUP,        // 无边框弹出窗口样式
        x, y,            // 屏幕右下角位置
        NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT,
        NULL, NULL, hInstance, NULL
    );
    
    // 创建失败时回退到系统托盘通知
    if (!hNotification) {
        if (wmessage) free(wmessage);
        ShowTrayNotification(hwnd, message);
        return;
    }
    
    // 保存消息文本到窗口属性中，便于绘制
    SetPropW(hNotification, L"MessageText", (HANDLE)wmessage);
    
    // 设置初始动画状态为淡入
    SetPropW(hNotification, L"AnimState", (HANDLE)ANIM_FADE_IN);
    SetPropW(hNotification, L"Opacity", (HANDLE)0);  // 起始透明度为0
    
    // 设置初始窗口为完全透明
    SetLayeredWindowAttributes(hNotification, 0, 0, LWA_ALPHA);
    
    // 显示窗口但不激活(不抢焦点)
    ShowWindow(hNotification, SW_SHOWNOACTIVATE);
    UpdateWindow(hNotification);
    
    // 启动淡入动画
    SetTimer(hNotification, ANIMATION_TIMER_ID, ANIMATION_INTERVAL, NULL);
    
    // 设置自动关闭定时器，使用全局配置的超时时间
    SetTimer(hNotification, NOTIFICATION_TIMER_ID, NOTIFICATION_TIMEOUT_MS, NULL);
}

/**
 * @brief 注册通知窗口类
 * @param hInstance 应用程序实例句柄
 * 
 * 向Windows注册自定义通知窗口类，定义窗口的基本行为和外观：
 * 1. 设置窗口过程函数
 * 2. 定义默认光标和背景
 * 3. 指定唯一的窗口类名
 * 
 * 此函数仅在首次显示通知时被调用，后续复用已注册的窗口类。
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
 * 处理通知窗口的所有Windows消息，包括：
 * - WM_PAINT: 绘制通知窗口内容(标题、消息文本)
 * - WM_TIMER: 处理自动关闭和动画效果
 * - WM_LBUTTONDOWN: 处理用户点击关闭
 * - WM_DESTROY: 释放窗口资源
 * 
 * 特别处理了动画状态的转换逻辑，确保平滑的淡入淡出效果。
 */
LRESULT CALLBACK NotificationWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口客户区大小
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // 创建兼容DC和位图用于双缓冲绘制，避免闪烁
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
            
            // 填充白色背景
            HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &clientRect, whiteBrush);
            DeleteObject(whiteBrush);
            
            // 绘制带边框的矩形
            DrawRoundedRectangle(memDC, clientRect, 0);
            
            // 设置文本绘制模式为透明背景
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
            
            // 将内存DC中的内容复制到窗口DC
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
                // 自动关闭定时器触发，开始淡出动画
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
                // 处理动画效果的定时器
                AnimationState state = (AnimationState)GetPropW(hwnd, L"AnimState");
                DWORD opacityVal = (DWORD)(DWORD_PTR)GetPropW(hwnd, L"Opacity");
                BYTE opacity = (BYTE)opacityVal;
                
                switch (state) {
                    case ANIM_FADE_IN:
                        // 淡入动画 - 逐渐增加不透明度
                        if (opacity >= 242 - ANIMATION_STEP) {
                            // 达到最大透明度，完成淡入
                            opacity = 242;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                            
                            // 切换到可见状态并停止动画
                            SetPropW(hwnd, L"AnimState", (HANDLE)ANIM_VISIBLE);
                            KillTimer(hwnd, ANIMATION_TIMER_ID);
                        } else {
                            // 正常淡入 - 每次增加一个步长
                            opacity += ANIMATION_STEP;
                            SetPropW(hwnd, L"Opacity", (HANDLE)(DWORD_PTR)opacity);
                            SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_ALPHA);
                        }
                        break;
                        
                    case ANIM_FADE_OUT:
                        // 淡出动画 - 逐渐减少不透明度
                        if (opacity <= ANIMATION_STEP) {
                            // 完全透明，销毁窗口
                            KillTimer(hwnd, ANIMATION_TIMER_ID);  // 确保先停止定时器
                            DestroyWindow(hwnd);
                        } else {
                            // 正常淡出 - 每次减少一个步长
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
            // 处理鼠标左键点击事件(提前关闭通知)
            
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
            // 窗口销毁时的清理工作
            
            // 停止所有计时器
            KillTimer(hwnd, NOTIFICATION_TIMER_ID);
            KillTimer(hwnd, ANIMATION_TIMER_ID);
            
            // 释放消息文本的内存
            wchar_t* message = (wchar_t*)GetPropW(hwnd, L"MessageText");
            if (message) {
                free(message);
            }
            
            // 移除所有窗口属性
            RemovePropW(hwnd, L"MessageText");
            RemovePropW(hwnd, L"AnimState");
            RemovePropW(hwnd, L"Opacity");
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

/**
 * @brief 绘制带边框的矩形
 * @param hdc 设备上下文
 * @param rect 矩形区域
 * @param radius 圆角半径(未使用)
 * 
 * 在指定的设备上下文中绘制带浅灰色边框的矩形。
 * 该函数预留了圆角半径参数，但当前实现使用标准矩形。
 * 可在未来版本中扩展支持真正的圆角矩形。
 */
void DrawRoundedRectangle(HDC hdc, RECT rect, int radius) {
    // 创建浅灰色边框画笔
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    
    // 使用普通矩形替代圆角矩形
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    
    // 清理资源
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
