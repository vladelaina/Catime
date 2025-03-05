/**
 * @file timer_events.h
 * @brief 计时器事件处理接口
 * 
 * 本文件定义了应用程序计时器事件处理相关的函数接口，
 * 包括倒计时和正计时模式的事件处理。
 */

#ifndef TIMER_EVENTS_H
#define TIMER_EVENTS_H

#include <windows.h>

/**
 * @brief 处理计时器消息
 * @param hwnd 窗口句柄
 * @param wp 消息参数
 * @return BOOL 是否处理了消息
 * 
 * 处理WM_TIMER消息，包括：
 * 1. 倒计时模式：更新剩余时间并执行超时动作
 * 2. 计时模式：累计已过时间
 * 3. 显示当前时间模式
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp);

#endif // TIMER_EVENTS_H