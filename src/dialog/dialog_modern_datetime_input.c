/**
 * @file dialog_modern_datetime_input.c
 * @brief Combo painting and date-time editing logic.
 */

#include "dialog_modern_internal.h"

void ModernPaintCombo(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd || ModernIsDateTimeControl(control)) return;

    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    if (!hdc) return;

    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->fieldBrush);

    wchar_t text[512] = {0};
    int selected = (int)SendMessageW(control->hwnd, CB_GETCURSEL, 0, 0);
    if (selected != CB_ERR) {
        SendMessageW(control->hwnd, CB_GETLBTEXT, selected, (LPARAM)text);
    } else {
        GetWindowTextW(control->hwnd, text, (int)_countof(text));
    }

    int arrowWidth = DialogModern_Scale(state->dpi, 34);
    RECT textRect = client;
    textRect.left += DialogModern_Scale(state->dpi, 10);
    textRect.right -= arrowWidth;
    COLORREF textColor = IsWindowEnabled(control->hwnd)
        ? state->palette.text : state->palette.mutedText;
    DialogModern_DrawText(hdc, state->editFont, textColor, &textRect, text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                          DT_END_ELLIPSIS);

    int centerX = client.right - arrowWidth / 2;
    int centerY = (client.top + client.bottom) / 2;
    int arm = max(3, DialogModern_Scale(state->dpi, 4));
    HPEN pen = CreatePen(PS_SOLID,
                         max(1, DialogModern_Scale(state->dpi, 1)),
                         control->hovered ? state->palette.accent : textColor);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    MoveToEx(hdc, centerX - arm, centerY - arm / 2, NULL);
    LineTo(hdc, centerX, centerY + arm / 2);
    LineTo(hdc, centerX + arm, centerY - arm / 2);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}
BOOL ModernGetDateTimeLayout(const ModernControl* control,
                                    ModernDateTimeLayout* layout) {
    if (!control || !control->owner || !control->hwnd || !layout) return FALSE;
    ZeroMemory(layout, sizeof(*layout));
    ModernDialogState* state = control->owner;
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return FALSE;

    int inset = max(2, DialogModern_Scale(state->dpi, 5));
    int stepperWidth = max(DialogModern_Scale(state->dpi, 24), height - inset * 2);
    if (stepperWidth > width / 3) stepperWidth = max(18, width / 4);
    layout->stepper.left = max(client.left, client.right - stepperWidth - inset);
    layout->stepper.top = client.top + inset;
    layout->stepper.right = client.right - inset;
    layout->stepper.bottom = client.bottom - inset;
    layout->stepUp = layout->stepper;
    layout->stepUp.bottom = (layout->stepper.top + layout->stepper.bottom) / 2;
    layout->stepDown = layout->stepper;
    layout->stepDown.top = layout->stepUp.bottom;

    layout->content.left = client.left + inset;
    layout->content.right = layout->stepper.left - inset;
    layout->content.top = client.top + inset;
    layout->content.bottom = client.bottom - inset;
    int contentWidth = layout->content.right - layout->content.left;
    int gap = DialogModern_Scale(state->dpi, 7);
    if (contentWidth < gap * 2 + DialogModern_Scale(state->dpi, 36)) {
        gap = max(2, DialogModern_Scale(state->dpi, 4));
    }
    int partWidth = (contentWidth - gap * 2) / 3;
    if (partWidth < 1) partWidth = 1;
    for (int i = 0; i < 3; i++) {
        layout->part[i].left = layout->content.left + i * (partWidth + gap);
        layout->part[i].right = layout->part[i].left + partWidth;
        layout->part[i].top = layout->content.top;
        layout->part[i].bottom = layout->content.bottom;
    }
    layout->part[2].right = layout->content.right;
    return TRUE;
}

int ModernDateTimePartMaximum(int part) {
    return part == MODERN_DATETIME_HOUR ? 23 : 59;
}

BOOL ModernReadDateTime(const ModernControl* control, SYSTEMTIME* value) {
    if (!control || !control->hwnd || !value) return FALSE;
    ZeroMemory(value, sizeof(*value));
    return SendMessageW(control->hwnd, DTM_GETSYSTEMTIME, 0,
                        (LPARAM)value) == GDT_VALID;
}

void ModernResetDateTimeInput(ModernControl* control) {
    if (!control) return;
    control->dateTimeDigitValue = 0;
    control->dateTimeDigitCount = 0;
    control->dateTimeDigitTick = 0;
}

