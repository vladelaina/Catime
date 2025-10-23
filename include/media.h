/**
 * @file media.h
 * @brief System-wide media playback control interface
 * @version 2.0 - Enhanced documentation and clarity
 * 
 * Provides functions for controlling system-wide media playback through
 * Windows virtual key events. Primarily used to pause active media players
 * when timer events occur (e.g., countdown start, Pomodoro timer activation).
 * 
 * Implementation uses standardized media control keys (VK_MEDIA_*) which
 * are supported by most modern Windows media players and applications.
 */

#ifndef CLOCK_MEDIA_H
#define CLOCK_MEDIA_H

#include <windows.h>

/**
 * @brief Force pause of any active media playback across the system
 * 
 * Sends a standardized sequence of media control keys to pause currently
 * playing media on the system. Uses an aggressive two-stage approach:
 * 
 * 1. Sends MEDIA_STOP to fully stop playback
 * 2. Sends MEDIA_PLAY_PAUSE multiple times for stubborn players
 * 
 * This function is designed to work with a wide variety of media players
 * including Windows Media Player, Spotify, iTunes, VLC, and web browsers
 * with media playback (YouTube, Netflix, etc.).
 * 
 * @note This function uses simulated keyboard events and includes delays
 *       for reliability. Total execution time is approximately 300ms.
 * 
 * @note No return value as media player response cannot be reliably verified.
 *       The function always attempts to send the key sequence regardless of
 *       whether any media is currently playing.
 * 
 * @warning This function is blocking and should not be called from
 *          time-critical code paths or UI update loops.
 * 
 * @see SendMediaKey() in media.c for the underlying key event implementation
 */
void PauseMediaPlayback(void);

#endif