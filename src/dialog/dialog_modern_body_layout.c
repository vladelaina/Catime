/**
 * @file dialog_modern_body_layout.c
 * @brief Scrollable body region placement and clipping.
 */

#include "dialog_modern_internal.h"

void ModernApplyBodyControlRegion(
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

void ModernLayoutBodyControls(ModernDialogState* state,
                                     BOOL suppressRedraw) {
    if (!state || !state->finalized) return;
    ModernUpdateBodyScrollMetrics(state);
    int extraWidth96 = state->clientWidth96 -
                       (state->contentWidth96 + state->sidePadding96 * 2);
    int contentOffsetX96 = state->sidePadding96 +
                           (extraWidth96 > 0 ? extraWidth96 / 2 : 0);

    size_t pendingCount = 0;
    int viewportTop = DialogModern_Scale(state->dpi,
                                         state->headerHeight96);
    int viewportBottom = DialogModern_Scale(
        state->dpi,
        state->headerHeight96 + state->bodyViewportHeight96);

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        control->bodyLayoutPending = FALSE;
        if (control->footer || control->kind == MODERN_CONTROL_CLOSE) continue;

        int x96 = contentOffsetX96 +
                  (control->source96.left - state->contentMinX96);
        int y96 = state->headerHeight96 +
                  (control->source96.top - state->contentMinY96) -
                  state->bodyScrollOffset96;
        int width96 = control->source96.right - control->source96.left;
        int height96 = control->source96.bottom - control->source96.top;

        /* Markdown/text panels authored as full-width owner-draw statics
         * should use the available surface width when a localized title makes
         * the dialog wider. Narrow canvases (HSV/swatch controls) intentionally
         * remain at their authored dimensions. */
        if (control->kind == MODERN_CONTROL_OTHER) {
            wchar_t className[32] = {0};
            LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
            if (GetClassNameW(control->hwnd, className, _countof(className)) &&
                _wcsicmp(className, L"Static") == 0 &&
                (style & SS_OWNERDRAW) != 0 &&
                width96 >= state->contentWidth96 - 24) {
                x96 = state->sidePadding96;
                width96 = state->clientWidth96 -
                          state->sidePadding96 * 2 -
                          (state->bodyScrollMax96 > 0 ? 12 : 0);
            }
        }
        control->bodyLayoutX = DialogModern_Scale(state->dpi, x96);
        control->bodyLayoutY = DialogModern_Scale(state->dpi, y96);
        control->bodyLayoutWidth = DialogModern_Scale(state->dpi, width96);
        control->bodyLayoutHeight = DialogModern_Scale(state->dpi, height96);
        control->bodyLayoutY96 = y96;

        int controlBottom = control->bodyLayoutY +
                            control->bodyLayoutHeight;
        BOOL completelyOutside = controlBottom <= viewportTop ||
                                 control->bodyLayoutY >= viewportBottom;
        if (suppressRedraw && completelyOutside &&
            control->bodyRegionMode == MODERN_BODY_REGION_HIDDEN) {
            continue;
        }
        control->bodyLayoutPending = TRUE;
        pendingCount++;
    }

    UINT positionFlags = SWP_NOZORDER | SWP_NOACTIVATE |
        (suppressRedraw ? SWP_NOREDRAW | SWP_NOCOPYBITS : 0);
    HDWP deferred = pendingCount > 0
        ? BeginDeferWindowPos((int)pendingCount) : NULL;
    BOOL deferredComplete = deferred != NULL;
    if (deferredComplete) {
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernControl* control = &state->controls[i];
            if (!control->bodyLayoutPending) continue;
            deferred = DeferWindowPos(
                deferred, control->hwnd, NULL,
                control->bodyLayoutX, control->bodyLayoutY,
                control->bodyLayoutWidth, control->bodyLayoutHeight,
                positionFlags);
            if (!deferred) {
                deferredComplete = FALSE;
                break;
            }
        }
        if (deferredComplete && !EndDeferWindowPos(deferred)) {
            deferredComplete = FALSE;
        }
    }

    if (!deferredComplete) {
        for (size_t i = 0; i < state->controlCount; i++) {
            ModernControl* control = &state->controls[i];
            if (!control->bodyLayoutPending) continue;
            SetWindowPos(control->hwnd, NULL,
                         control->bodyLayoutX, control->bodyLayoutY,
                         control->bodyLayoutWidth, control->bodyLayoutHeight,
                         positionFlags);
        }
    }

    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->bodyLayoutPending) continue;
        ModernApplyBodyControlRegion(state, control,
                                      control->bodyLayoutY96,
                                      suppressRedraw);
    }
}
