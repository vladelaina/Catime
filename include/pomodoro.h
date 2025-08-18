#ifndef POMODORO_H
#define POMODORO_H

typedef enum {
    POMODORO_PHASE_IDLE = 0,
    POMODORO_PHASE_WORK,
    POMODORO_PHASE_BREAK,
    POMODORO_PHASE_LONG_BREAK
} POMODORO_PHASE;

extern POMODORO_PHASE current_pomodoro_phase;

extern int current_pomodoro_time_index;

extern int complete_pomodoro_cycles;

void InitializePomodoro(void);

#endif