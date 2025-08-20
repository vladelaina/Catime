/**
 * @file pomodoro.h
 * @brief Pomodoro timer functionality
 * 
 * Defines the Pomodoro technique timer phases and state management
 */

#ifndef POMODORO_H
#define POMODORO_H

/**
 * @brief Pomodoro timer phases
 */
typedef enum {
    POMODORO_PHASE_IDLE = 0,        /**< No active Pomodoro session */
    POMODORO_PHASE_WORK,           /**< Work phase (typically 25 minutes) */
    POMODORO_PHASE_BREAK,          /**< Short break (typically 5 minutes) */
    POMODORO_PHASE_LONG_BREAK      /**< Long break (typically 15-30 minutes) */
} POMODORO_PHASE;

/** @brief Current Pomodoro phase state */
extern POMODORO_PHASE current_pomodoro_phase;

/** @brief Index of current time configuration being used */
extern int current_pomodoro_time_index;

/** @brief Number of completed Pomodoro work cycles */
extern int complete_pomodoro_cycles;

/**
 * @brief Initialize Pomodoro timer system
 * 
 * Sets up initial state for new Pomodoro session
 */
void InitializePomodoro(void);

#endif