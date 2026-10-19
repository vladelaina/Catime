#include "window_procedure/window_commands.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_utils.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_procedure.h"
#include "config.h"
#include "pomodoro_phase_action.h"
#include "utils/string_convert.h"
#include <stdio.h>
#include <string.h>

static LRESULT ShowPomodoroPhaseActionDialog(HWND hwnd,
                                             const wchar_t* title,
                                             const wchar_t* prompt,
                                             const char* configKey,
                                             const char* defaultActions) {
    wchar_t defaultText[256] = {0};
    wchar_t result[256] = {0};

    if (defaultActions && defaultActions[0] != '\0') {
        Utf8ToWide(defaultActions, defaultText, _countof(defaultText));
    }

    if (!InputBox(hwnd, title, prompt, defaultText, result, _countof(result))) {
        return 0;
    }

    char actions[256] = {0};
    if (!WideToUtf8(result, actions, sizeof(actions))) {
        return 0;
    }

    if (!WriteConfigPomodoroPhaseActionKey(configKey, actions)) {
        return 0;
    }

    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
}

static BOOL AppendPomodoroActionToken(char* output, size_t outputSize, const char* token) {
    if (!output || !token || token[0] == '\0') {
        return FALSE;
    }

    size_t currentLen = strlen(output);
    int written = _snprintf_s(output + currentLen, outputSize - currentLen, _TRUNCATE,
                              "%s%s", currentLen > 0 ? "," : "", token);
    return written >= 0;
}

static BOOL BuildUpdatedPomodoroActionChain(const char* currentChain,
                                            const char* selectedToken,
                                            char* output,
                                            size_t outputSize) {
    const PomodoroPhaseActionDefinition* selectedAction =
        PomodoroPhaseAction_GetByToken(selectedToken);
    if (!selectedAction || !output || outputSize == 0) {
        return FALSE;
    }

    BOOL tokenWasPresent = FALSE;

    char actions[256];
    if (!PomodoroPhaseAction_NormalizeChain(currentChain ? currentChain : "",
                                            actions, sizeof(actions))) {
        return FALSE;
    }
    output[0] = '\0';

    char* context = NULL;
    char* current = strtok_s(actions, ",|", &context);
    while (current) {
        if (strcmp(current, selectedAction->token) == 0) {
            tokenWasPresent = TRUE;
            current = strtok_s(NULL, ",|", &context);
            continue;
        }

        if (!AppendPomodoroActionToken(output, outputSize, current)) {
            return FALSE;
        }

        current = strtok_s(NULL, ",|", &context);
    }

    if (!tokenWasPresent && !AppendPomodoroActionToken(output, outputSize, selectedAction->token)) {
        return FALSE;
    }

    return TRUE;
}

static const char* GetPomodoroPhaseActionsConfigKey(int phaseIndex) {
    switch (phaseIndex) {
        case 0:
            return CFG_KEY_POMODORO_WORK_ACTIONS;
        case 1:
            return CFG_KEY_POMODORO_SHORT_BREAK_ACTIONS;
        case 2:
            return CFG_KEY_POMODORO_LONG_BREAK_ACTIONS;
        default:
            return NULL;
    }
}

static const char* GetPomodoroPhaseActionsChain(int phaseIndex) {
    switch (phaseIndex) {
        case 0:
            return g_AppConfig.pomodoro.work_actions;
        case 1:
            return g_AppConfig.pomodoro.short_break_actions;
        case 2:
            return g_AppConfig.pomodoro.long_break_actions;
        default:
            return NULL;
    }
}

BOOL HandlePomodoroPhaseActionSelection(HWND hwnd, UINT cmd, int index) {
    (void)index;

    int phaseIndex = -1;
    const PomodoroPhaseActionDefinition* action = NULL;
    if (!PomodoroPhaseAction_ParseMenuId(cmd, &phaseIndex, &action) ||
        !action || !action->token) {
        return FALSE;
    }
    const char* configKey = GetPomodoroPhaseActionsConfigKey(phaseIndex);
    const char* currentChain = GetPomodoroPhaseActionsChain(phaseIndex);
    if (!configKey || !currentChain) {
        return FALSE;
    }

    char updatedActions[256];
    if (!BuildUpdatedPomodoroActionChain(currentChain,
                                         action->token,
                                         updatedActions,
                                         sizeof(updatedActions))) {
        return FALSE;
    }

    if (!WriteConfigPomodoroPhaseActionKey(configKey, updatedActions)) {
        return FALSE;
    }

    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

LRESULT CmdPomodoroWorkActions(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return ShowPomodoroPhaseActionDialog(
        hwnd,
        GetLocalizedString(NULL, L"Set Work Actions"),
        GetLocalizedString(NULL, L"PomodoroActionInputHelp"),
        CFG_KEY_POMODORO_WORK_ACTIONS,
        g_AppConfig.pomodoro.work_actions
    );
}

LRESULT CmdPomodoroBreakActions(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return ShowPomodoroPhaseActionDialog(
        hwnd,
        GetLocalizedString(NULL, L"Set Short Break Actions"),
        GetLocalizedString(NULL, L"PomodoroActionInputHelp"),
        CFG_KEY_POMODORO_SHORT_BREAK_ACTIONS,
        g_AppConfig.pomodoro.short_break_actions
    );
}

LRESULT CmdPomodoroLongBreakActions(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    return ShowPomodoroPhaseActionDialog(
        hwnd,
        GetLocalizedString(NULL, L"Set Long Break Actions"),
        GetLocalizedString(NULL, L"PomodoroActionInputHelp"),
        CFG_KEY_POMODORO_LONG_BREAK_ACTIONS,
        g_AppConfig.pomodoro.long_break_actions
    );
}
