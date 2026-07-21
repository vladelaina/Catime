/**
 * @file color_feedback.c
 * @brief Reusable parsing state and inline rendering for color input feedback.
 */

#include <ctype.h>
#include <string.h>
#include <windows.h>
#include "color/color_feedback.h"
#include "color/color_state.h"
#include "dialog/dialog_modern.h"

static BOOL ColorFeedback_IsEmpty(const char* input) {
    if (!input) return TRUE;
    while (*input) {
        if (!isspace((unsigned char)*input)) return FALSE;
        input++;
    }
    return TRUE;
}

void ColorFeedback_Evaluate(const char* input, ColorFeedbackResult* result) {
    if (!result) return;
    ZeroMemory(result, sizeof(*result));

    if (ColorFeedback_IsEmpty(input)) {
        result->kind = COLOR_FEEDBACK_EMPTY;
        return;
    }

    if (!NormalizeColorConfigValue(input, result->normalized,
                                   sizeof(result->normalized))) {
        result->kind = COLOR_FEEDBACK_INVALID;
        return;
    }

    if (strchr(result->normalized, '_')) {
        if (GetGradientInfoSnapshotByName(result->normalized,
                                          &result->gradient) != GRADIENT_NONE) {
            result->kind = COLOR_FEEDBACK_GRADIENT;
            return;
        }
    } else if (ColorStringToColorRef(result->normalized,
                                     &result->solidColor)) {
        result->kind = COLOR_FEEDBACK_SOLID;
        return;
    }

    result->normalized[0] = '\0';
    result->kind = COLOR_FEEDBACK_INVALID;
}

BOOL ColorFeedback_IsValid(const ColorFeedbackResult* result) {
    return result && (result->kind == COLOR_FEEDBACK_SOLID ||
                      result->kind == COLOR_FEEDBACK_GRADIENT);
}

static void ColorFeedback_Fill(HDC hdc, const RECT* rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    if (!brush) return;
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
}

static void ColorFeedback_DrawOutline(HDC hdc, const RECT* rect,
                                      int cornerDiameter, COLORREF color,
                                      int width) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom,
              cornerDiameter, cornerDiameter);
    if (oldBrush) SelectObject(hdc, oldBrush);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void ColorFeedback_DrawGradient(HDC hdc, const RECT* rect,
                                       const GradientInfo* gradient,
                                       int cornerDiameter) {
    int saved = SaveDC(hdc);
    HRGN clip = CreateRoundRectRgn(rect->left, rect->top,
                                  rect->right + 1, rect->bottom + 1,
                                  cornerDiameter, cornerDiameter);
    if (saved != 0 && clip) {
        SelectClipRgn(hdc, clip);
        DrawGradientRect(hdc, rect, gradient);
    }
    if (clip) DeleteObject(clip);
    if (saved != 0) RestoreDC(hdc, saved);
}

void ColorFeedback_DrawInline(HWND hwndOwner, const DRAWITEMSTRUCT* item,
                              const ColorFeedbackResult* result,
                              const wchar_t* invalidText) {
    if (!hwndOwner || !item || !result) return;

    DialogModernPalette palette;
    DialogModern_CopyPalette(hwndOwner, &palette);
    RECT bounds = item->rcItem;
    ColorFeedback_Fill(item->hDC, &bounds, palette.surface);

    if (result->kind == COLOR_FEEDBACK_EMPTY) return;

    UINT dpi = DialogModern_GetDpi(hwndOwner);
    HFONT font = (HFONT)SendMessageW(item->hwndItem, WM_GETFONT, 0, 0);
    if (result->kind == COLOR_FEEDBACK_INVALID) {
        RECT textRect = {
            bounds.left + DialogModern_Scale(dpi, 18),
            bounds.top,
            bounds.right - DialogModern_Scale(dpi, 8),
            bounds.bottom
        };
        const wchar_t* message = invalidText && invalidText[0]
            ? invalidText : L"Invalid input format";
        DialogModern_DrawInlineFeedback(
            item->hDC, font, &textRect, message,
            DIALOG_MODERN_FEEDBACK_ERROR, dpi, palette.danger);
        return;
    }

    RECT swatch = {
        bounds.left + DialogModern_Scale(dpi, 1),
        bounds.top + DialogModern_Scale(dpi, 1),
        bounds.right - DialogModern_Scale(dpi, 1),
        bounds.top + DialogModern_Scale(dpi, 17)
    };
    int swatchRadius = DialogModern_Scale(dpi, 8);
    if (result->kind == COLOR_FEEDBACK_GRADIENT) {
        ColorFeedback_DrawGradient(item->hDC, &swatch,
                                   &result->gradient.info,
                                   swatchRadius);
        ColorFeedback_DrawOutline(item->hDC, &swatch, swatchRadius,
                                  palette.border, 1);
    } else {
        DialogModern_DrawRoundedRect(item->hDC, &swatch, swatchRadius,
                                     result->solidColor,
                                     palette.border, 1);
    }

    wchar_t normalized[COLOR_HEX_BUFFER] = {0};
    MultiByteToWideChar(CP_UTF8, 0, result->normalized, -1,
                        normalized, (int)_countof(normalized));
    RECT textRect = {
        bounds.left + DialogModern_Scale(dpi, 18),
        swatch.bottom + DialogModern_Scale(dpi, 2),
        bounds.right - DialogModern_Scale(dpi, 8),
        bounds.bottom
    };
    DialogModern_DrawInlineFeedback(
        item->hDC, font, &textRect, normalized,
        DIALOG_MODERN_FEEDBACK_SUCCESS, dpi, palette.accent);
}
