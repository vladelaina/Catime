        control->kind == MODERN_CONTROL_LIST ||
        control->kind == MODERN_CONTROL_COMBO ||
        control->kind == MODERN_CONTROL_SLIDER) {
        SetWindowSubclass(control->hwnd, ModernControlSubclassProc,
                          MODERN_CONTROL_SUBCLASS_ID, (DWORD_PTR)control);
    }
}

/* Owner-draw buttons cannot retain BS_DEFPUSHBUTTON in their type mask.
 * Restore the dialog manager's default-command contract explicitly so Enter
 * keeps activating the same primary action after visual styling. */
static void ModernSetDefaultButton(ModernDialogState* state) {
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

static void ModernUpdateBodyScrollMetrics(ModernDialogState* state) {
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
        state->scrollBarHovered = FALSE;
        state->scrollBarDragging = FALSE;
        if (GetCapture() == state->hwnd) ReleaseCapture();
    }
}

static BOOL ModernBodyRegionMatches(const ModernControl* control,
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

static void ModernRememberBodyRegion(ModernControl* control,
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

static void ModernHideBodyControl(ModernControl* control, BOOL redraw) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode != MODERN_BODY_REGION_HIDDEN) {
        SetWindowRgn(control->hwnd, NULL, redraw);
        ShowWindow(control->hwnd, SW_HIDE);
    }
    ModernRememberBodyRegion(control, MODERN_BODY_REGION_HIDDEN,
                             0, 0, 0, 0, 0);
}

static void ModernShowBodyControl(ModernControl* control) {
    if (!control || !control->hwnd) return;
    if (control->bodyRegionMode == MODERN_BODY_REGION_HIDDEN ||
        control->bodyRegionMode == MODERN_BODY_REGION_UNKNOWN) {
        ShowWindow(control->hwnd, SW_SHOWNA);
    }
}

