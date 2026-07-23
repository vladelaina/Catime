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

static BOOL ModernAdjustDateTimePart(ModernControl* control, int part,
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

static int ModernDateTimeHitTest(const ModernDateTimeLayout* layout, POINT point) {
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

static void ModernSelectDateTimePart(ModernControl* control, int part) {
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

static BOOL ModernInputDateTimeDigit(ModernControl* control, int digit) {
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

static void ModernStopDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    KillTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER);
    control->dateTimeRepeatStarted = FALSE;
}

static void ModernStartDateTimeRepeat(ModernControl* control) {
    if (!control || !control->hwnd) return;
    ModernStopDateTimeRepeat(control);
    SetTimer(control->hwnd, MODERN_DATETIME_REPEAT_TIMER, 420, NULL);
}

static void ModernPaintDateTime(ModernControl* control, HDC suppliedDc) {
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
        COLORREF upArrow = stepUpHot || stepUpPressed
            ? (state->palette.highContrast && stepUpPressed
                   ? state->palette.surface : state->palette.accent)
            : idleArrow;
        COLORREF downArrow = stepDownHot || stepDownPressed
            ? (state->palette.highContrast && stepDownPressed
                   ? state->palette.surface : state->palette.accent)
            : idleArrow;
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

static void ModernTrackMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd;
    TrackMouseEvent(&track);
}

static void ModernTrackNonClientMouse(HWND hwnd) {
    TRACKMOUSEEVENT track = {0};
    track.cbSize = sizeof(track);
