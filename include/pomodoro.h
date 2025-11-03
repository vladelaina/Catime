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
    POMODORO_PHASE_WORK,           /**< Typically 25min */
    POMODORO_PHASE_BREAK,          /**< Short break, typically 5min */
    POMODORO_PHASE_LONG_BREAK      /**< Long break, typically 15-30min */
} POMODORO_PHASE;

extern POMODORO_PHASE current_pomodoro_phase;
extern int current_pomodoro_time_index;
extern int complete_pomodoro_cycles;     /**< For long break calculation */

extern int POMODORO_TIMES[10];           /**< Up to 10 custom intervals */
extern int POMODORO_TIMES_COUNT;

/**
 * @note InitializePomodoro() declared in timer_events.h
 */

#endif