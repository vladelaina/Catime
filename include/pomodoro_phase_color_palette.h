#ifndef POMODORO_PHASE_COLOR_PALETTE_H
#define POMODORO_PHASE_COLOR_PALETTE_H

#include <stddef.h>
#include <windows.h>
#include "pomodoro.h"

/* Phase colors affect display only; CLOCK_TEXT_COLOR remains the user's setting. */
BOOL SetPomodoroPhaseColor(POMODORO_PHASE phase);
BOOL GetPomodoroDisplayColor(char* output, size_t outputSize);
void ClearPomodoroPhaseColor(void);

#endif /* POMODORO_PHASE_COLOR_PALETTE_H */
