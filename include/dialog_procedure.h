/**
 * @file dialog_procedure.h
 * @brief 对话框消息处理过程接口
 * 
 * 本文件定义了应用程序的对话框消息处理回调函数接口，
 * 处理对话框的所有消息事件包括初始化、颜色管理、按钮点击和键盘事件。
 */

#ifndef DIALOG_PROCEDURE_H
#define DIALOG_PROCEDURE_H

#include <windows.h>

/**
 * @brief 输入对话框过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 处理倒计时输入对话框的：
 * 1. 控件初始化与焦点设置
 * 2. 背景/控件颜色管理
 * 3. 确定按钮点击处理
 * 4. 回车键响应
 * 5. 资源清理
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 显示关于对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示包含程序版本、作者和第三方库信息的关于对话框。
 * 包含以下链接按钮：
 * - 鸣谢 (IDC_CREDITS) - 打开网页 https://vladelaina.github.io/Catime/#thanks
 * - 反馈 (IDC_FEEDBACK)
 * - GitHub (IDC_GITHUB)
 * - 版权声明 (IDC_COPYRIGHT_LINK) - 打开网页 https://github.com/vladelaina/Catime?tab=readme-ov-file#%EF%B8%8F%E7%89%88%E6%9D%83%E5%A3%B0%E6%98%8E
 * - 支持 (IDC_SUPPORT) - 打开网页 https://vladelaina.github.io/Catime/support.html
 */
void ShowAboutDialog(HWND hwndParent);

INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 显示错误对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示统一的错误提示对话框。
 */
void ShowErrorDialog(HWND hwndParent);

/**
 * @brief 显示番茄钟循环次数设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示用于设置番茄钟循环次数的对话框。
 * 允许用户输入1-99之间的循环次数。
 */
void ShowPomodoroLoopDialog(HWND hwndParent);

INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 显示网站URL输入对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示用于输入超时时打开的网站URL的对话框。
 */
void ShowWebsiteDialog(HWND hwndParent);

/**
 * @brief 显示番茄钟组合对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示用于设置番茄钟时间组合的对话框。
 * 允许用户输入自定义的番茄钟时间序列。
 */
void ShowPomodoroComboDialog(HWND hwndParent);

/**
 * @brief 解析时间输入
 * @param input 输入字符串 (如 "25m", "30s", "1h30m")
 * @param seconds 输出秒数
 * @return BOOL 解析成功返回TRUE，失败返回FALSE
 */
BOOL ParseTimeInput(const char* input, int* seconds);

/**
 * @brief 显示通知消息设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示通知消息设置对话框，用于修改各种通知提示文本。
 */
void ShowNotificationMessagesDialog(HWND hwndParent);

/**
 * @brief 显示通知显示设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示通知显示设置对话框，用于修改通知显示时间和透明度。
 */
void ShowNotificationDisplayDialog(HWND hwndParent);

/**
 * @brief 显示整合后的通知设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示同时包含通知内容和通知显示设置的整合对话框。
 * 与之前的两个单独对话框不同，这个对话框整合了所有通知相关设置。
 */
void ShowNotificationSettingsDialog(HWND hwndParent);

/**
 * @brief 全局倒计时输入对话框句柄
 * 
 * 用于跟踪当前显示的倒计时输入对话框。
 * 如果为NULL，表示没有输入对话框正在显示。
 */
extern HWND g_hwndInputDialog;

#endif // DIALOG_PROCEDURE_H