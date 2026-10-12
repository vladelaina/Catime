#include "pomodoro_phase_color_palette.h"
#include <string.h>
#include "color/color_state.h"
#include "pomodoro_phase_hook_config.h"

static POMODORO_PHASE g_colorPhase = POMODORO_PHASE_IDLE;
static char g_phaseColor[COLOR_HEX_BUFFER] = "";

BOOL SetPomodoroPhaseColor(POMODORO_PHASE phase) {
    PomodoroPhaseHookConfig config;
    if (!LoadPomodoroPhaseHookConfig(phase, &config)) {
        return FALSE;
    }
    strcpy_s(g_phaseColor, sizeof(g_phaseColor), config.color);
    g_colorPhase = phase;
    return TRUE;
}

BOOL GetPomodoroDisplayColor(char* output, size_t outputSize) {
    if (!output || outputSize == 0 || current_pomodoro_phase == POMODORO_PHASE_IDLE ||
        current_pomodoro_phase != g_colorPhase) {
        return FALSE;
    }
    const char* color = _stricmp(g_phaseColor, POMODORO_HOOK_DEFAULT) == 0
        ? CLOCK_TEXT_COLOR : g_phaseColor;
    return strncpy_s(output, outputSize, color, _TRUNCATE) == 0;
}

void ClearPomodoroPhaseColor(void) {
    g_colorPhase = POMODORO_PHASE_IDLE;
    g_phaseColor[0] = '\0';
}
