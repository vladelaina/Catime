/**
 * @file media.c
 * @brief 媒体控制功能实现
 * 
 * 本文件实现了应用程序的媒体控制相关功能，
 * 包括暂停、播放等媒体控制操作。
 */

#include <windows.h>
#include "../include/media.h"

/**
 * @brief 暂停媒体播放
 * 
 * 通过模拟媒体控制键的按键事件来暂停当前正在播放的媒体。
 * 包括停止和暂停/播放的组合操作，以确保媒体被正确暂停。
 */
void PauseMediaPlayback(void) {
    keybd_event(VK_MEDIA_STOP, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);

    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);

    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(100);
}