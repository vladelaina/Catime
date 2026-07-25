    InvalidateRect(control->hwnd, NULL, FALSE);
    HWND parent = GetParent(control->hwnd);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    UINT message = (style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    SendMessageW(parent, message, MAKEWPARAM(notification, position),
                 (LPARAM)control->hwnd);
    return TRUE;
}

static BOOL ModernSetSliderFromPoint(ModernControl* control, int x, int y,
                                     UINT notification) {
    if (!control || !control->hwnd) return FALSE;
    int position = ModernSliderPositionFromPoint(control, x, y);
    return ModernSetSliderPosition(control, position, notification);
}

static ModernControl* ModernFindWheelControl(ModernDialogState* state,
                                             POINT screenPoint) {
    if (!state) return NULL;
    HWND hitWindow = WindowFromPoint(screenPoint);
    if (!hitWindow) return NULL;
    for (size_t i = 0; i < state->controlCount; i++) {
        ModernControl* control = &state->controls[i];
        if (!control->hwnd || !IsWindowVisible(control->hwnd) ||
            !IsWindowEnabled(control->hwnd) ||
            (control->kind != MODERN_CONTROL_SLIDER &&
             !ModernIsDateTimeControl(control))) {
            continue;
        }
        if (hitWindow == control->hwnd ||
            IsChild(control->hwnd, hitWindow)) {
            return control;
        }
    }
    return NULL;
}

static BOOL ModernWindowOwnsFocus(HWND owner, HWND focused) {
    if (!owner || !focused) return FALSE;
    HWND current = focused;
    while (current) {
        if (current == owner || IsChild(owner, current)) return TRUE;
        HWND parent = GetParent(current);
        if (!parent || parent == current) break;
        current = parent;
    }
    return FALSE;
}

static void ModernClearFocusedChild(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    HWND focused = GetFocus();
    if (ModernWindowOwnsFocus(state->hwnd, focused)) {
        SetFocus(state->hwnd);
    }
}

