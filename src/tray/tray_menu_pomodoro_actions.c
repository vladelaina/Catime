#include "tray/tray_menu_pomodoro.h"
#include "config.h"
#include "language.h"
#include "pomodoro_phase_action.h"
#include <string.h>

typedef struct {
    const wchar_t* labelKey;
    const char* actionChain;
} PomodoroPhaseActionMenuEntry;

static BOOL ActionChainContainsToken(const char* actionChain, const char* token) {
    if (!actionChain || !token || token[0] == '\0') {
        return FALSE;
    }

    char actions[256];
    if (!PomodoroPhaseAction_NormalizeChain(actionChain, actions, sizeof(actions))) {
        return FALSE;
    }

    char* context = NULL;
    char* current = strtok_s(actions, ",|", &context);
    while (current) {
        if (_stricmp(current, token) == 0) {
            return TRUE;
        }
        current = strtok_s(NULL, ",|", &context);
    }

    return FALSE;
}

static HMENU BuildPomodoroPhaseActionSelectionMenu(int phaseIndex,
                                                   const char* actionChain) {
    HMENU hPhaseMenu = CreatePopupMenu();
    if (!hPhaseMenu) {
        return NULL;
    }

    size_t actionCount = PomodoroPhaseAction_GetCount();
    for (size_t i = 0; i < actionCount; ++i) {
        const PomodoroPhaseActionDefinition* action = PomodoroPhaseAction_GetByIndex(i);
        if (!action || !action->token) {
            continue;
        }

        UINT flags = MF_STRING;
        if (ActionChainContainsToken(actionChain, action->token)) {
            flags |= MF_CHECKED;
        }

        UINT menuId = PomodoroPhaseAction_MakeMenuId(phaseIndex, i);
        if (menuId != 0) {
            AppendMenuW(hPhaseMenu, flags, menuId,
                        GetLocalizedString(NULL, action->labelKey));
        }
    }
    return hPhaseMenu;
}

void AppendPomodoroPhaseActionMenu(HMENU hPomodoroMenu) {
    const PomodoroPhaseActionMenuEntry phaseMenus[] = {
        {L"Work Actions", g_AppConfig.pomodoro.work_actions},
        {L"Short Break Actions", g_AppConfig.pomodoro.short_break_actions},
        {L"Long Break Actions", g_AppConfig.pomodoro.long_break_actions}
    };

    HMENU hStateActionConfigMenu = CreatePopupMenu();
    if (hStateActionConfigMenu) {
        BOOL buildFailed = FALSE;
        for (UINT phaseIndex = 0; phaseIndex < (UINT)_countof(phaseMenus); ++phaseIndex) {
            HMENU hPhaseMenu = BuildPomodoroPhaseActionSelectionMenu(
                (int)phaseIndex,
                phaseMenus[phaseIndex].actionChain);
            if (!hPhaseMenu) {
                buildFailed = TRUE;
                break;
            }

            if (!AppendMenuW(hStateActionConfigMenu, MF_POPUP, (UINT_PTR)hPhaseMenu,
                             GetLocalizedString(NULL, phaseMenus[phaseIndex].labelKey))) {
                DestroyMenu(hPhaseMenu);
                buildFailed = TRUE;
                break;
            }
        }

        if (!buildFailed) {
            if (!AppendMenuW(hPomodoroMenu, MF_POPUP, (UINT_PTR)hStateActionConfigMenu,
                             GetLocalizedString(NULL, L"State Action Config"))) {
                DestroyMenu(hStateActionConfigMenu);
            }
        } else {
            DestroyMenu(hStateActionConfigMenu);
        }
    }

}
