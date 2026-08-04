/**
 * @file dialog_modern_datetime_paint.c
 * @brief Date-time painting and native child subclassing.
 */

#include "dialog_modern_internal.h"

void ModernStopDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    KillTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER);
    control->dateTimeRepeatStarted = FALSE;
}

void ModernStartDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    ModernStopDateTimeRepeat(control);
    SetTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER, 420, NULL);
}

void ModernPaintDateTime(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = width > 0 && height > 0 ? CreateCompatibleDC(hdc) : NULL;
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(hdc, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    HDC drawDc = buffer && bitmap ? buffer : hdc;
    FillRect(drawDc, &client, state->fieldBrush);

    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time)) GetLocalTime(&time);
    ModernDateTimeLayout layout = {0};
    if (ModernGetDateTimeLayout(control, &layout)) {
        BOOL enabled = IsWindowEnabled(control->hwnd);
        COLORREF normalText = enabled ? state->palette.text : state->palette.mutedText;
        COLORREF selectedFill = state->palette.highContrast
            ? state->palette.accent
            : ModernBlendColor(state->palette.field, state->palette.accent,
                               state->palette.darkMode ? 24 : 13);
        COLORREF hoverFill = ModernBlendColor(
            state->palette.field, state->palette.surface,
            state->palette.darkMode ? 42 : 58);
        wchar_t digits[3][4] = {0};
        StringCchPrintfW(digits[0], _countof(digits[0]), L"%02u", time.wHour);
        StringCchPrintfW(digits[1], _countof(digits[1]), L"%02u", time.wMinute);
        StringCchPrintfW(digits[2], _countof(digits[2]), L"%02u", time.wSecond);
        for (int i = 0; i < 3; i++) {
            BOOL selected = enabled && control->focused &&
                            control->dateTimeSelectedPart == i;
            BOOL hovered = enabled && control->dateTimeHotPart == i;
            if (selected || hovered) {
                DialogModern_DrawRoundedRect(
                    drawDc, &layout.part[i], DialogModern_Scale(state->dpi, 5),
                    selected ? selectedFill : hoverFill,
                    selected ? selectedFill : hoverFill, 0);
            }
            RECT textRect = layout.part[i];
            DialogModern_DrawText(
                drawDc, state->editFont,
                selected ? (state->palette.highContrast
                                ? state->palette.surface
                                : state->palette.accent)
                         : normalText, &textRect,
                digits[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        COLORREF separator = enabled ? state->palette.mutedText : normalText;
        SetBkMode(drawDc, TRANSPARENT);
        for (int i = 0; i < 2; i++) {
            RECT colon = {layout.part[i].right,
                          layout.content.top,
                          layout.part[i + 1].left,
                          layout.content.bottom};
            DialogModern_DrawText(drawDc, state->editFont, separator, &colon,
                                  L":", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        COLORREF divider = ModernBlendColor(state->palette.border,
                                             state->palette.field, 48);
        HPEN dividerPen = CreatePen(PS_SOLID, 1, divider);
        HGDIOBJ oldPen = dividerPen ? SelectObject(drawDc, dividerPen) : NULL;
        int dividerX = layout.stepper.left - DialogModern_Scale(state->dpi, 3);
        MoveToEx(drawDc, dividerX, layout.stepper.top, NULL);
        LineTo(drawDc, dividerX, layout.stepper.bottom);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (dividerPen) DeleteObject(dividerPen);

        BOOL stepUpHot = enabled && control->dateTimeHotPart == MODERN_DATETIME_STEP_UP;
        BOOL stepDownHot = enabled && control->dateTimeHotPart == MODERN_DATETIME_STEP_DOWN;
        BOOL stepUpPressed = control->dateTimePressedPart == MODERN_DATETIME_STEP_UP;
        BOOL stepDownPressed = control->dateTimePressedPart == MODERN_DATETIME_STEP_DOWN;
        if (stepUpHot || stepUpPressed) {
            DialogModern_DrawRoundedRect(drawDc, &layout.stepUp,
                                         DialogModern_Scale(state->dpi, 5),
                                         stepUpPressed ? selectedFill : hoverFill,
                                         stepUpPressed ? selectedFill : hoverFill, 0);
        }
        if (stepDownHot || stepDownPressed) {
            DialogModern_DrawRoundedRect(drawDc, &layout.stepDown,
                                         DialogModern_Scale(state->dpi, 5),
                                         stepDownPressed ? selectedFill : hoverFill,
                                         stepDownPressed ? selectedFill : hoverFill, 0);
        }
        COLORREF idleArrow = enabled ? state->palette.text
                                     : state->palette.mutedText;
        COLORREF upArrow = stepUpPressed
            ? (state->palette.highContrast
                   ? state->palette.surface : state->palette.accent)
            : (stepUpHot ? state->palette.accent : idleArrow);
        COLORREF downArrow = stepDownPressed
            ? (state->palette.highContrast
                   ? state->palette.surface : state->palette.accent)
            : (stepDownHot ? state->palette.accent : idleArrow);
        int arrowWidth = max(1, DialogModern_Scale(state->dpi, 1));
        int centerX = (layout.stepper.left + layout.stepper.right) / 2;
        int arm = max(2, DialogModern_Scale(state->dpi, 3));
        int upY = (layout.stepUp.top + layout.stepUp.bottom) / 2;
        int downY = (layout.stepDown.top + layout.stepDown.bottom) / 2;
        HPEN arrowPen = CreatePen(PS_SOLID, arrowWidth, upArrow);
        oldPen = arrowPen ? SelectObject(drawDc, arrowPen) : NULL;
        MoveToEx(drawDc, centerX - arm, upY + arm / 2, NULL);
        LineTo(drawDc, centerX, upY - arm / 2);
        LineTo(drawDc, centerX + arm, upY + arm / 2);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (arrowPen) DeleteObject(arrowPen);
        arrowPen = CreatePen(PS_SOLID, arrowWidth, downArrow);
        oldPen = arrowPen ? SelectObject(drawDc, arrowPen) : NULL;
        MoveToEx(drawDc, centerX - arm, downY - arm / 2, NULL);
        LineTo(drawDc, centerX, downY + arm / 2);
        LineTo(drawDc, centerX + arm, downY - arm / 2);
        if (oldPen) SelectObject(drawDc, oldPen);
        if (arrowPen) DeleteObject(arrowPen);
    }
    ModernDrawFieldOutlineToDc(control, drawDc);

    if (buffer && bitmap) {
        BitBlt(hdc, client.left, client.top, width, height,
               buffer, client.left, client.top, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

void ModernTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

void ModernTrackNonClientMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE | TME_NONCLIENT;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

BOOL ModernControlOwnsVerticalScroll(const ModernControl* control) {
    if (!control || !control->hwnd) return FALSE;
    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    if ((style & WS_VSCROLL) != 0) return TRUE;
    if (control->kind == MODERN_CONTROL_LIST ||
        (control->kind == MODERN_CONTROL_COMBO &&
         !ModernIsDateTimeControl(control))) {
        return TRUE;
    }
    if (control->kind == MODERN_CONTROL_FIELD &&
        (style & ES_MULTILINE) != 0 && !ModernIsCompactEdit(control)) {
        return TRUE;
    }
    return FALSE;
}

LRESULT CALLBACK ModernDateTimeChildSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclassId, DWORD_PTR refData) {
    (void)wParam;
    (void)refData;
    switch (msg) {
        case WM_WINDOWPOSCHANGING:
            if (lParam) {
                WINDOWPOS* position = (WINDOWPOS*)lParam;
                position->flags &= ~SWP_SHOWWINDOW;
                position->flags |= SWP_HIDEWINDOW;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernDateTimeChildSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