static BOOL ModernPointIsPassiveContent(ModernDialogState* state,
                                        POINT point) {
    if (!state || !state->hwnd) return FALSE;
    HWND target = ChildWindowFromPointEx(
        state->hwnd, point,
        CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
    if (!target || target == state->hwnd) return TRUE;
    while (target && GetParent(target) != state->hwnd) {
        target = GetParent(target);
    }
    ModernControl* control = ModernFindControl(state, target);
    return control && (control->kind == MODERN_CONTROL_OTHER ||
                       control->kind == MODERN_CONTROL_GROUP);
}

static BOOL ModernCursorIsOverPassiveContent(ModernDialogState* state) {
    if (!state || !state->hwnd) return FALSE;
    POINT point = {0};
    RECT client = {0};
    if (!GetCursorPos(&point) ||
        !ScreenToClient(state->hwnd, &point) ||
        !GetClientRect(state->hwnd, &client) ||
        !PtInRect(&client, point)) {
        return FALSE;
    }
    return ModernPointIsPassiveContent(state, point);
}

static BOOL ModernHandleInteractiveWheel(ModernDialogState* state,
                                         WPARAM wParam, LPARAM lParam) {
    POINT screenPoint = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ModernControl* target = ModernFindWheelControl(state, screenPoint);
    if (!target) return FALSE;
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    if (delta == 0) return TRUE;

    if (target->kind == MODERN_CONTROL_SLIDER) {
        target->sliderWheelDelta += delta;
        int detents = target->sliderWheelDelta / WHEEL_DELTA;
        target->sliderWheelDelta %= WHEEL_DELTA;
        if (detents != 0) {
            int lineSize = (int)SendMessageW(target->hwnd, TBM_GETLINESIZE, 0, 0);
            if (lineSize <= 0) lineSize = 1;
            int current = (int)SendMessageW(target->hwnd, TBM_GETPOS, 0, 0);
            long long requested = (long long)current +
                                  (long long)detents * lineSize;
            if (requested < INT_MIN) requested = INT_MIN;
            if (requested > INT_MAX) requested = INT_MAX;
            ModernSetSliderPosition(target, (int)requested, TB_THUMBPOSITION);
        }
        return TRUE;
    }

    ModernDateTimeLayout layout = {0};
    POINT clientPoint = screenPoint;
    ScreenToClient(target->hwnd, &clientPoint);
    if (ModernGetDateTimeLayout(target, &layout)) {
        int hit = ModernDateTimeHitTest(&layout, clientPoint);
        if (hit >= MODERN_DATETIME_HOUR && hit <= MODERN_DATETIME_SECOND) {
            ModernSelectDateTimePart(target, hit);
        }
    }
    target->dateTimeWheelDelta += delta;
    int detents = target->dateTimeWheelDelta / WHEEL_DELTA;
    target->dateTimeWheelDelta %= WHEEL_DELTA;
    if (detents != 0) {
        ModernAdjustDateTimePart(target, target->dateTimeSelectedPart, detents);
    }
    return TRUE;
}

static LRESULT CALLBACK ModernControlSubclassProc(HWND hwnd, UINT msg,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
                                                  DWORD_PTR refData) {
    (void)subclassId;
    ModernControl* control = (ModernControl*)refData;
    ModernDialogState* state = control ? control->owner : NULL;

    switch (msg) {
        case WM_MOUSEMOVE:
            if (control && ModernIsDateTimeControl(control)) {
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                if (hit != control->dateTimeHotPart) {
                    control->dateTimeHotPart = hit;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed && GetCapture() == hwnd) {
                ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                         GET_Y_LPARAM(lParam), TB_THUMBTRACK);
                return 0;
            }
            if (control && !control->hovered) {
                control->hovered = TRUE;
                ModernTrackMouse(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            if (control && (control->hovered ||
                            control->dateTimeHotPart !=
                                MODERN_DATETIME_HIT_NONE)) {
                control->hovered = FALSE;
                control->dateTimeHotPart = MODERN_DATETIME_HIT_NONE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_LBUTTONDBLCLK:
            if (!control || !ModernIsDateTimeControl(control)) break;
            /* fall through */
        case WM_LBUTTONDOWN:
            if (control && control->kind == MODERN_CONTROL_GROUP) {
                ModernClearFocusedChild(state);
            }
            if (control && ModernIsDateTimeControl(control)) {
                if (!IsWindowEnabled(hwnd)) return 0;
                SetFocus(hwnd);
                ModernDateTimeLayout layout = {0};
                POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                int hit = ModernGetDateTimeLayout(control, &layout)
                    ? ModernDateTimeHitTest(&layout, point)
                    : MODERN_DATETIME_HIT_NONE;
                control->dateTimeHotPart = hit;
                if (hit >= MODERN_DATETIME_HOUR &&
                    hit <= MODERN_DATETIME_SECOND) {
                    ModernSelectDateTimePart(control, hit);
                } else if (hit == MODERN_DATETIME_STEP_UP ||
                           hit == MODERN_DATETIME_STEP_DOWN) {
                    control->dateTimePressedPart = hit;
                    SetCapture(hwnd);
                    ModernAdjustDateTimePart(
                        control, control->dateTimeSelectedPart,
                        hit == MODERN_DATETIME_STEP_UP ? 1 : -1);
                    ModernStartDateTimeRepeat(control);
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (control) {
                control->pressed = TRUE;
                if (control->kind == MODERN_CONTROL_SLIDER) {
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                    ModernSetSliderFromPoint(control, GET_X_LPARAM(lParam),
                                             GET_Y_LPARAM(lParam),
                                             TB_THUMBTRACK);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            break;
        case WM_LBUTTONUP:
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed) {
                ModernEndSliderDrag(control, TRUE, GET_X_LPARAM(lParam),
                                    GET_Y_LPARAM(lParam));
                return 0;
            }
            /* fall through */
        case WM_CAPTURECHANGED:
            if (control) {
                if (control->kind == MODERN_CONTROL_SLIDER &&
                    control->pressed && (HWND)lParam != hwnd) {
                    ModernEndSliderDrag(control, FALSE, 0, 0);
                    return 0;
                }
                if (ModernIsDateTimeControl(control)) {
                    ModernStopDateTimeRepeat(control);
                    control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                }
                control->pressed = FALSE;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        case WM_CANCELMODE:
            if (control && control->kind == MODERN_CONTROL_SLIDER &&
                control->pressed) {
                ModernEndSliderDrag(control, FALSE, 0, 0);
                return 0;
            }
            if (control && ModernIsDateTimeControl(control)) {
                ModernStopDateTimeRepeat(control);
                control->dateTimePressedPart = MODERN_DATETIME_HIT_NONE;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        case WM_TIMER:
            if (control && ModernIsDateTimeControl(control) &&
                wParam == MODERN_DATETIME_REPEAT_TIMER) {
