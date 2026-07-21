/**
 * @file dialog_modern.h
 * @brief Lightweight modern drawing and hosting for Catime dialogs.
 */

#ifndef DIALOG_MODERN_H
#define DIALOG_MODERN_H

#include <windows.h>

typedef struct {
    BOOL darkMode;
    BOOL highContrast;
    COLORREF background;
    COLORREF surface;
    COLORREF field;
    COLORREF border;
    COLORREF text;
    COLORREF mutedText;
    COLORREF accent;
    COLORREF accentHover;
    COLORREF warning;
    COLORREF danger;
    COLORREF dangerBackground;
} DialogModernPalette;

typedef enum {
    DIALOG_MODERN_FEEDBACK_SUCCESS = 0,
    DIALOG_MODERN_FEEDBACK_ERROR
} DialogModernFeedbackKind;

/** Attach modern chrome and control styling to a resource dialog. */
BOOL DialogModern_Attach(HWND hwndDlg, int dialogType);

/** Return TRUE when the resource dialog already has a modern host. */
BOOL DialogModern_IsAttached(HWND hwndDlg);

/** Re-read system colors and redraw the dialog and its children. */
void DialogModern_Refresh(HWND hwndDlg);

/** Shared DPI and design-token helpers used by specialized dialogs. */
UINT DialogModern_GetDpi(HWND hwnd);
int DialogModern_Scale(UINT dpi, int value);
void DialogModern_SetChildRect96(HWND hwndDlg, int controlId, UINT dpi,
                                 int x, int y, int width, int height);
BOOL DialogModern_GetChildRect96(HWND hwndDlg, int controlId, UINT dpi,
                                 RECT* rect);
BOOL DialogModern_MeasureText96(HWND hwnd, HFONT font, const wchar_t* text,
                                UINT dpi, SIZE* size);

void DialogModern_ResolvePalette(DialogModernPalette* palette);
BOOL DialogModern_CopyPalette(HWND hwnd, DialogModernPalette* palette);
HFONT DialogModern_CreateFont(UINT dpi, int pixelSize, LONG weight);

/** Shared GDI primitives. Coordinates are physical pixels. */
void DialogModern_DrawRoundedRect(HDC hdc, const RECT* rect, int cornerDiameter,
                                  COLORREF fill, COLORREF border,
                                  int borderWidth);
/** Draw the shared transparent close button X icon. */
void DialogModern_DrawCloseButton(HDC hdc, const RECT* rect, UINT dpi,
                                  BOOL hovered, BOOL focused,
                                  BOOL highContrast, COLORREF accent,
                                  COLORREF mutedText);
void DialogModern_DrawText(HDC hdc, HFONT font, COLORREF color,
                           const RECT* rect, const wchar_t* text,
                           UINT format);
/** Draw the same icon-and-text feedback used by the countdown input. */
void DialogModern_DrawInlineFeedback(HDC hdc, HFONT font, const RECT* textRect,
                                     const wchar_t* text,
                                     DialogModernFeedbackKind kind,
                                     UINT dpi, COLORREF color);
/** Mark a shared modern field as invalid so its outline uses the danger color. */
void DialogModern_SetFieldInvalid(HWND hwndField, BOOL invalid);
/** Apply the shared light/dark appearance to a window and native children. */
void DialogModern_ApplyTheme(HWND hwnd, BOOL darkMode);
void DialogModern_ApplyWindowShape(HWND hwnd, UINT dpi, int cornerRadius);

#endif /* DIALOG_MODERN_H */
