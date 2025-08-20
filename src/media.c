/**
 * @file media.c
 * @brief System-wide media playback control via virtual key events
 */
#include <windows.h>
#include "../include/media.h"

/**
 * @brief Force pause of any active media playback across the system
 * Uses aggressive stop-then-pause sequence to handle stubborn media players
 */
void PauseMediaPlayback(void) {
    /** Send STOP first to ensure media is fully stopped */
    keybd_event(VK_MEDIA_STOP, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_STOP, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);

    /** Double PLAY_PAUSE sequence for compatibility with various media players */
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(50);

    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
    Sleep(50);
    keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
    Sleep(100);
}