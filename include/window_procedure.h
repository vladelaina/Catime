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
#define HOTKEY_ID_QUICK_COUNTDOWN1 103 // 快捷倒计时1热键ID
#define HOTKEY_ID_QUICK_COUNTDOWN2 104 // 快捷倒计时2热键ID
#define HOTKEY_ID_QUICK_COUNTDOWN3 105 // 快捷倒计时3热键ID
#define HOTKEY_ID_POMODORO        106  // 番茄钟热键ID
#define HOTKEY_ID_TOGGLE_VISIBILITY 107 // 隐藏/显示热键ID
#define HOTKEY_ID_EDIT_MODE       108  // 编辑模式热键ID
#define HOTKEY_ID_PAUSE_RESUME    109  // 暂停/继续热键ID
#define HOTKEY_ID_RESTART_TIMER   110  // 重新开始热键ID

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

/**
 * @brief 快捷倒计时1函数
 * @param hwnd 窗口句柄
 * 
 * 使用预设时间选项中的第一项启动倒计时
 */
void StartQuickCountdown1(HWND hwnd);

/**
 * @brief 快捷倒计时2函数
 * @param hwnd 窗口句柄
 * 
 * 使用预设时间选项中的第二项启动倒计时
 */
void StartQuickCountdown2(HWND hwnd);

/**
 * @brief 快捷倒计时3函数
 * @param hwnd 窗口句柄
 * 
 * 使用预设时间选项中的第三项启动倒计时
 */
void StartQuickCountdown3(HWND hwnd);

#endif // WINDOW_PROCEDURE_H 