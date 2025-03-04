/**
 * @file color.h
 * @brief 颜色处理功能接口
 * 
 * 本文件定义了应用程序的颜色处理功能接口，包括颜色结构体、
 * 全局变量声明以及颜色管理相关的函数声明。
 */

#ifndef COLOR_H
#define COLOR_H

#include <windows.h>

/**
 * @brief 预定义颜色结构体
 * 
 * 存储预定义的颜色十六进制值
 */
typedef struct {
    const char* hexColor;  ///< 十六进制颜色值，格式如 "#RRGGBB"
} PredefinedColor;

/**
 * @brief CSS颜色名称结构体
 * 
 * 存储CSS标准颜色名称与对应的十六进制值
 */
typedef struct {
    const char* name;     ///< 颜色名称，如 "red"
    const char* hex;      ///< 对应的十六进制值，如 "#FF0000"
} CSSColor;

/// @name 全局变量声明
/// @{
extern PredefinedColor* COLOR_OPTIONS;   ///< 预定义颜色选项数组
extern size_t COLOR_OPTIONS_COUNT;       ///< 预定义颜色选项数量
extern char PREVIEW_COLOR[10];           ///< 预览颜色值
extern BOOL IS_COLOR_PREVIEWING;         ///< 是否正在预览颜色
extern char CLOCK_TEXT_COLOR[10];        ///< 当前文本颜色值
/// @}

/// @name 颜色管理函数
/// @{

/**
 * @brief 初始化默认语言和颜色设置
 * 
 * 读取配置文件中的颜色设置，若未找到则创建默认配置。
 */
void InitializeDefaultLanguage(void);

/**
 * @brief 添加颜色选项
 * @param hexColor 颜色的十六进制值
 * 
 * 添加一个颜色选项到全局颜色选项数组中。
 */
void AddColorOption(const char* hexColor);

/**
 * @brief 清除所有颜色选项
 * 
 * 释放所有颜色选项占用的内存，并重置颜色选项计数。
 */
void ClearColorOptions(void);

/**
 * @brief 将颜色写入配置文件
 * @param color_input 要写入的颜色值
 * 
 * 将指定的颜色值写入应用程序配置文件。
 */
void WriteConfigColor(const char* color_input);

/**
 * @brief 标准化颜色格式
 * @param input 输入的颜色字符串
 * @param output 输出的标准化颜色字符串
 * @param output_size 输出缓冲区大小
 * 
 * 将各种格式的颜色字符串转换为标准的十六进制颜色格式。
 */
void normalizeColor(const char* input, char* output, size_t output_size);

/**
 * @brief 检查颜色是否有效
 * @param input 要检查的颜色字符串
 * @return BOOL 如果颜色有效则返回TRUE，否则返回FALSE
 * 
 * 验证给定的颜色表示是否可以被解析为有效的颜色。
 */
BOOL isValidColor(const char* input);

/**
 * @brief 颜色编辑框子类化处理过程
 * @param hwnd 窗口句柄
 * @param msg 消息ID
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return LRESULT 消息处理结果
 * 
 * 处理颜色编辑框的消息事件，实现颜色预览功能。
 */
LRESULT CALLBACK ColorEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 颜色设置对话框处理过程
 * @param hwndDlg 对话框窗口句柄
 * @param msg 消息ID
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 处理颜色设置对话框的消息事件。
 */
INT_PTR CALLBACK ColorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 检查颜色是否已存在
 * @param hexColor 十六进制颜色值
 * @return BOOL 如果颜色已存在则返回TRUE，否则返回FALSE
 * 
 * 检查指定的颜色是否已存在于颜色选项列表中。
 */
BOOL IsColorExists(const char* hexColor);

/**
 * @brief 显示颜色选择对话框
 * @param hwnd 父窗口句柄
 * @return COLORREF 选择的颜色值，如果用户取消则返回(COLORREF)-1
 * 
 * 显示Windows标准颜色选择对话框，允许用户选择颜色。
 */
COLORREF ShowColorDialog(HWND hwnd);

/**
 * @brief 颜色对话框钩子过程
 * @param hdlg 对话框窗口句柄
 * @param msg 消息ID
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return UINT_PTR 消息处理结果
 * 
 * 处理颜色选择对话框的自定义消息事件，实现颜色预览功能。
 */
UINT_PTR CALLBACK ColorDialogHookProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
/// @}

#endif // COLOR_H