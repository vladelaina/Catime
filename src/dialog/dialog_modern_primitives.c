/**
 * @file dialog_modern_primitives.c
 * @brief DPI, palette, typography, and GDI primitives for modern dialogs.
 */

#include "dialog/dialog_modern.h"
#include "utils/win32_dynamic_loader.h"
#include <dwmapi.h>
#include <strsafe.h>
#include <wchar.h>

#define MODERN_DWM_CORNER_ATTRIBUTE 33
#define MODERN_DWM_CORNER_ROUND 2

/* Keep the product accent identical across light and dark modern dialogs. */
#define MODERN_ACCENT_COLOR RGB(0x54, 0xAE, 0xFF)
#define MODERN_ACCENT_HOVER_COLOR RGB(0x3C, 0x9A, 0xE8)

UINT DialogModern_GetDpi(HWND hwnd) {
    typedef UINT (WINAPI* GetDpiForWindowFunc)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (hwnd && user32) {
        GetDpiForWindowFunc getDpiForWindowFunc = NULL;
        CATIME_LOAD_PROC_ADDRESS(user32, "GetDpiForWindow",
                                 getDpiForWindowFunc);
        if (getDpiForWindowFunc) {
            UINT dpi = getDpiForWindowFunc(hwnd);
            if (dpi > 0) return dpi;
        }
    }

    HDC hdc = hwnd ? GetDC(hwnd) : GetDC(NULL);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) ReleaseDC(hwnd, hdc);
    return dpi > 0 ? (UINT)dpi : 96u;
}

int DialogModern_Scale(UINT dpi, int value) {
    return MulDiv(value, (int)(dpi ? dpi : 96u), 96);
}

void DialogModern_SetChildRect96(HWND hwndDlg, int controlId, UINT dpi,
                                 int x, int y, int width, int height) {
    HWND control = hwndDlg ? GetDlgItem(hwndDlg, controlId) : NULL;
    if (!control) return;
    SetWindowPos(control, NULL,
                 DialogModern_Scale(dpi, x),
                 DialogModern_Scale(dpi, y),
                 DialogModern_Scale(dpi, width),
                 DialogModern_Scale(dpi, height),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL DialogModern_GetChildRect96(HWND hwndDlg, int controlId, UINT dpi,
                                 RECT* rect) {
    if (!hwndDlg || !rect) return FALSE;
    HWND control = GetDlgItem(hwndDlg, controlId);
    RECT physical = {0};
    if (!control || !GetWindowRect(control, &physical)) return FALSE;
    MapWindowPoints(NULL, hwndDlg, (POINT*)&physical, 2);
    int actualDpi = (int)(dpi ? dpi : 96u);
    rect->left = MulDiv(physical.left, 96, actualDpi);
    rect->top = MulDiv(physical.top, 96, actualDpi);
    rect->right = MulDiv(physical.right, 96, actualDpi);
    rect->bottom = MulDiv(physical.bottom, 96, actualDpi);
    return TRUE;
}

BOOL DialogModern_MeasureText96(HWND hwnd, HFONT font, const wchar_t* text,
                                UINT dpi, SIZE* size) {
    if (!hwnd || !text || !size) return FALSE;
    size->cx = 0;
    size->cy = 0;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return FALSE;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    SIZE physical = {0};
    BOOL measured = GetTextExtentPoint32W(
        hdc, text, (int)wcslen(text), &physical);
    if (oldFont) SelectObject(hdc, oldFont);
    ReleaseDC(hwnd, hdc);
    if (!measured) return FALSE;
    int actualDpi = (int)(dpi ? dpi : 96u);
    size->cx = MulDiv(physical.cx, 96, actualDpi);
    size->cy = MulDiv(physical.cy, 96, actualDpi);
    return TRUE;
}

static int ModernColorLuma(COLORREF color) {
    return (GetRValue(color) * 299 + GetGValue(color) * 587 +
            GetBValue(color) * 114) / 1000;
}

void DialogModern_ResolvePalette(DialogModernPalette* palette) {
    if (!palette) return;
    ZeroMemory(palette, sizeof(*palette));

    HIGHCONTRASTW highContrast = {0};
    highContrast.cbSize = sizeof(highContrast);
    palette->highContrast =
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast),
                              &highContrast, 0) &&
        (highContrast.dwFlags & HCF_HIGHCONTRASTON);
    palette->darkMode = !palette->highContrast &&
                        ModernColorLuma(GetSysColor(COLOR_WINDOW)) < 150;

    if (palette->highContrast) {
        palette->background = GetSysColor(COLOR_WINDOW);
        palette->surface = palette->background;
        palette->field = palette->background;
        palette->border = GetSysColor(COLOR_WINDOWTEXT);
        palette->text = GetSysColor(COLOR_WINDOWTEXT);
        palette->mutedText = palette->text;
        palette->accent = GetSysColor(COLOR_HIGHLIGHT);
        palette->accentHover = palette->accent;
        palette->warning = palette->accent;
        palette->danger = GetSysColor(COLOR_HIGHLIGHTTEXT);
        palette->dangerBackground = GetSysColor(COLOR_HIGHLIGHT);
    } else if (palette->darkMode) {
        palette->background = RGB(0x16, 0x18, 0x1D);
        palette->surface = RGB(0x21, 0x24, 0x2B);
        palette->field = RGB(0x2A, 0x2E, 0x37);
        palette->border = RGB(0x3B, 0x42, 0x4F);
        palette->text = RGB(0xF4, 0xF7, 0xFB);
        palette->mutedText = RGB(0xA9, 0xB2, 0xC0);
        palette->accent = MODERN_ACCENT_COLOR;
        palette->accentHover = MODERN_ACCENT_HOVER_COLOR;
        palette->warning = RGB(0xF4, 0xB9, 0x42);
        palette->danger = RGB(0xFF, 0xA4, 0xA4);
        palette->dangerBackground = RGB(0x4B, 0x2B, 0x31);
    } else {
        palette->background = RGB(0xF3, 0xF5, 0xF9);
        palette->surface = RGB(0xFF, 0xFF, 0xFF);
        palette->field = RGB(0xF3, 0xF5, 0xF8);
        palette->border = RGB(0xD8, 0xDE, 0xE8);
        palette->text = RGB(0x18, 0x22, 0x30);
        palette->mutedText = RGB(0x6D, 0x78, 0x88);
        palette->accent = MODERN_ACCENT_COLOR;
        palette->accentHover = MODERN_ACCENT_HOVER_COLOR;
        palette->warning = RGB(0xB8, 0x68, 0x00);
        palette->danger = RGB(0xC9, 0x3D, 0x4D);
        palette->dangerBackground = RGB(0xFF, 0xEC, 0xEE);
    }
}

