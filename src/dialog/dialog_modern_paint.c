/**
 * @file dialog_modern_paint.c
 * @brief Buffered field, choice, and slider painting.
 */

#include "dialog_modern_internal.h"

void ModernPaintBuffered(ModernDialogState* state, HDC target) {
    RECT client = {0};
    GetClientRect(state->hwnd, &client);
    int width = client.right - client.left;
    int height = client.bottom - client.top;
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = buffer && bitmap ? SelectObject(buffer, bitmap) : NULL;
    if (buffer && bitmap) {
        ModernDrawDialog(state, buffer);
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    } else {
        ModernDrawDialog(state, target);
        if (bitmap) DeleteObject(bitmap);
        if (buffer) DeleteDC(buffer);
    }
}

void ModernDrawFieldOutlineToDc(ModernControl* control, HDC hdc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state || !control->hwnd || !hdc) return;
    RECT rect = {0};
    GetClientRect(control->hwnd, &rect);
    InflateRect(&rect, -1, -1);
    COLORREF border = control->focused
        ? state->palette.accent
        : (control->hovered
               ? ModernBlendColor(state->palette.border,
                                  state->palette.accent, 38)
               : state->palette.border);
    HPEN pen = CreatePen(PS_SOLID,
                         control->focused ? DialogModern_Scale(state->dpi, 2) : 1,
                         border);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom,
              DialogModern_Scale(state->dpi, 18),
              DialogModern_Scale(state->dpi, 18));
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

void ModernDrawFieldOutline(ModernControl* control) {
    if (!control || !control->hwnd) return;
    HDC hdc = GetDC(control->hwnd);
    if (!hdc) return;
    ModernDrawFieldOutlineToDc(control, hdc);
    ReleaseDC(control->hwnd, hdc);
}

void ModernPaintChoiceControl(ModernControl* control, HDC suppliedDc) {
    ModernDialogState* state = control ? control->owner : NULL;
    if (!state) return;
    PAINTSTRUCT paint = {0};
    HDC hdc = suppliedDc ? suppliedDc : BeginPaint(control->hwnd, &paint);
    RECT client = {0};
    GetClientRect(control->hwnd, &client);
    FillRect(hdc, &client, state->surfaceBrush);

    wchar_t text[512] = {0};
    GetWindowTextW(control->hwnd, text, (int)_countof(text));
    BOOL enabled = IsWindowEnabled(control->hwnd);
    COLORREF textColor = enabled ? state->palette.text : state->palette.mutedText;

    if (control->kind == MODERN_CONTROL_GROUP) {
        int labelHeight = DialogModern_Scale(state->dpi, 18);
        RECT borderRect = client;
        borderRect.top += labelHeight / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, state->palette.border);
        HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, borderRect.left, borderRect.top,
                  borderRect.right, borderRect.bottom,
                  DialogModern_Scale(state->dpi, 16),
                  DialogModern_Scale(state->dpi, 16));
        SelectObject(hdc, oldBrush);
        if (oldPen) SelectObject(hdc, oldPen);
        if (pen) DeleteObject(pen);
        RECT label = {DialogModern_Scale(state->dpi, 12), 0,
                      client.right - DialogModern_Scale(state->dpi, 12),
                      labelHeight};
        if (text[0]) {
            SIZE textSize = {0};
            ModernMeasureText(control->hwnd, state->labelFont, &textSize);
            RECT labelBackdrop = {
                label.left - DialogModern_Scale(state->dpi, 4),
                0,
                label.left + textSize.cx + DialogModern_Scale(state->dpi, 4),
                labelHeight
            };
            if (labelBackdrop.right > label.right) {
                labelBackdrop.right = label.right;
            }
            FillRect(hdc, &labelBackdrop, state->surfaceBrush);
        }
        DialogModern_DrawText(hdc, state->labelFont, state->palette.mutedText,
                              &label, text,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                              DT_END_ELLIPSIS);
    } else {
        int glyphSize = DialogModern_Scale(state->dpi, 16);
        int glyphY = (client.bottom - glyphSize) / 2;
        RECT glyph = {1, glyphY, 1 + glyphSize, glyphY + glyphSize};
        LRESULT checked = SendMessageW(control->hwnd, BM_GETCHECK, 0, 0);
        BOOL selected = checked == BST_CHECKED || checked == BST_INDETERMINATE;
        COLORREF selectionMark = state->palette.highContrast
            ? GetSysColor(COLOR_HIGHLIGHTTEXT)
            : RGB(0xFF, 0xFF, 0xFF);
        if (control->kind == MODERN_CONTROL_RADIO) {
            HBRUSH brush = CreateSolidBrush(selected ? state->palette.accent :
                                                       state->palette.surface);
            HPEN pen = CreatePen(PS_SOLID, 1,
                                 selected ? state->palette.accent :
                                            state->palette.border);
            HGDIOBJ oldBrush = brush ? SelectObject(hdc, brush) : NULL;
            HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
            Ellipse(hdc, glyph.left, glyph.top, glyph.right, glyph.bottom);
            if (selected) {
                RECT dot = glyph;
                InflateRect(&dot, -DialogModern_Scale(state->dpi, 5),
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

void ModernPaintSlider(ModernControl* control, HDC suppliedDc) {
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
    RECT completed;
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
