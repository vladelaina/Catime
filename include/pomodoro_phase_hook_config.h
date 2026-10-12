#ifndef POMODORO_PHASE_HOOK_CONFIG_H
#define POMODORO_PHASE_HOOK_CONFIG_H

#include <stddef.h>
#include <windows.h>
#include "pomodoro.h"
#include "color/color_parser.h"

#define POMODORO_HOOK_DEFAULT "DEFAULT"
#define POMODORO_HOOK_DEFAULT_SOUND "SYSTEM_BEEP"

typedef struct {
    char color[COLOR_HEX_BUFFER];
    char soundFile[MAX_PATH];
} PomodoroPhaseHookConfig;

BOOL GetPomodoroPhaseHookConfigPath(POMODORO_PHASE phase, char* output, size_t outputSize);
/* Read on phase entry. Missing/invalid values keep the corresponding phase defaults. */
BOOL LoadPomodoroPhaseHookConfig(POMODORO_PHASE phase, PomodoroPhaseHookConfig* config);

#endif /* POMODORO_PHASE_HOOK_CONFIG_H */
