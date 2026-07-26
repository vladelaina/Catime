/**
 * @file dialog_modern.c
 * @brief Public API for modern resource dialog hosting.
 */

#include "dialog_modern_internal.h"

BOOL DialogModern_CopyPalette(HWND hwnd, DialogModernPalette* palette) {
    if (!palette) return FALSE;
    const ModernDialogState* state = ModernGetState(hwnd);
    if (!state && hwnd) state = ModernGetState(GetParent(hwnd));
    if (state && state->surfaceBrush) {
        *palette = state->palette;
        return TRUE;
    }
    DialogModern_ResolvePalette(palette);
    return TRUE;
}
BOOL DialogModern_IsAttached(HWND hwndDlg) {
    return ModernGetState(hwndDlg) != NULL;
}

BOOL DialogModern_Attach(HWND hwndDlg, int dialogType) {
    if (!hwndDlg || !IsWindow(hwndDlg) || DialogModern_IsAttached(hwndDlg)) {
        return hwndDlg && DialogModern_IsAttached(hwndDlg);
    }

    wchar_t className[64] = {0};
    if (!GetClassNameW(hwndDlg, className, _countof(className)) ||
        wcscmp(className, L"#32770") != 0) {
        return FALSE;
    }

    ModernDialogState* state =
        (ModernDialogState*)calloc(1, sizeof(*state));
    if (!state) return FALSE;
    state->hwnd = hwndDlg;
    state->dialogType = dialogType;
    state->dpi = DialogModern_GetDpi(hwndDlg);
    state->attached = TRUE;
    if (!SetPropW(hwndDlg, MODERN_DIALOG_STATE_PROP, (HANDLE)state) ||
        !SetWindowSubclass(hwndDlg, ModernDialogSubclassProc,
                           MODERN_DIALOG_SUBCLASS_ID, (DWORD_PTR)state)) {
        RemovePropW(hwndDlg, MODERN_DIALOG_STATE_PROP);
        free(state);
        return FALSE;
    }
    PostMessageW(hwndDlg, MODERN_DIALOG_FINALIZE_MESSAGE, 0, 0);
    return TRUE;
}

void DialogModern_Refresh(HWND hwndDlg) {
    ModernDialogState* state = ModernGetState(hwndDlg);
    if (!state) return;
    if (state->refreshing) {
        state->refreshPending = TRUE;
        return;
    }

    state->refreshing = TRUE;
    for (;;) {
        state->refreshPending = FALSE;
        ModernRebuildResources(state);
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernSetControlFont(state, &state->controls[i]);
            ModernApplyFieldRegion(&state->controls[i]);
            ModernApplyEditLayout(&state->controls[i]);
            ModernHideDateTimeSpinner(&state->controls[i]);
        }
        RedrawWindow(hwndDlg, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);

        if (ModernGetState(hwndDlg) != state) return;
        if (!state->refreshPending) break;
    }
    state->refreshing = FALSE;
}
