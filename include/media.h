/**
 * @file media.h
 * @brief Media control functionality interface
 * 
 * This file defines the application's media control related function interfaces,
 * including pause, play and other media control operations.
 */

#ifndef CLOCK_MEDIA_H
#define CLOCK_MEDIA_H

#include <windows.h>

/**
 * @brief Pause media playback
 * 
 * Pauses currently playing media by simulating media control key press events.
 * Includes a combination of stop and pause/play operations to ensure the media is properly paused.
 */
void PauseMediaPlayback(void);

#endif // CLOCK_MEDIA_H