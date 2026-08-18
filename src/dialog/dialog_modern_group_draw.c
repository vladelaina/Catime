/**
 * @file dialog_modern_group_draw.c
 * @brief Parent-surface painting for scrollable group frames.
 */

#include "dialog_modern_internal.h"

void ModernDrawBodyGroups(ModernDialogState* state, HDC hdc) {
    if (!state || !hdc || state->bodyViewportHeight96 <= 0) return;

    int viewportTop = DialogModern_Scale(
        state->dpi, state->headerHeight96);
    int viewportBottom = DialogModern_Scale(
        state->dpi,
        state->headerHeight96 + state->bodyViewportHeight96);
    int savedDc = SaveDC(hdc);
    if (savedDc == 0) return;
    RECT dialogClient = {0};
    GetClientRect(state->hwnd, &dialogClient);
    IntersectClipRect(hdc, 0, viewportTop,
                      dialogClient.right, viewportBottom);

    for (size_t i = 0; i < state->controlCount; i++) {
        const ModernControl* control = &state->controls[i];
        if (!control->sourceVisible || control->footer ||
            control->kind != MODERN_CONTROL_GROUP ||
            control->bodyLayoutWidth <= 0 ||
            control->bodyLayoutHeight <= 0) {
            continue;
        }

        RECT client = {
            control->bodyLayoutX,
            control->bodyLayoutY,
            control->bodyLayoutX + control->bodyLayoutWidth,
            control->bodyLayoutY + control->bodyLayoutHeight
        };
        if (client.bottom <= viewportTop || client.top >= viewportBottom) {
            continue;
        }

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

        wchar_t labelText[512] = {0};
        GetWindowTextW(control->hwnd, labelText,
                       (int)_countof(labelText));
        if (!labelText[0]) continue;

        RECT label = {
            client.left + DialogModern_Scale(state->dpi, 12),
            client.top,
            client.right - DialogModern_Scale(state->dpi, 12),
            client.top + labelHeight
        };
        SIZE textSize = {0};
        HGDIOBJ oldFont = state->labelFont
            ? SelectObject(hdc, state->labelFont) : NULL;
        GetTextExtentPoint32W(hdc, labelText,
                              (int)wcslen(labelText), &textSize);
        if (oldFont) SelectObject(hdc, oldFont);
        RECT backdrop = {
            label.left - DialogModern_Scale(state->dpi, 4),
            label.top,
            label.left + textSize.cx + DialogModern_Scale(state->dpi, 4),
            label.bottom
        };
        if (backdrop.right > label.right) backdrop.right = label.right;
        FillRect(hdc, &backdrop, state->surfaceBrush);
        DialogModern_DrawText(
            hdc, state->labelFont, state->palette.mutedText,
            &label, labelText,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RestoreDC(hdc, savedDc);
}
