#include "pomodoro_phase_action.h"
#include <ctype.h>
#include <string.h>

extern const PomodoroPhaseActionDefinition POMODORO_PHASE_ACTION_COLOR;
extern const PomodoroPhaseActionDefinition POMODORO_PHASE_ACTION_SOUND;

static const PomodoroPhaseActionDefinition* const POMODORO_PHASE_ACTION_DEFINITIONS[] = {
    &POMODORO_PHASE_ACTION_COLOR,
    &POMODORO_PHASE_ACTION_SOUND
};

size_t PomodoroPhaseAction_GetCount(void) {
    return _countof(POMODORO_PHASE_ACTION_DEFINITIONS);
}

const PomodoroPhaseActionDefinition* PomodoroPhaseAction_GetByIndex(size_t index) {
    if (index >= PomodoroPhaseAction_GetCount()) {
        return NULL;
    }
    return POMODORO_PHASE_ACTION_DEFINITIONS[index];
}

static BOOL TokenEquals(const char* token, const char* expected) {
    if (!token || !expected) {
        return FALSE;
    }

    while (*token && isspace((unsigned char)*token)) {
        token++;
    }
    size_t tokenLength = strlen(token);
    while (tokenLength > 0 && isspace((unsigned char)token[tokenLength - 1])) {
        tokenLength--;
    }

    return strlen(expected) == tokenLength && _strnicmp(expected, token, tokenLength) == 0;
}

const PomodoroPhaseActionDefinition* PomodoroPhaseAction_GetByToken(const char* token) {
    for (size_t i = 0; i < PomodoroPhaseAction_GetCount(); ++i) {
        const PomodoroPhaseActionDefinition* action = POMODORO_PHASE_ACTION_DEFINITIONS[i];
        if (action && TokenEquals(token, action->token)) {
            return action;
        }
    }

    return NULL;
}

BOOL PomodoroPhaseAction_NormalizeChain(const char* actionChain, char* output, size_t outputSize) {
    char actions[256];
    BOOL seen[_countof(POMODORO_PHASE_ACTION_DEFINITIONS)] = {FALSE};
    if (!actionChain || !output || outputSize == 0 ||
        strncpy_s(actions, sizeof(actions), actionChain, _TRUNCATE) != 0) {
        return FALSE;
    }

    /* Copy before clearing output so configuration fields can normalize in place. */
    output[0] = '\0';
    char* context = NULL;
    char* token = strtok_s(actions, ",|", &context);
    while (token) {
        const PomodoroPhaseActionDefinition* action = PomodoroPhaseAction_GetByToken(token);
        /* Older phase-specific colors now share one stage-aware COLOR action. */
        if (!action && (TokenEquals(token, "COLOR_DEFAULT") ||
                        TokenEquals(token, "COLOR_LIGHT_GREEN") ||
                        TokenEquals(token, "COLOR_GREEN"))) {
            action = &POMODORO_PHASE_ACTION_COLOR;
        }

        if (action) {
            size_t index = 0;
            while (index < PomodoroPhaseAction_GetCount() &&
                   POMODORO_PHASE_ACTION_DEFINITIONS[index] != action) {
                index++;
            }
            if (index < PomodoroPhaseAction_GetCount() && !seen[index]) {
                size_t used = strlen(output);
                size_t length = strlen(action->token);
                if (used + (used > 0 ? 1 : 0) + length >= outputSize) {
                    return FALSE;
                }
                if (used > 0) output[used++] = ',';
                memcpy(output + used, action->token, length + 1);
                seen[index] = TRUE;
            }
        }
        token = strtok_s(NULL, ",|", &context);
    }
    return TRUE;
}

UINT PomodoroPhaseAction_MakeMenuId(int phaseIndex, size_t actionIndex) {
    if (phaseIndex < 0 || phaseIndex >= CLOCK_IDM_POMODORO_PHASE_ACTION_PHASE_COUNT) {
        return 0;
    }
    if (actionIndex >= PomodoroPhaseAction_GetCount() ||
        actionIndex >= CLOCK_IDM_POMODORO_PHASE_ACTION_MENU_STRIDE) {
        return 0;
    }

    return CLOCK_IDM_POMODORO_PHASE_ACTION_BASE +
           (UINT)phaseIndex * CLOCK_IDM_POMODORO_PHASE_ACTION_MENU_STRIDE +
           (UINT)actionIndex;
}

BOOL PomodoroPhaseAction_ParseMenuId(UINT menuId,
                                     int* phaseIndex,
                                     const PomodoroPhaseActionDefinition** action) {
    if (menuId < CLOCK_IDM_POMODORO_PHASE_ACTION_BASE) {
        return FALSE;
    }

    UINT offset = menuId - CLOCK_IDM_POMODORO_PHASE_ACTION_BASE;
    UINT maxRange = CLOCK_IDM_POMODORO_PHASE_ACTION_RANGE;
    if (offset >= maxRange) {
        return FALSE;
    }

    UINT parsedPhaseIndex = offset / CLOCK_IDM_POMODORO_PHASE_ACTION_MENU_STRIDE;
    UINT actionIndex = offset % CLOCK_IDM_POMODORO_PHASE_ACTION_MENU_STRIDE;
    if (parsedPhaseIndex >= CLOCK_IDM_POMODORO_PHASE_ACTION_PHASE_COUNT ||
        actionIndex >= PomodoroPhaseAction_GetCount()) {
        return FALSE;
    }

    if (phaseIndex) {
        *phaseIndex = (int)parsedPhaseIndex;
    }
    if (action) {
        *action = PomodoroPhaseAction_GetByIndex(actionIndex);
    }

    return TRUE;
}

BOOL PomodoroPhaseAction_IsMenuId(UINT menuId) {
    return PomodoroPhaseAction_ParseMenuId(menuId, NULL, NULL);
}

BOOL PomodoroPhaseAction_ExecuteByToken(HWND hwnd, POMODORO_PHASE phase, const char* token) {
    const PomodoroPhaseActionDefinition* action = PomodoroPhaseAction_GetByToken(token);
    if (!action || !action->execute) {
        return FALSE;
    }

    return action->execute(hwnd, phase);
}