BOOL ModernWriteDateTimePart(ModernControl* control, int part, int value) {
    if (!control || !control->hwnd || part < 0 || part > 2 ||
        !IsWindowEnabled(control->hwnd)) return FALSE;
    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time)) return FALSE;
    int maximum = ModernDateTimePartMaximum(part);
    if (value < 0) value = 0;
    if (value > maximum) value = maximum;
    if (part == MODERN_DATETIME_HOUR) time.wHour = (WORD)value;
    else if (part == MODERN_DATETIME_MINUTE) time.wMinute = (WORD)value;
    else time.wSecond = (WORD)value;
    LRESULT result = SendMessageW(control->hwnd, DTM_SETSYSTEMTIME,
                                  GDT_VALID, (LPARAM)&time);
    if (result == 0) return FALSE;
    ModernResetDateTimeInput(control);
    InvalidateRect(control->hwnd, NULL, FALSE);
    return TRUE;
}

BOOL ModernAdjustDateTimePart(ModernControl* control, int part,
                                     int delta) {
    if (!control || delta == 0) return FALSE;
    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time) || !IsWindowEnabled(control->hwnd)) {
        return FALSE;
    }
    int current = part == MODERN_DATETIME_HOUR ? time.wHour
                 : part == MODERN_DATETIME_MINUTE ? time.wMinute : time.wSecond;
    int maximum = ModernDateTimePartMaximum(part);
    int span = maximum + 1;
    int next = (current + (delta % span) + span) % span;
    return ModernWriteDateTimePart(control, part, next);
}

int ModernDateTimeHitTest(const ModernDateTimeLayout* layout, POINT point) {
    if (!layout) return MODERN_DATETIME_HIT_NONE;
    if (PtInRect(&layout->stepUp, point)) return MODERN_DATETIME_STEP_UP;
    if (PtInRect(&layout->stepDown, point)) return MODERN_DATETIME_STEP_DOWN;
    for (int i = 0; i < 3; i++) {
        if (PtInRect(&layout->part[i], point)) return i;
    }
    if (PtInRect(&layout->content, point)) {
        int best = 0;
        int bestDistance = INT_MAX;
        for (int i = 0; i < 3; i++) {
            int center = (layout->part[i].left + layout->part[i].right) / 2;
            int distance = abs(point.x - center);
            if (distance < bestDistance) {
                best = i;
                bestDistance = distance;
            }
        }
        return best;
    }
    return MODERN_DATETIME_HIT_NONE;
}

void ModernSelectDateTimePart(ModernControl* control, int part) {
    if (!control) return;
    if (part < MODERN_DATETIME_HOUR) part = MODERN_DATETIME_HOUR;
    if (part > MODERN_DATETIME_SECOND) part = MODERN_DATETIME_SECOND;
    if (control->dateTimeSelectedPart != part ||
        control->dateTimeDigitCount != 0) {
        if (control->dateTimeSelectedPart != part) {
            control->dateTimeWheelDelta = 0;
        }
        control->dateTimeSelectedPart = part;
        ModernResetDateTimeInput(control);
        InvalidateRect(control->hwnd, NULL, FALSE);
    }
}

BOOL ModernInputDateTimeDigit(ModernControl* control, int digit) {
    if (!control || digit < 0 || digit > 9 ||
        !IsWindowEnabled(control->hwnd)) return FALSE;
    int part = control->dateTimeSelectedPart;
    if (part < MODERN_DATETIME_HOUR || part > MODERN_DATETIME_SECOND) {
        part = MODERN_DATETIME_HOUR;
        control->dateTimeSelectedPart = part;
    }
    DWORD now = GetTickCount();
    BOOL continuing = control->dateTimeDigitCount == 1 &&
        (DWORD)(now - control->dateTimeDigitTick) <=
            MODERN_DATETIME_INPUT_TIMEOUT_MS;
    int maximum = ModernDateTimePartMaximum(part);
    if (continuing) {
        int candidate = control->dateTimeDigitValue * 10 + digit;
        if (candidate <= maximum) {
            if (!ModernWriteDateTimePart(control, part, candidate)) return FALSE;
            if (part < MODERN_DATETIME_SECOND) {
                ModernSelectDateTimePart(control, part + 1);
            }
            return TRUE;
        }
    }

    if (!ModernWriteDateTimePart(control, part, digit)) return FALSE;
    if (digit > maximum / 10) {
        if (part < MODERN_DATETIME_SECOND) {
            ModernSelectDateTimePart(control, part + 1);
        }
    } else {
        control->dateTimeDigitValue = digit;
        control->dateTimeDigitCount = 1;
        control->dateTimeDigitTick = now;
    }
    return TRUE;
}
