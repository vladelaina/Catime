/**
 * @file dialog_modern_layout.c
 * @brief Footer, scrollbar, sizing, and window placement.
 */

#include "dialog_modern_internal.h"

void ModernLayoutControls(ModernDialogState* state) {
    if (!state || !state->finalized) return;
    ModernLayoutBodyControls(state, FALSE);

    ModernControl** footer = NULL;
    size_t footerCount = 0;
    if (state->hasFooter) {
        footer = (ModernControl**)calloc(state->controlCount, sizeof(*footer));
        if (footer) {
            for (size_t i = 0; i < state->controlCount; i++) {
                if (state->controls[i].footer) {
                    footer[footerCount++] = &state->controls[i];
                }
            }
        }
    }

    if (footer && footerCount) {
        qsort(footer, footerCount, sizeof(*footer), ModernControlCompareX);
        int totalWidth96 = (int)(footerCount - 1) * 10;
        for (size_t i = 0; i < footerCount; i++) {
            totalWidth96 += footer[i]->source96.right - footer[i]->source96.left;
        }
        int x96 = state->clientWidth96 - state->sidePadding96 - totalWidth96;
        for (size_t i = 0; i < footerCount; i++) {
            ModernControl* control = footer[i];
            int width96 = control->source96.right - control->source96.left;
            SetWindowPos(control->hwnd, NULL,
                         DialogModern_Scale(state->dpi, x96),
                         DialogModern_Scale(state->dpi, state->footerY96),
                         DialogModern_Scale(state->dpi, width96),
                         DialogModern_Scale(state->dpi, state->footerHeight96),
                         SWP_NOZORDER | SWP_NOACTIVATE);
            x96 += width96 + 10;
        }
    }
    free(footer);

    if (state->closeButton) {
        int size96 = 32;
        int x96 = state->clientWidth96 - state->sidePadding96 - size96;
        SetWindowPos(state->closeButton, HWND_TOP,
                     DialogModern_Scale(state->dpi, x96),
                     DialogModern_Scale(state->dpi, 16),
                     DialogModern_Scale(state->dpi, size96),
                     DialogModern_Scale(state->dpi, size96),
                     SWP_NOACTIVATE);
    }
}

void ModernAttachComboList(ModernControl* control) {
    if (!control || !control->hwnd) return;
    COMBOBOXINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetComboBoxInfo(control->hwnd, &info) || !info.hwndList) return;

    LONG_PTR style = GetWindowLongPtrW(info.hwndList, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(info.hwndList, GWL_EXSTYLE);
    SetWindowLongPtrW(info.hwndList, GWL_STYLE,
                      style & ~(WS_BORDER | WS_HSCROLL | WS_VSCROLL));
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE,
                      exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    DialogModern_ApplyTheme(info.hwndList,
                            control->owner->palette.darkMode);
    DialogModern_DisablePopupShadow(info.hwndList);
    SetWindowSubclass(info.hwndList, ModernComboListSubclassProc,
                      MODERN_COMBO_LIST_SUBCLASS_ID,
                      (DWORD_PTR)control);
    ModernApplyComboListRegion(info.hwndList, control);
    RedrawWindow(info.hwndList, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_FRAME);
}

void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96) {
    if (!state) return;
    ModernUpdateBodyScrollMetrics(state);
    if (offset96 < 0) offset96 = 0;
    if (offset96 > state->bodyScrollMax96) {
        offset96 = state->bodyScrollMax96;
    }
    if (offset96 == state->bodyScrollOffset96) return;

    state->bodyScrollOffset96 = offset96;

    /* DeferWindowPos and SWP_NOREDRAW commit the child layout without exposing
     * intermediate positions. Redraw both the body and its children afterward:
     * Windows 11 can otherwise retain stale child pixels after wheel scrolling. */
    ModernLayoutBodyControls(state, TRUE);

    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    RECT body = {
        0,
        DialogModern_Scale(state->dpi, state->headerHeight96),
        client.right,
        DialogModern_Scale(
            state->dpi,
            state->headerHeight96 + state->bodyViewportHeight96)
    };
    RedrawWindow(state->hwnd, &body, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

BOOL ModernGetScrollbarRects(const ModernDialogState* state,
                                    RECT* track, RECT* thumb) {
    if (!state || !track || !thumb || state->bodyScrollMax96 <= 0) {
        return FALSE;
    }

    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    int gutter = DialogModern_Scale(state->dpi, 14);
    int width = DialogModern_Scale(state->dpi, 5);
    int margin = DialogModern_Scale(state->dpi, 6);
    track->left = client.right - gutter;
    track->right = track->left + width;
    track->top = DialogModern_Scale(state->dpi, state->headerHeight96) +
                 margin;
    track->bottom = DialogModern_Scale(
                        state->dpi,
                        state->headerHeight96 +
                            state->bodyViewportHeight96) -
                    margin;
    if (track->bottom <= track->top) return FALSE;

    int trackHeight = track->bottom - track->top;
    int thumbHeight = MulDiv(
        trackHeight, state->bodyViewportHeight96, state->bodyHeight96);
    int minimumThumb = DialogModern_Scale(state->dpi, 28);
    if (thumbHeight < minimumThumb) thumbHeight = minimumThumb;
    if (thumbHeight > trackHeight) thumbHeight = trackHeight;

    int travel = trackHeight - thumbHeight;
    int thumbTop = track->top;
    if (travel > 0 && state->bodyScrollMax96 > 0) {
        thumbTop += MulDiv(travel, state->bodyScrollOffset96,
                           state->bodyScrollMax96);
    }
    thumb->left = track->left - DialogModern_Scale(state->dpi, 2);
    thumb->right = track->right + DialogModern_Scale(state->dpi, 2);
    thumb->top = thumbTop;
    thumb->bottom = thumbTop + thumbHeight;
    return TRUE;
}

void ModernDrawBodyScrollbar(const ModernDialogState* state, HDC hdc) {
    if (!state || !hdc) return;
    RECT track = {0};
    RECT thumb = {0};
    if (!ModernGetScrollbarRects(state, &track, &thumb)) return;

    COLORREF trackColor = state->palette.field;
    COLORREF thumbColor = state->scrollBarDragging
        ? state->palette.accentHover
        : (state->scrollBarHovered ? state->palette.accent
                                   : state->palette.border);
    DialogModern_DrawRoundedRect(
        hdc, &track,
        DialogModern_Scale(state->dpi, 3),
        trackColor, trackColor, 0);
    DialogModern_DrawRoundedRect(
        hdc, &thumb,
        DialogModern_Scale(state->dpi, 4),
        thumbColor, thumbColor, 0);
}

void ModernEnsureControlVisible(ModernControl* control) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control || control->footer || control->kind == MODERN_CONTROL_CLOSE ||
        state->bodyScrollMax96 <= 0) {
        return;
    }

    int top96 = control->source96.top - state->contentMinY96;
    int bottom96 = control->source96.bottom - state->contentMinY96;
    int offset = state->bodyScrollOffset96;
    if (top96 < offset) {
        offset = top96;
    } else if (bottom96 > offset + state->bodyViewportHeight96) {
        offset = bottom96 - state->bodyViewportHeight96;
    }
    ModernSetBodyScrollOffset(state, offset);
}

