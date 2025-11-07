/**
 * @file pomodoro.h
 * @brief Pomodoro technique state tracking
 * 
 * Phase-based state machine enables automatic work/break transitions.
 * Cycle counter determines when to take long breaks vs short breaks.
 */

#ifndef POMODORO_H
#define POMODORO_H

/**
 * @brief Pomodoro phase states
 */
typedef enum {
    POMODORO_PHASE_IDLE = 0,
    POMODORO_PHASE_WORK,
    POMODORO_PHASE_BREAK,
    POMODORO_PHASE_LONG_BREAK
} POMODORO_PHASE;

extern POMODORO_PHASE current_pomodoro_phase;
extern int current_pomodoro_time_index;
extern int complete_pomodoro_cycles;

/**
 * @note InitializePomodoro() declared in timer_events.h
 * @note POMODORO_TIMES and related config now in g_AppConfig.pomodoro
 */

#endif