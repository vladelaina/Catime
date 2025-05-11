/**
 * @file window_procedure.h
 * @brief 窗口消息处理过程接口
 * 
 * 本文件定义了应用程序的主窗口消息处理回调函数接口，
 * 处理窗口的所有消息事件包括绘制、鼠标、键盘、菜单和定时器事件。
 */

#ifndef WINDOW_PROCEDURE_H
#define WINDOW_PROCEDURE_H

#include <windows.h>

/**
 * @brief 热键ID定义
 */
#define HOTKEY_ID_SHOW_TIME       100  // 显示当前时间热键ID
#define HOTKEY_ID_COUNT_UP        101  // 正计时热键ID
#define HOTKEY_ID_COUNTDOWN       102  // 倒计时热键ID
#define HOTKEY_ID_POMODORO        103  // 番茄钟热键ID
#define HOTKEY_ID_TOGGLE_VISIBILITY 104 // 隐藏/显示热键ID
#define HOTKEY_ID_EDIT_MODE       105  // 编辑模式热键ID
#define HOTKEY_ID_PAUSE_RESUME    106  // 暂停/继续热键ID
#define HOTKEY_ID_RESTART_TIMER   107  // 重新开始热键ID

/**
 * @brief 主窗口消息处理函数
 * @param hwnd 窗口句柄
 * @param msg 消息ID
 * @param wp 消息参数
 * @param lp 消息参数
 * @return LRESULT 消息处理结果
 * 
 * 处理应用程序主窗口的所有窗口消息，包括：
 * - 创建和销毁事件
 * - 绘制和重绘
 * - 鼠标和键盘输入
 * - 窗口位置和大小变化
 * - 托盘图标事件
 * - 菜单命令消息
 * - 定时器事件
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/**
 * @brief 注册全局热键
 * @param hwnd 窗口句柄
 * 
 * 从配置文件读取并注册全局热键设置，用于快速切换显示当前时间、正计时和默认倒计时。
 * 如果热键已注册，会先取消注册再重新注册。
 * 
 * @return BOOL 是否成功注册至少一个热键
 */
BOOL RegisterGlobalHotkeys(HWND hwnd);

/**
 * @brief 取消注册全局热键
 * @param hwnd 窗口句柄
 * 
 * 取消注册所有已注册的全局热键。
 */
void UnregisterGlobalHotkeys(HWND hwnd);

/**
 * @brief 切换到显示当前时间模式
 * @param hwnd 窗口句柄
 */
void ToggleShowTimeMode(HWND hwnd);

/**
 * @brief 开始正计时
 * @param hwnd 窗口句柄
 */
void StartCountUp(HWND hwnd);

/**
 * @brief 开始默认倒计时
 * @param hwnd 窗口句柄
 */
void StartDefaultCountDown(HWND hwnd);

/**
 * @brief 开始番茄钟
 * @param hwnd 窗口句柄
 */
void StartPomodoroTimer(HWND hwnd);

/**
 * @brief 切换编辑模式
 * @param hwnd 窗口句柄
 */
void ToggleEditMode(HWND hwnd);

/**
 * @brief 暂停/继续计时
 * @param hwnd 窗口句柄
 */
void TogglePauseResume(HWND hwnd);

/**
 * @brief 重新开始当前计时
 * @param hwnd 窗口句柄
 */
void RestartCurrentTimer(HWND hwnd);

#endif // WINDOW_PROCEDURE_H 