/**
 * @file media.h
 * @brief 媒体控制功能接口
 * 
 * 本文件定义了应用程序媒体控制相关的函数接口，
 * 包括暂停、播放等媒体控制操作。
 */

#ifndef CLOCK_MEDIA_H
#define CLOCK_MEDIA_H

#include <windows.h>

/**
 * @brief 暂停媒体播放
 * 
 * 通过模拟媒体控制键的按键事件来暂停当前正在播放的媒体。
 * 包括停止和暂停/播放的组合操作，以确保媒体被正确暂停。
 */
void PauseMediaPlayback(void);

#endif // CLOCK_MEDIA_H