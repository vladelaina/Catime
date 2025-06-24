/**
 * @file media.c
 * @brief Media control functionality implementation
 * 
 * This file implements the application's media control related functions,
 * including pause, play and other media control operations.
 */

#include <windows.h>
#include "../include/media.h"

/**
 * @brief Pause media playback
 * 
 * Pauses currently playing media by simulating media control key press events.
 * Includes a combination of stop and pause/play operations to ensure the media is properly paused.
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