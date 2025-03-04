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

#endif // WINDOW_PROCEDURE_H 