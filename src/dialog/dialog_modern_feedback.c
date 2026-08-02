/**
 * @file dialog_modern_feedback.c
 * @brief Countdown-style valid and invalid input feedback.
 */

#include "dialog_modern_internal.h"

#define MODERN_FEEDBACK_MAX_CHARS 4096

static wchar_t* ModernFeedbackReadText(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0 || length > MODERN_FEEDBACK_MAX_CHARS) return NULL;
    wchar_t* text = (wchar_t*)calloc((size_t)length + 1, sizeof(*text));
    if (!text) return NULL;
    if (GetWindowTextW(hwnd, text, length + 1) <= 0) {
        free(text);
        return NULL;
    }
    return text;
}

static int ModernFeedbackMeasure(HDC hdc, HFONT font,
                                 const wchar_t* text, int width) {
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    RECT measure = {0, 0, width, 0};
    DrawTextW(hdc, text, -1, &measure,
              DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    if (oldFont) SelectObject(hdc, oldFont);
    return measure.bottom - measure.top;
}

static void ModernFeedbackDrawCheck(HDC hdc, int x, int y,
                                    int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, max(1, radius / 4), color);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, x - radius, y - radius, x + radius, y + radius);
    MoveToEx(hdc, x - radius / 2, y, NULL);
    LineTo(hdc, x - radius / 8, y + radius / 2);
    LineTo(hdc, x + radius * 2 / 3, y - radius / 2);
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void ModernFeedbackDrawWarning(HDC hdc, int x, int y,
                                      int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, max(1, radius / 4), color);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    POINT triangle[3] = {{x, y - radius},
                         {x - radius, y + radius},
                         {x + radius, y + radius}};
    Polygon(hdc, triangle, _countof(triangle));
    MoveToEx(hdc, x, y - radius / 3, NULL);
    LineTo(hdc, x, y + radius / 3);
    Ellipse(hdc, x - 1, y + radius / 2 - 1,
            x + 1, y + radius / 2 + 1);
    SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void ModernFeedbackDraw(ModernControl* control, HDC hdc,
                               const RECT* client) {
    ModernDialogState* state = control->owner;
    FillRect(hdc, client, state->surfaceBrush);
    wchar_t* text = ModernFeedbackReadText(control->hwnd);
    if (!text) return;

    DialogModernFeedbackKind kind = (DialogModernFeedbackKind)(INT_PTR)
        GetPropW(control->hwnd, MODERN_FEEDBACK_STATE_PROP);
    COLORREF color = kind == DIALOG_MODERN_FEEDBACK_INVALID
        ? state->palette.danger : state->palette.accent;
    int radius = DialogModern_Scale(state->dpi, 7);
    int centerX = client->left + radius + DialogModern_Scale(state->dpi, 2);
    int centerY = (client->top + client->bottom) / 2;
    if (kind == DIALOG_MODERN_FEEDBACK_INVALID) {
        ModernFeedbackDrawWarning(hdc, centerX, centerY, radius, color);
    } else if (kind == DIALOG_MODERN_FEEDBACK_VALID) {
        ModernFeedbackDrawCheck(hdc, centerX, centerY, radius, color);
    }

    RECT textRect = {centerX + radius + DialogModern_Scale(state->dpi, 8),
                     client->top,
                     client->right - DialogModern_Scale(state->dpi, 4),
                     client->bottom};
    int textHeight = ModernFeedbackMeasure(
        hdc, state->labelFont, text, textRect.right - textRect.left);
    if (textHeight > 0 && textHeight < textRect.bottom - textRect.top) {
        textRect.top += ((textRect.bottom - textRect.top) - textHeight) / 2;
    }
    DialogModern_DrawText(
        hdc, state->labelFont,
        kind == DIALOG_MODERN_FEEDBACK_NONE ? state->palette.mutedText : color,
        &textRect, text, DT_LEFT | DT_WORDBREAK);
    free(text);
}

void ModernPaintFeedback(ModernControl* control, HDC suppliedDc) {
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
    HDC target = buffer && bitmap ? buffer : hdc;
    ModernFeedbackDraw(control, target, &client);
    if (buffer && bitmap) {
        BitBlt(hdc, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    if (!suppliedDc) EndPaint(control->hwnd, &paint);
}