HFONT DialogModern_CreateFont(UINT dpi, int pixelSize, LONG weight) {
    LOGFONTW lf = {0};
    lf.lfHeight = -DialogModern_Scale(dpi, pixelSize);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    if (FAILED(StringCchCopyW(lf.lfFaceName, _countof(lf.lfFaceName),
                              L"Segoe UI"))) {
        return NULL;
    }
    return CreateFontIndirectW(&lf);
}

void DialogModern_DrawRoundedRect(HDC hdc, const RECT* rect,
                                  int cornerDiameter, COLORREF fill,
                                  COLORREF border, int borderWidth) {
    if (!hdc || !rect) return;
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = borderWidth > 0 ? CreatePen(PS_SOLID, borderWidth, border)
                               : (HPEN)GetStockObject(NULL_PEN);
    HGDIOBJ oldBrush = brush ? SelectObject(hdc, brush) : NULL;
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    RoundRect(hdc, rect->left, rect->top, rect->right, rect->bottom,
              cornerDiameter, cornerDiameter);
    if (oldPen) SelectObject(hdc, oldPen);
    if (oldBrush) SelectObject(hdc, oldBrush);
    if (borderWidth > 0 && pen) DeleteObject(pen);
    if (brush) DeleteObject(brush);
}

void DialogModern_DrawCloseButton(HDC hdc, const RECT* rect, UINT dpi,
                                  BOOL hovered, BOOL focused,
                                  BOOL highContrast, COLORREF accent,
                                  COLORREF mutedText, COLORREF border) {
    if (!hdc || !rect) return;

    BOOL active = hovered || focused;
    if (active) {
        RECT circle = *rect;
        int inset = DialogModern_Scale(dpi, 1);
        if (inset > 0) InflateRect(&circle, -inset, -inset);
        int width = circle.right - circle.left;
        int height = circle.bottom - circle.top;
        int diameter = width < height ? width : height;
        COLORREF fill = highContrast ? GetSysColor(COLOR_HIGHLIGHT) :
                                        RGB(0xFF, 0xFF, 0xFF);
        COLORREF outline = highContrast ? GetSysColor(COLOR_WINDOWTEXT) :
                                          border;
        DialogModern_DrawRoundedRect(hdc, &circle, diameter,
                                     fill, outline, 1);
    }

    int centerX = (rect->left + rect->right) / 2;
    int centerY = (rect->top + rect->bottom) / 2;
    int arm = (rect->right - rect->left) / 5;
    int penWidth = DialogModern_Scale(dpi, 2);
    if (penWidth < 1) penWidth = 1;
    COLORREF iconColor = active ?
        (highContrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) : accent) :
        (highContrast ? GetSysColor(COLOR_WINDOWTEXT) : mutedText);
    HPEN pen = CreatePen(PS_SOLID, penWidth, iconColor);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    MoveToEx(hdc, centerX - arm, centerY - arm, NULL);
    LineTo(hdc, centerX + arm, centerY + arm);
    MoveToEx(hdc, centerX + arm, centerY - arm, NULL);
    LineTo(hdc, centerX - arm, centerY + arm);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

void DialogModern_DrawText(HDC hdc, HFONT font, COLORREF color,
                           const RECT* rect, const wchar_t* text,
                           UINT format) {
    if (!hdc || !rect || !text) return;
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(hdc, color);
    RECT drawRect = *rect;
    DrawTextW(hdc, text, -1, &drawRect, format | DT_NOPREFIX);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
    if (oldFont) SelectObject(hdc, oldFont);
}

void DialogModern_ApplyWindowShape(HWND hwnd, UINT dpi, int cornerRadius) {
    if (!hwnd) return;
    int preference = MODERN_DWM_CORNER_ROUND;
    HRESULT rounded = DwmSetWindowAttribute(hwnd, MODERN_DWM_CORNER_ATTRIBUTE,
                                             &preference, sizeof(preference));
    if (SUCCEEDED(rounded)) return;

    RECT client = {0};
    GetClientRect(hwnd, &client);
    int radius = DialogModern_Scale(dpi, cornerRadius);
    HRGN region = CreateRoundRectRgn(client.left, client.top,
                                     client.right + 1, client.bottom + 1,
                                     radius * 2, radius * 2);
    if (region && !SetWindowRgn(hwnd, region, TRUE)) {
        DeleteObject(region);
    }
}
