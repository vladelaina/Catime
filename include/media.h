/**
 * @file media.h
 * @brief System media playback control
 * 
 * Functions for controlling system media playback during timer events
 */

#ifndef CLOCK_MEDIA_H
#define CLOCK_MEDIA_H

#include <windows.h>

/**
 * @brief Pause current system media playback
 * 
 * Sends pause command to active media players when timer starts
 */
void PauseMediaPlayback(void);

#endif