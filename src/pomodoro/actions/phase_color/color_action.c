#include "pomodoro_phase_action.h"
#include "pomodoro_phase_color_palette.h"

static BOOL ExecuteColor(HWND hwnd, POMODORO_PHASE phase) {
    (void)hwnd;
    return SetPomodoroPhaseColor(phase);
}

const PomodoroPhaseActionDefinition POMODORO_PHASE_ACTION_COLOR = {
    "COLOR",
    L"Color",
    ExecuteColor
};
