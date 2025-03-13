/**
 * @file window.h
 * @brief 窗口管理功能接口
 * 
 * 本文件定义了应用程序窗口管理相关的常量和函数接口，
 * 包括窗口创建、位置调整、透明度、点击穿透和拖拽功能。
 */

#ifndef WINDOW_H
#define WINDOW_H

#include <windows.h>
#include <dwmapi.h>

/// @name 窗口尺寸和位置常量
/// @{
extern int CLOCK_BASE_WINDOW_WIDTH;    ///< 基础窗口宽度
extern int CLOCK_BASE_WINDOW_HEIGHT;   ///< 基础窗口高度
extern float CLOCK_WINDOW_SCALE;       ///< 窗口缩放比例
extern int CLOCK_WINDOW_POS_X;         ///< 窗口X坐标
extern int CLOCK_WINDOW_POS_Y;         ///< 窗口Y坐标
/// @}

/// @name 窗口状态变量
/// @{
extern BOOL CLOCK_EDIT_MODE;           ///< 是否处于编辑模式
extern BOOL CLOCK_IS_DRAGGING;         ///< 是否正在拖拽窗口
extern POINT CLOCK_LAST_MOUSE_POS;     ///< 上次鼠标位置
extern BOOL CLOCK_WINDOW_TOPMOST;       ///< 窗口是否置顶
/// @}

/// @name 文本区域变量
/// @{
extern RECT CLOCK_TEXT_RECT;           ///< 文本区域矩形
extern BOOL CLOCK_TEXT_RECT_VALID;     ///< 文本区域是否有效
/// @}

/// @name 缩放限制常量
/// @{
#define MIN_SCALE_FACTOR 0.5f          ///< 最小缩放比例
#define MAX_SCALE_FACTOR 100.0f        ///< 最大缩放比例
/// @}

/// @name 字体相关常量
/// @{
extern float CLOCK_FONT_SCALE_FACTOR;    ///< 字体缩放比例
extern int CLOCK_BASE_FONT_SIZE;         ///< 基础字体大小 
/// @}

/**
 * @brief 设置窗口点击穿透属性
 * @param hwnd 窗口句柄
 * @param enable 是否启用点击穿透
 * 
 * 控制窗口是否让鼠标点击事件穿透到下层窗口。
 * 在启用时，窗口将不接收任何鼠标事件。
 */
void SetClickThrough(HWND hwnd, BOOL enable);

/**
 * @brief 设置窗口背景模糊效果
 * @param hwnd 窗口句柄
 * @param enable 是否启用模糊效果
 * 
 * 控制窗口背景是否应用DWM模糊效果，
 * 使背景半透明并具有磨砂玻璃效果。
 */
void SetBlurBehind(HWND hwnd, BOOL enable);

/**
 * @brief 调整窗口位置
 * @param hwnd 窗口句柄
 * @param forceOnScreen 是否强制窗口在屏幕内
 * 
 * 当forceOnScreen为TRUE时，确保窗口位置在屏幕边界内，
 * 当窗口位置超出边界时自动调整到合适位置。
 * 在编辑模式下可以将forceOnScreen设置为FALSE，允许窗口拖出屏幕。
 */
void AdjustWindowPosition(HWND hwnd, BOOL forceOnScreen);

/**
 * @brief 保存窗口设置
 * @param hwnd 窗口句柄
 * 
 * 将窗口的当前位置、大小和缩放状态保存到配置文件。
 */
void SaveWindowSettings(HWND hwnd);

/**
 * @brief 加载窗口设置
 * @param hwnd 窗口句柄
 * 
 * 从配置文件加载窗口的位置、大小和缩放状态，
 * 并应用到指定窗口。
 */
void LoadWindowSettings(HWND hwnd);

/**
 * @brief 初始化DWM模糊功能
 * @return BOOL 初始化是否成功
 * 
 * 加载并初始化DWM API函数指针，用于窗口模糊效果。
 */
BOOL InitDWMFunctions(void);

/**
 * @brief 处理窗口鼠标滚轮消息
 * @param hwnd 窗口句柄
 * @param delta 滚轮滚动量
 * @return BOOL 是否处理了消息
 * 
 * 处理鼠标滚轮事件，在编辑模式下调整窗口大小。
 */
BOOL HandleMouseWheel(HWND hwnd, int delta);

/**
 * @brief 处理窗口鼠标移动消息
 * @param hwnd 窗口句柄
 * @return BOOL 是否处理了消息
 * 
 * 处理鼠标移动事件，在编辑模式下拖拽窗口。
 */
BOOL HandleMouseMove(HWND hwnd);

/**
 * @brief 创建并初始化主窗口
 * @param hInstance 应用实例句柄
 * @param nCmdShow 显示命令参数
 * @return HWND 创建的窗口句柄
 * 
 * 创建应用程序的主窗口，并设置初始属性。
 */
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);

/**
 * @brief 初始化应用程序
 * @param hInstance 应用程序实例句柄
 * @return BOOL 初始化是否成功
 * 
 * 执行应用程序启动时的初始化工作，包括设置控制台代码页、
 * 初始化语言、更新自启动快捷方式、读取配置文件和加载字体资源。
 */
BOOL InitializeApplication(HINSTANCE hInstance);

/**
 * @brief 打开文件选择对话框
 * @param hwnd 父窗口句柄
 * @param filePath 存储选中文件路径的缓冲区
 * @param maxPath 缓冲区最大长度
 * @return BOOL 是否成功选择文件
 */
BOOL OpenFileDialog(HWND hwnd, char* filePath, DWORD maxPath);

/**
 * @brief 设置窗口置顶状态
 * @param hwnd 窗口句柄
 * @param topmost 是否置顶
 * 
 * 控制窗口是否始终显示在其他窗口之上。
 * 同时更新全局状态变量并保存配置。
 */
void SetWindowTopmost(HWND hwnd, BOOL topmost);

#endif // WINDOW_H