static void ModernApplyBodyControlRegion(
    ModernDialogState* state, ModernControl* control, int y96,
    BOOL suppressRedraw) {
    if (!state || !control || !control->hwnd || !control->sourceVisible) {
        return;
    }

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    int controlTop = DialogModern_Scale(state->dpi, y96);
    int viewportTop =
        DialogModern_Scale(state->dpi, state->headerHeight96);
    int viewportBottom = DialogModern_Scale(
        state->dpi,
        state->headerHeight96 + state->bodyViewportHeight96);
    int visibleTop = viewportTop - controlTop;
    int visibleBottom = viewportBottom - controlTop;

    /* Empty window regions are not reliable for child controls on all
     * supported Windows builds: SetWindowRgn may reject them and leave the
     * previous pixels visible. Hide controls that are completely outside the
     * viewport, and only use a non-empty region for partial clipping. */
    if (visibleBottom <= 0 || visibleTop >= height) {
        ModernHideBodyControl(control, !suppressRedraw);
        return;
    }

    int viewportHeight = viewportBottom - viewportTop;
    BOOL fullyInside = visibleTop <= 0 && visibleBottom >= height;

    /* Group boxes are decorative sibling windows; their labelled frame must
     * not be moved or shortened as it scrolls out of the body.  Once the
     * group title is above the viewport, hide only the frame.  Its sibling
     * controls remain visible and continue to be clipped independently. */
    if (!fullyInside && control->kind == MODERN_CONTROL_GROUP &&
        controlTop < viewportTop) {
        ModernHideBodyControl(control, !suppressRedraw);
        return;
    }

    if (!fullyInside && height > viewportHeight &&
        ModernControlOwnsVerticalScroll(control)) {
        int clippedTop = controlTop < viewportTop ? viewportTop : controlTop;
        int controlBottom = controlTop + height;
        int clippedBottom = controlBottom > viewportBottom
            ? viewportBottom
            : controlBottom;
        int clippedHeight = clippedBottom - clippedTop;
        if (clippedHeight <= 0) {
            ModernHideBodyControl(control, !suppressRedraw);
            return;
        }

        RECT windowRect = {0};
        GetWindowRect(control->hwnd, &windowRect);
        MapWindowPoints(NULL, state->hwnd, (POINT*)&windowRect, 2);
        if (!ModernBodyRegionMatches(
                control, MODERN_BODY_REGION_CROPPED_SCROLL,
                windowRect.right - windowRect.left, clippedHeight,
                0, clippedHeight, state->dpi)) {
            SetWindowRgn(control->hwnd, NULL, !suppressRedraw);
        }
        ModernShowBodyControl(control);
        SetWindowPos(control->hwnd, NULL,
                     windowRect.left, clippedTop,
                     windowRect.right - windowRect.left, clippedHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE |
                         (suppressRedraw ? SWP_NOREDRAW | SWP_NOCOPYBITS : 0));
        ModernRememberBodyRegion(
            control, MODERN_BODY_REGION_CROPPED_SCROLL,
            windowRect.right - windowRect.left, clippedHeight,
            0, clippedHeight, state->dpi);
        return;
    }

    /* Plain instruction statics should leave the fixed title area clean once
     * their top edge has scrolled above the body viewport. Region-clipping a
     * tall static is rendered inconsistently by PrintWindow on older Windows
     * versions. Owner-draw Markdown panels keep their clipped scrolling. */
    if (!fullyInside && controlTop < viewportTop &&
        control->kind == MODERN_CONTROL_OTHER) {
        wchar_t className[32] = {0};
        LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
        if (GetClassNameW(control->hwnd, className, _countof(className)) &&
            _wcsicmp(className, L"Static") == 0 &&
            (style & SS_OWNERDRAW) == 0) {
            ModernHideBodyControl(control, !suppressRedraw);
            return;
        }
    }

    if (!fullyInside && height <= viewportHeight &&
        control->kind != MODERN_CONTROL_GROUP) {
        ModernHideBodyControl(control, !suppressRedraw);
        return;
    }

    ModernShowBodyControl(control);
    if (visibleTop < 0) visibleTop = 0;
    if (visibleBottom > height) visibleBottom = height;

    BOOL fullyVisible = visibleTop == 0 && visibleBottom == height;
    BOOL rounded = control->kind == MODERN_CONTROL_FIELD ||
                   control->kind == MODERN_CONTROL_LIST ||
                   control->kind == MODERN_CONTROL_COMBO;
    if (fullyVisible) {
        ModernBodyRegionMode mode = rounded
            ? MODERN_BODY_REGION_FULL_ROUNDED
            : MODERN_BODY_REGION_FULL_PLAIN;
        if (ModernBodyRegionMatches(control, mode, width, height,
                                    0, height, state->dpi)) {
            return;
        }
        if (rounded) {
            if (!ModernApplyFieldRegionRaw(control, !suppressRedraw)) {
                control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
                return;
            }
        } else {
            SetWindowRgn(control->hwnd, NULL, !suppressRedraw);
        }
        ModernRememberBodyRegion(control, mode, width, height,
                                 0, height, state->dpi);
        return;
    }

    if (visibleBottom < visibleTop) visibleBottom = visibleTop;
    ModernBodyRegionMode mode = rounded
        ? MODERN_BODY_REGION_PARTIAL_ROUNDED
        : MODERN_BODY_REGION_PARTIAL_PLAIN;
    if (ModernBodyRegionMatches(control, mode, width, height,
                                visibleTop, visibleBottom, state->dpi)) {
        return;
    }
    HRGN shape = NULL;
    if (rounded) {
        int radius = DialogModern_Scale(state->dpi, 9);
        shape = CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                   radius * 2, radius * 2);
    } else {
        shape = CreateRectRgn(0, 0, width, height);
    }
    HRGN clip = CreateRectRgn(0, visibleTop, width, visibleBottom);
    if (!shape || !clip) {
        if (shape) DeleteObject(shape);
        if (clip) DeleteObject(clip);
        return;
    }

    CombineRgn(shape, shape, clip, RGN_AND);
    DeleteObject(clip);
    if (!SetWindowRgn(control->hwnd, shape, !suppressRedraw)) {
        DeleteObject(shape);
        control->bodyRegionMode = MODERN_BODY_REGION_UNKNOWN;
        return;
    }
    ModernRememberBodyRegion(control, mode, width, height,
                             visibleTop, visibleBottom, state->dpi);
}

static void ModernLayoutBodyControls(ModernDialogState* state,
                                     BOOL suppressRedraw) {
    if (!state || !state->finalized) return;
