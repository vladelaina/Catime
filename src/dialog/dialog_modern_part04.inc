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

static void ModernLayoutControls(ModernDialogState* state) {
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

static void ModernAttachComboList(ModernControl* control) {
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

static void ModernSetBodyScrollOffset(ModernDialogState* state, int offset96) {
    if (!state) return;
    ModernUpdateBodyScrollMetrics(state);
    if (offset96 < 0) offset96 = 0;
    if (offset96 > state->bodyScrollMax96) {
        offset96 = state->bodyScrollMax96;
    }
    if (offset96 == state->bodyScrollOffset96) return;

    state->bodyScrollOffset96 = offset96;

    /* Moving, clipping and hiding a large group of child windows one by one
     * exposes intermediate frames on slower machines.  Freeze the short
     * layout transaction, then invalidate one complete frame.  Keeping the
     * repaint asynchronous lets consecutive wheel/drag messages coalesce;
     * forcing every intermediate frame here makes scrolling visibly stall. */
    SendMessageW(state->hwnd, WM_SETREDRAW, FALSE, 0);
    ModernLayoutBodyControls(state, TRUE);
    SendMessageW(state->hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(state->hwnd, NULL, NULL,
                 RDW_INVALIDATE | RDW_NOERASE | RDW_ALLCHILDREN);
}

static BOOL ModernGetScrollbarRects(const ModernDialogState* state,
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

static void ModernDrawBodyScrollbar(const ModernDialogState* state, HDC hdc) {
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
