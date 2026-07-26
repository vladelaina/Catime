/**
 * @file dialog_modern_interaction.c
 * @brief Slider positioning, focus, passive content, and wheel routing.
 */

#include "dialog_modern_internal.h"

int ModernSliderPositionFromPoint(ModernControl* control, int x, int y) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return 0;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    BOOL vertical = (style & TBS_VERT) != 0;
    BOOL reversed = (style & TBS_REVERSED) != 0;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    if (maximum <= minimum) return minimum;
    int thumbRadius = DialogModern_Scale(state->dpi, 7);
    int edge = thumbRadius + DialogModern_Scale(state->dpi, 2);
    int start = vertical ? client.top + edge : client.left + edge;
    int finish = vertical ? client.bottom - edge : client.right - edge;
    int coordinate = vertical ? y : x;
    if (coordinate < start) coordinate = start;
    if (coordinate > finish) coordinate = finish;
    int position = minimum + MulDiv(coordinate - start,
                                    maximum - minimum,
                                    max(1, finish - start));
    if (reversed) position = maximum - (position - minimum);
    return position;
}

BOOL ModernSetSliderPosition(ModernControl* control, int position,
                                    UINT notification) {
    if (!control || !control->hwnd ||
        control->kind != MODERN_CONTROL_SLIDER) return FALSE;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    if (position < minimum) position = minimum;
    if (position > maximum) position = maximum;
    int previous = (int)SendMessageW(control->hwnd, TBM_GETPOS, 0, 0);
    if (position == previous) return FALSE;
    SendMessageW(control->hwnd, TBM_SETPOS, FALSE, position);
    InvalidateRect(control->hwnd, NULL, FALSE);
    HWND parent = GetParent(control->hwnd);
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    UINT message = (style & TBS_VERT) ? WM_VSCROLL : WM_HSCROLL;
    SendMessageW(parent, message, MAKEWPARAM(notification, position),
                 (LPARAM)control->hwnd);
    return TRUE;
}

BOOL ModernSetSliderFromPoint(ModernControl* control, int x, int y,
                                     UINT notification) {
    if (!control || !control->hwnd) return FALSE;
    int position = ModernSliderPositionFromPoint(control, x, y);
    return ModernSetSliderPosition(control, position, notification);
}

ModernControl* ModernFindWheelControl(ModernDialogState* state,
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

BOOL ModernWindowOwnsFocus(HWND owner, HWND focused) {
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

void ModernClearFocusedChild(ModernDialogState* state) {
    if (!state || !state->hwnd) return;
    HWND focused = GetFocus();
    if (ModernWindowOwnsFocus(state->hwnd, focused)) {
        SetFocus(state->hwnd);
    }
}

BOOL ModernPointIsPassiveContent(ModernDialogState* state,
                                        POINT point) {
    if (!state || !state->hwnd) return FALSE;
    HWND target = ChildWindowFromPointEx(
        state->hwnd, point,
        CWP_SKIPINVISIBLE | CWP_SKIPDISABLED | CWP_SKIPTRANSPARENT);
    if (!target || target == state->hwnd) return TRUE;
    while (target && GetParent(target) != state->hwnd) {
        target = GetParent(target);
    }
    const ModernControl* control = ModernFindControl(state, target);
    return control && (control->kind == MODERN_CONTROL_OTHER ||
                       control->kind == MODERN_CONTROL_GROUP);
}

BOOL ModernCursorIsOverPassiveContent(ModernDialogState* state) {
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

BOOL ModernHandleInteractiveWheel(ModernDialogState* state,
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
