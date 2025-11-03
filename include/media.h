/**
 * @file media.h
 * @brief System-wide media control via VK_MEDIA_* keys
 * 
 * Two-stage approach (STOP + repeated PLAY_PAUSE) handles stubborn players.
 * Works with most media players (WMP, Spotify, iTunes, VLC, browsers).
 */

#ifndef CLOCK_MEDIA_H
#define CLOCK_MEDIA_H

#include <windows.h>

/**
 * @brief Force pause all system media playback
 * 
 * @details
 * Sends MEDIA_STOP then repeated MEDIA_PLAY_PAUSE via simulated keyboard
 * events. Execution time ~300ms (includes delays for reliability).
 * No return value (cannot verify player response).
 * 
 * @warning Blocking call - avoid in time-critical or UI update paths
 */
void PauseMediaPlayback(void);

#endif