BOOL ModernAppendCloseButton(ModernDialogState* state) {
    const wchar_t* cancel = GetLocalizedString(NULL, L"Cancel");
    state->closeButton = CreateWindowExW(
        0, L"BUTTON", cancel ? cancel : L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        0, 0, 0, 0, state->hwnd,
        (HMENU)(INT_PTR)MODERN_DIALOG_CLOSE_ID,
        GetModuleHandleW(NULL), NULL);
    if (!state->closeButton) {
        return FALSE;
    }
    if (!ModernEnsureControlCapacity(state, state->controlCount + 1)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        return FALSE;
    }

    ModernControl* control = &state->controls[state->controlCount];
    ZeroMemory(control, sizeof(*control));
    control->owner = state;
    control->hwnd = state->closeButton;
    control->id = MODERN_DIALOG_CLOSE_ID;
    control->kind = MODERN_CONTROL_CLOSE;
    control->sourceVisible = TRUE;
    ModernSetControlFont(state, control);
    if (!SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                           MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control)) {
        DestroyWindow(state->closeButton);
        state->closeButton = NULL;
        ZeroMemory(control, sizeof(*control));
        return FALSE;
    }
    state->controlCount++;
    return TRUE;
}

void ModernCommitClientSize(ModernDialogState* state,
                                   int width, int height) {
    state->clientWidth96 = ModernTo96(state->dpi, width);
    state->clientHeight96 = ModernTo96(state->dpi, height);
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 - state->bottomPadding96 -
                           state->footerHeight96;
    }
}

/* A dialog can be constrained by the work area or resized by accessibility
 * tooling. Keep the layout model tied to the actual client region so the
 * footer remains anchored and the body can scroll into the remaining space. */
void ModernSyncClientSizeFromWindow(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    RECT client = {0};
    if (!GetClientRect(state->hwnd, &client)) return;

    int width96 = ModernTo96(state->dpi, client.right - client.left);
    int height96 = ModernTo96(state->dpi, client.bottom - client.top);
    if (width96 > 0) state->clientWidth96 = width96;
    if (height96 > 0) state->clientHeight96 = height96;
    if (state->hasFooter) {
        state->footerY96 = state->clientHeight96 -
                           state->bottomPadding96 - state->footerHeight96;
    }
}

void ModernCenterAndResize(ModernDialogState* state) {
    int width = DialogModern_Scale(state->dpi,
                                   state->desiredClientWidth96);
    int height = DialogModern_Scale(state->dpi,
                                    state->desiredClientHeight96);
    HMONITOR monitor = MonitorFromWindow(state->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) {
        ModernCommitClientSize(state, width, height);
        SetWindowPos(state->hwnd, NULL, 0, 0, width, height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
        return;
    }

    int maxWidth = info.rcWork.right - info.rcWork.left -
                   DialogModern_Scale(state->dpi, 24);
    int maxHeight = info.rcWork.bottom - info.rcWork.top -
                    DialogModern_Scale(state->dpi, 24);
    if (width > maxWidth) width = maxWidth;
    if (height > maxHeight) height = maxHeight;
    ModernCommitClientSize(state, width, height);
    int x = info.rcWork.left +
            ((info.rcWork.right - info.rcWork.left) - width) / 2;
    int y = info.rcWork.top +
            ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
    SetWindowPos(state->hwnd, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
