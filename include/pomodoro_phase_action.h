#ifndef POMODORO_PHASE_ACTION_H
#define POMODORO_PHASE_ACTION_H

#include <stddef.h>
#include <windows.h>
#include "../resource/resource.h"
#include "pomodoro.h"

#define POMODORO_ACTION_CHAIN_BUFFER 256

typedef BOOL (*PomodoroPhaseActionExecuteFn)(HWND hwnd, POMODORO_PHASE phase);

typedef struct {
    const char* token;
    const wchar_t* labelKey;
    PomodoroPhaseActionExecuteFn execute;
} PomodoroPhaseActionDefinition;

size_t PomodoroPhaseAction_GetCount(void);
const PomodoroPhaseActionDefinition* PomodoroPhaseAction_GetByIndex(size_t index);
const PomodoroPhaseActionDefinition* PomodoroPhaseAction_GetByToken(const char* token);
BOOL PomodoroPhaseAction_NormalizeChain(const char* actionChain, char* output, size_t outputSize);
BOOL PomodoroPhaseAction_ParseMenuId(UINT menuId,
                                     int* phaseIndex,
                                     const PomodoroPhaseActionDefinition** action);
UINT PomodoroPhaseAction_MakeMenuId(int phaseIndex, size_t actionIndex);
BOOL PomodoroPhaseAction_IsMenuId(UINT menuId);
BOOL PomodoroPhaseAction_ExecuteByToken(HWND hwnd, POMODORO_PHASE phase, const char* token);

#endif /* POMODORO_PHASE_ACTION_H */
