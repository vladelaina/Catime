/**
 * @file drawing.h
 * @brief 窗口绘图功能接口
 * 
 * 本文件定义了应用程序窗口绘图相关的函数接口，
 * 包括文本渲染、颜色设置和窗口内容绘制等功能。
 */

#ifndef DRAWING_H
#define DRAWING_H

#include <windows.h>

/**
 * @brief 处理窗口绘制
 * @param hwnd 窗口句柄
 * @param ps 绘制结构体
 * 
 * 处理窗口的WM_PAINT消息，执行以下操作：
 * 1. 创建内存DC双缓冲防止闪烁
 * 2. 根据模式计算剩余时间/获取当前时间
 * 3. 动态加载字体资源(支持实时预览)
 * 4. 解析颜色配置(支持HEX/RGB格式)
 * 5. 使用双缓冲机制绘制文本
 * 6. 自动调整窗口尺寸适应文本内容
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps);

#endif // DRAWING_H