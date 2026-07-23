                            -DialogModern_Scale(state->dpi, 5));
                HBRUSH dotBrush = CreateSolidBrush(selectionMark);
                HGDIOBJ prior = SelectObject(hdc, dotBrush);
                Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
                SelectObject(hdc, prior);
                DeleteObject(dotBrush);
            }
            if (oldPen) SelectObject(hdc, oldPen);
            if (oldBrush) SelectObject(hdc, oldBrush);
            if (pen) DeleteObject(pen);
            if (brush) DeleteObject(brush);
        } else {
            DialogModern_DrawRoundedRect(hdc, &glyph,
                                         DialogModern_Scale(state->dpi, 7),
                                         selected ? state->palette.accent :
                                                    state->palette.surface,
                                         selected ? state->palette.accent :
                                                    state->palette.border, 1);
            if (selected) {
                HPEN checkPen = CreatePen(PS_SOLID, 2, selectionMark);
                HGDIOBJ oldPen = checkPen ? SelectObject(hdc, checkPen) : NULL;
                int midX = (glyph.left + glyph.right) / 2;
                int midY = (glyph.top + glyph.bottom) / 2;
                MoveToEx(hdc, glyph.left + glyphSize / 4, midY, NULL);
                LineTo(hdc, midX - glyphSize / 10, glyph.bottom - glyphSize / 4);
                LineTo(hdc, glyph.right - glyphSize / 5,
                       glyph.top + glyphSize / 4);
                if (oldPen) SelectObject(hdc, oldPen);
                if (checkPen) DeleteObject(checkPen);
            }
        }

        RECT textRect = {glyph.right + DialogModern_Scale(state->dpi, 8), 0,
                         client.right, client.bottom};
        DialogModern_DrawText(hdc, state->bodyFont, textColor, &textRect, text,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    }

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernPaintSlider(ModernControl* control, HDC suppliedDc) {
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
    FillRect(drawDc, &client, state->surfaceBrush);

    LONG_PTR style = GetWindowLongPtrW(control->hwnd, GWL_STYLE);
    BOOL vertical = (style & TBS_VERT) != 0;
    BOOL reversed = (style & TBS_REVERSED) != 0;
    int minimum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMIN, 0, 0);
    int maximum = (int)SendMessageW(control->hwnd, TBM_GETRANGEMAX, 0, 0);
    int position = (int)SendMessageW(control->hwnd, TBM_GETPOS, 0, 0);
    if (maximum <= minimum) maximum = minimum + 1;
    if (position < minimum) position = minimum;
    if (position > maximum) position = maximum;

    int thumbRadius = DialogModern_Scale(state->dpi,
                                         control->pressed ? 7 : 6);
    int channelThickness = DialogModern_Scale(state->dpi, 4);
    int edge = thumbRadius + DialogModern_Scale(state->dpi, 2);
    int usable = vertical ? (client.bottom - client.top - edge * 2) :
                            (client.right - client.left - edge * 2);
    if (usable < 1) usable = 1;
    int offset = MulDiv(position - minimum, usable, maximum - minimum);
    if (reversed) offset = usable - offset;

    POINT thumb = {0};
    RECT channel = client;
    RECT completed = client;
    if (vertical) {
        thumb.x = (client.left + client.right) / 2;
        thumb.y = client.top + edge + offset;
        channel.left = thumb.x - channelThickness / 2;
        channel.right = channel.left + channelThickness;
        channel.top = client.top + edge;
        channel.bottom = client.bottom - edge;
        completed = channel;
        completed.bottom = thumb.y;
    } else {
        thumb.x = client.left + edge + offset;
        thumb.y = (client.top + client.bottom) / 2;
        channel.top = thumb.y - channelThickness / 2;
        channel.bottom = channel.top + channelThickness;
        channel.left = client.left + edge;
        channel.right = client.right - edge;
        completed = channel;
        completed.right = thumb.x;
    }

    DialogModern_DrawRoundedRect(drawDc, &channel, channelThickness,
                                 state->palette.border,
                                 state->palette.border, 0);
    DialogModern_DrawRoundedRect(drawDc, &completed, channelThickness,
                                 state->palette.accent,
                                 state->palette.accent, 0);

    BOOL enabled = IsWindowEnabled(control->hwnd);
    COLORREF thumbColor = enabled ? state->palette.accent :
                                   state->palette.mutedText;
    COLORREF outline = control->focused || control->hovered ?
                       state->palette.accentHover : state->palette.surface;
    HBRUSH thumbBrush = CreateSolidBrush(thumbColor);
    HPEN thumbPen = CreatePen(PS_SOLID,
                              control->focused ?
                                  DialogModern_Scale(state->dpi, 2) : 1,
                              outline);
    HGDIOBJ oldBrush = thumbBrush ? SelectObject(drawDc, thumbBrush) : NULL;
    HGDIOBJ oldPen = thumbPen ? SelectObject(drawDc, thumbPen) : NULL;
    Ellipse(drawDc, thumb.x - thumbRadius, thumb.y - thumbRadius,
            thumb.x + thumbRadius + 1, thumb.y + thumbRadius + 1);
    if (oldPen) SelectObject(drawDc, oldPen);
    if (oldBrush) SelectObject(drawDc, oldBrush);
    if (thumbPen) DeleteObject(thumbPen);
    if (thumbBrush) DeleteObject(thumbBrush);

    if (buffer && bitmap) {
        BitBlt(hdc, client.left, client.top, width, height,
               buffer, client.left, client.top, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);

    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}

static void ModernPaintCombo(ModernControl* control, HDC suppliedDc) {
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

typedef struct {
    RECT part[3];
    RECT stepper;
    RECT stepUp;
    RECT stepDown;
    RECT content;
} ModernDateTimeLayout;

static BOOL ModernGetDateTimeLayout(const ModernControl* control,
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

static int ModernDateTimePartMaximum(int part) {
    return part == MODERN_DATETIME_HOUR ? 23 : 59;
}

static BOOL ModernReadDateTime(const ModernControl* control, SYSTEMTIME* value) {
    if (!control || !control->hwnd || !value) return FALSE;
    return SendMessageW(control->hwnd, DTM_GETSYSTEMTIME, 0,
                        (LPARAM)value) == GDT_VALID;
}

static void ModernResetDateTimeInput(ModernControl* control) {
    if (!control) return;
    control->dateTimeDigitValue = 0;
    control->dateTimeDigitCount = 0;
    control->dateTimeDigitTick = 0;
}

static BOOL ModernWriteDateTimePart(ModernControl* control, int part, int value) {
    if (!control || !control->hwnd || part < 0 || part > 2 ||
        !IsWindowEnabled(control->hwnd)) return FALSE;
    SYSTEMTIME time = {0};
    if (!ModernReadDateTime(control, &time)) return FALSE;
