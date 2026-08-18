/**
 * @file dialog_modern_style.c
 * @brief Control styling and body visibility metadata.
 */

#include "dialog_modern_internal.h"

BOOL ModernPasteCompactEdit(HWND hwnd) {
    if (!hwnd || !IsClipboardFormatAvailable(CF_UNICODETEXT) ||
        !OpenClipboard(hwnd)) {
        return FALSE;
    }

    BOOL handled = FALSE;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    const wchar_t* source = data ? (const wchar_t*)GlobalLock(data) : NULL;
    if (source) {
        SIZE_T capacity = GlobalSize(data) / sizeof(*source);
        size_t length = 0;
        while (length < capacity && source[length] != L'\0') length++;
        if (length < capacity) {
            BOOL changed = FALSE;
            wchar_t* text = ModernCreateSingleLineText(
                source, length, &changed);
            if (text && changed) {
                SendMessageW(hwnd, EM_REPLACESEL, TRUE, (LPARAM)text);
                handled = TRUE;
            }
            free(text);
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
    return handled;
}

void ModernStyleControl(ModernDialogState* state,
                               ModernControl* control) {
    if (!state || !control || !control->hwnd) return;
    ModernSetControlFont(state, control);

    if (control->kind == MODERN_CONTROL_PUSH) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        style = (style & ~(BS_TYPEMASK | BS_NOTIFY)) | BS_OWNERDRAW;
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style);
    } else if (control->kind == MODERN_CONTROL_FIELD ||
               control->kind == MODERN_CONTROL_LIST ||
               control->kind == MODERN_CONTROL_COMBO) {
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        LONG_PTR exStyle = GetWindowLongPtrW(control->hwnd, GWL_EXSTYLE);
        SetWindowLongPtrW(control->hwnd, GWL_STYLE, style & ~WS_BORDER);
        SetWindowLongPtrW(control->hwnd, GWL_EXSTYLE,
                          exStyle & ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        if (control->kind == MODERN_CONTROL_COMBO &&
            !ModernIsDateTimeControl(control)) {
            style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            SetWindowLongPtrW(control->hwnd, GWL_STYLE,
                              style | CBS_HASSTRINGS);
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, (WPARAM)-1,
                         DialogModern_Scale(state->dpi, 30));
            SendMessageW(control->hwnd, CB_SETITEMHEIGHT, 0,
                         DialogModern_Scale(state->dpi, 34));
            SendMessageW(control->hwnd, CB_SETMINVISIBLE,
                         MODERN_COMBO_VISIBLE_ITEMS, 0);
            ModernAttachComboList(control);
        }
        ModernApplyFieldRegion(control);
        if (control->kind == MODERN_CONTROL_FIELD) {
            ModernApplyEditLayout(control);
        }
    }

    if (ModernIsDateTimeControl(control)) {
        SendMessageW(control->hwnd, DTM_SETFORMATW, 0,
                     (LPARAM)L"HH':'mm':'ss");
        ModernHideDateTimeSpinner(control);
    }

    if (control->kind == MODERN_CONTROL_PUSH ||
        control->kind == MODERN_CONTROL_CHECK ||
        control->kind == MODERN_CONTROL_RADIO ||
        control->kind == MODERN_CONTROL_GROUP ||
        control->kind == MODERN_CONTROL_FIELD ||
        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO ||
        control->kind == MODERN_CONTROL_SLIDER ||
        control->kind == MODERN_CONTROL_INSTRUCTION ||
        control->kind == MODERN_CONTROL_FEEDBACK) {
        SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                          MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control);
    }
}

/* Owner-draw buttons cannot retain BS_DEFPUSHBUTTON in their type mask.
 * Restore the dialog manager's default-command contract explicitly so Enter
 * keeps activating the same primary action after visual styling. */
void ModernSetDefaultButton(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    for (size_t i = 0; i < state->controlCount; i++) {
        const ModernControl* control = &state->controls[i];
        if (control->sourceVisible && control->kind == MODERN_CONTROL_PUSH &&
            control->primary) {
            SendMessageW(state->hwnd, DM_SETDEFID, (WPARAM)control->id, 0);
            return;
        }
    }
}

void ModernUpdateBodyScrollMetrics(ModernDialogState* state) {
    if (!state) return;

    int viewportBottom96 = state->hasFooter
        ? state->footerY96 - 16
        : state->clientHeight96 - state->bottomPadding96;
    int viewportHeight96 = viewportBottom96 - state->headerHeight96;
    if (viewportHeight96 < 1) viewportHeight96 = 1;

    state->bodyViewportHeight96 = viewportHeight96;
    state->bodyScrollMax96 =
        state->bodyHeight96 > viewportHeight96
            ? state->bodyHeight96 - viewportHeight96
            : 0;
    if (state->bodyScrollOffset96 < 0) {
        state->bodyScrollOffset96 = 0;
    }
    if (state->bodyScrollOffset96 > state->bodyScrollMax96) {
        state->bodyScrollOffset96 = state->bodyScrollMax96;
    }
    if (state->bodyScrollMax96 == 0) {
        state->bodyWheelDelta = 0;
        state->scrollBarHovered = FALSE;
        state->scrollBarDragging = FALSE;
        state->scrollUpdatePending = FALSE;
        if (state->scrollUpdateTimerActive) {
            KillTimer(state->hwnd, MODERN_SCROLL_DRAG_TIMER);
            state->scrollUpdateTimerActive = FALSE;
        }
        if (GetCapture() == state->hwnd) ReleaseCapture();
    }
}

BOOL ModernBodyRegionMatches(const ModernControl* control,
                                    ModernBodyRegionMode mode,
                                    int width, int height,
                                    int top, int bottom, UINT dpi) {
    return control && control->bodyRegionMode == mode &&
           control->bodyRegionWidth == width &&
           control->bodyRegionHeight == height &&
           control->bodyRegionTop == top &&
           control->bodyRegionBottom == bottom &&
           control->bodyRegionDpi == dpi;
}

void ModernRememberBodyRegion(ModernControl* control,
                                     ModernBodyRegionMode mode,
                                     int width, int height,
                                     int top, int bottom, UINT dpi) {
    if (!control) return;
    control->bodyRegionMode = mode;
    control->bodyRegionWidth = width;
    control->bodyRegionHeight = height;
    control->bodyRegionTop = top;
    control->bodyRegionBottom = bottom;
    control->bodyRegionDpi = dpi;
}

void ModernHideBodyControl(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode != MODERN_BODY_REGION_HIDDEN) {
        SetWindowRgn(control->hwnd, NULL, redraw);
        if (redraw) {
            ShowWindow(control->hwnd, SW_HIDE);
        } else {
            /* Hide without invalidating the parent while a scroll layout is
             * still moving the rest of the body controls. */
            SetWindowPos(control->hwnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOREDRAW);
        }
    }
    ModernRememberBodyRegion(control, MODERN_BODY_REGION_HIDDEN,
                             0, 0, 0, 0, 0);
}

void ModernShowBodyControl(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode == MODERN_BODY_REGION_HIDDEN ||
        control->bodyRegionMode == MODERN_BODY_REGION_UNKNOWN) {
        if (redraw) {
            ShowWindow(control->hwnd, SW_SHOWNA);
        } else {
            /* SetWindowPos can reveal the child without scheduling an
             * intermediate paint while a scroll layout is being committed. */
            SetWindowPos(control->hwnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOREDRAW);
            InvalidateRect(control->hwnd, NULL, FALSE);
        }
    }
}
