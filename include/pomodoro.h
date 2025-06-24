/**
 * @file pomodoro.h
 * @brief Pomodoro timer related definitions
 * 
 * Defines Pomodoro timer phases and shared variables
 */

#ifndef POMODORO_H
#define POMODORO_H

// Pomodoro phase enumeration
typedef enum {
    POMODORO_PHASE_IDLE = 0,  // Idle state
    POMODORO_PHASE_WORK,      // Work phase
    POMODORO_PHASE_BREAK,     // Break phase
    POMODORO_PHASE_LONG_BREAK // Long break phase
} POMODORO_PHASE;

// Current Pomodoro phase
extern POMODORO_PHASE current_pomodoro_phase;

// Current Pomodoro time index being executed
extern int current_pomodoro_time_index;

// Number of completed Pomodoro cycles
extern int complete_pomodoro_cycles;

// Initialize Pomodoro state to work phase
void InitializePomodoro(void);

#endif // POMODORO_H
