/**
 * @file dialog_modern_window_state.c
 * @brief DPI changes and modern dialog state destruction.
 */

#include "dialog_modern_internal.h"

void ModernHandleDpiChanged(ModernDialogState* state, WPARAM wParam,
                                   LPARAM lParam) {
    state->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96u;
    ModernRebuildResources(state);
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernSetControlFont(state, &state->controls[i]);
    }
    RECT* suggested = (RECT*)lParam;
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = suggested ?
        MonitorFromRect(suggested, MONITOR_DEFAULTTONEAREST) :
        MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        int maxWidth = info.rcWork.right - info.rcWork.left -
                       DialogModern_Scale(state->dpi, 24);
        int maxHeight = info.rcWork.bottom - info.rcWork.top -
                        DialogModern_Scale(state->dpi, 24);
        if (width > maxWidth) width = maxWidth;
        if (height > maxHeight) height = maxHeight;
    }
    ModernCommitClientSize(state, width, height);
    int x = suggested ? suggested->left : 0;
    int y = suggested ? suggested->top : 0;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE |
                 (suggested ? 0 : SWP_NOMOVE));
    ModernLayoutControls(state);
    DialogModern_ApplyWindowShape(state->hwnd, state->dpi, 20);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

void ModernFreeState(ModernDialogState* state) {
    if (!state) return;
    ModernDeleteFonts(state);
    ModernDeleteBrushes(state);
    free(state->controls);
    free(state);
}
