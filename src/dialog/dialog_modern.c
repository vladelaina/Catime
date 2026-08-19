/**
 * @file dialog_modern.c
 * @brief Public API for modern resource dialog hosting.
 */

#include "dialog_modern_internal.h"

#include <dwmapi.h>

static void ModernArmWindowFirstShowGuard(HWND hwnd, BOOL* cloaked,
                                          BOOL* transparent,
                                          BOOL allowCloak) {
    if (cloaked) *cloaked = FALSE;
    if (transparent) *transparent = FALSE;
    if (!hwnd || !cloaked || !transparent || IsWindowVisible(hwnd)) return;

    if (allowCloak) {
        BOOL cloak = TRUE;
        if (SUCCEEDED(DwmSetWindowAttribute(
                hwnd, MODERN_DWM_CLOAK_ATTRIBUTE,
                &cloak, sizeof(cloak)))) {
            *cloaked = TRUE;
            return;
        }
    }

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_LAYERED) != 0) return;

    SetLastError(ERROR_SUCCESS);
    LONG_PTR previous = SetWindowLongPtrW(
        hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) return;
    if (SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA)) {
        *transparent = TRUE;
    } else {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    }
}

static void ModernReleaseWindowFirstShowGuard(HWND hwnd, BOOL cloaked,
                                              BOOL transparent) {
    if (!hwnd) return;

    if (cloaked) {
        BOOL cloak = FALSE;
        (void)DwmFlush();
        (void)DwmSetWindowAttribute(
            hwnd, MODERN_DWM_CLOAK_ATTRIBUTE, &cloak, sizeof(cloak));
    }
    if (transparent) {
        (void)SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if ((exStyle & WS_EX_LAYERED) != 0) {
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                              exStyle & ~WS_EX_LAYERED);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_NOACTIVATE | SWP_FRAMECHANGED);
            RedrawWindow(hwnd, NULL, NULL,
                         RDW_INVALIDATE | RDW_NOERASE | RDW_FRAME |
                             RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
    }
}

void ModernArmFirstShowGuard(ModernDialogState* state, BOOL allowCloak) {
    if (!state || state->firstShowCloaked ||
        state->firstShowTransparent) return;
    ModernArmWindowFirstShowGuard(state->hwnd, &state->firstShowCloaked,
                                  &state->firstShowTransparent, allowCloak);
}

void ModernReleaseFirstShowGuard(ModernDialogState* state) {
    if (!state) return;
    ModernReleaseWindowFirstShowGuard(state->hwnd,
                                      state->firstShowCloaked,
                                      state->firstShowTransparent);
    state->firstShowCloaked = FALSE;
    state->firstShowTransparent = FALSE;
}

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

BOOL DialogModern_PrepareForShow(HWND hwndDlg) {
    ModernDialogState* state = ModernGetState(hwndDlg);
    if (!state || !ModernFinalize(state)) return FALSE;
    ModernArmFirstShowGuard(state, TRUE);
    return TRUE;
}

void DialogModern_ShowPaintedWindow(HWND hwnd, int showCommand) {
    if (!hwnd || !IsWindow(hwnd)) return;

    ModernDialogState* state = ModernGetState(hwnd);
    if (state) (void)ModernFinalize(state);
    Dialog_EnsureWindowVisible(hwnd);

    BOOL cloaked = FALSE;
    BOOL transparent = FALSE;
    ModernArmWindowFirstShowGuard(hwnd, &cloaked, &transparent, FALSE);
    ShowWindow(hwnd, showCommand);
    RedrawWindow(hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_FRAME |
                     RDW_ALLCHILDREN | RDW_UPDATENOW);
    ModernReleaseWindowFirstShowGuard(hwnd, cloaked, transparent);
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

void DialogModern_SetFeedback(HWND hwndDlg, int controlId,
                              DialogModernFeedbackKind kind,
                              const wchar_t* text) {
    if (!hwndDlg || !IsWindow(hwndDlg)) return;
    HWND control = GetDlgItem(hwndDlg, controlId);
    if (!control) return;

    if (kind < DIALOG_MODERN_FEEDBACK_NONE ||
        kind > DIALOG_MODERN_FEEDBACK_INVALID) {
        kind = DIALOG_MODERN_FEEDBACK_NONE;
    }
    SetWindowTextW(control, text ? text : L"");
    if (kind == DIALOG_MODERN_FEEDBACK_NONE) {
        RemovePropW(control, MODERN_FEEDBACK_STATE_PROP);
    } else {
        SetPropW(control, MODERN_FEEDBACK_STATE_PROP,
                 (HANDLE)(INT_PTR)kind);
    }
    InvalidateRect(control, NULL, FALSE);
}

void ModernSetImeCompositionActive(HWND hwndEdit, BOOL active) {
    if (!hwndEdit || !IsWindow(hwndEdit)) return;
    if (active) {
        SetPropW(hwndEdit, MODERN_IME_COMPOSITION_PROP, (HANDLE)1);
    } else {
        RemovePropW(hwndEdit, MODERN_IME_COMPOSITION_PROP);
    }
}
