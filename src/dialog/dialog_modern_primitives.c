/**
 * @file dialog_modern_primitives.c
 * @brief DPI, palette, typography, and GDI primitives for modern dialogs.
 */

#include "dialog/dialog_modern.h"
#include "tray/tray_menu_theme.h"
#include "utils/win32_dynamic_loader.h"
#include <dwmapi.h>
#include <strsafe.h>
#include <wchar.h>

#define MODERN_DWM_CORNER_ATTRIBUTE 33
#define MODERN_DWM_CORNER_ROUND 2
#define MODERN_THEME_MODE_PROP L"Catime.DialogThemeMode"

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
                        IsApplicationDarkModeActive();

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
                                  COLORREF mutedText) {
    if (!hdc || !rect) return;

    BOOL active = hovered || focused;
    int centerX = (rect->left + rect->right) / 2;
    int centerY = (rect->top + rect->bottom) / 2;
    int arm = (rect->right - rect->left) / 5;
    int penWidth = DialogModern_Scale(dpi, 2);
    if (penWidth < 1) penWidth = 1;
    COLORREF iconColor = active ? accent :
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

static COLORREF DialogModernBlendColor(COLORREF from, COLORREF to,
                                       int toPercent) {
    if (toPercent < 0) toPercent = 0;
    if (toPercent > 100) toPercent = 100;
    int fromPercent = 100 - toPercent;
    return RGB((GetRValue(from) * fromPercent + GetRValue(to) * toPercent) / 100,
               (GetGValue(from) * fromPercent + GetGValue(to) * toPercent) / 100,
               (GetBValue(from) * fromPercent + GetBValue(to) * toPercent) / 100);
}

static void DialogModernDrawBezierStroke(HDC hdc, const POINT points[4],
                                         int width, COLORREF color) {
    LOGBRUSH brush = {0};
    brush.lbStyle = BS_SOLID;
    brush.lbColor = color;
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND |
                                PS_JOIN_ROUND,
                            (DWORD)(width > 0 ? width : 1), &brush, 0, NULL);
    if (!pen) pen = CreatePen(PS_SOLID, width > 0 ? width : 1, color);
    HGDIOBJ oldPen = pen ? SelectObject(hdc, pen) : NULL;
    if (pen) PolyBezier(hdc, points, 4);
    if (oldPen) SelectObject(hdc, oldPen);
    if (pen) DeleteObject(pen);
}

static void DialogModernScaleSignaturePoints(const POINT source[4],
                                             POINT target[4],
                                             int originX, int originY,
                                             int scale) {
    for (int i = 0; i < 4; i++) {
        target[i].x = (source[i].x - originX) * scale;
        target[i].y = (source[i].y - originY) * scale;
    }
}

void DialogModern_DrawTitleSignature(HDC hdc, const RECT* titleRect, UINT dpi,
                                     int titleTextWidth, COLORREF accent,
                                     COLORREF surface, BOOL darkMode,
                                     BOOL highContrast) {
    if (!hdc || !titleRect) return;

    int minimumWidth = DialogModern_Scale(dpi, 86);
    int maximumWidth = DialogModern_Scale(dpi, 188);
    int width = titleTextWidth + DialogModern_Scale(dpi, 16);
    if (width < minimumWidth) width = minimumWidth;
    if (width > maximumWidth) width = maximumWidth;
    int available = titleRect->right - titleRect->left;
    if (width > available) width = available;
    if (width < DialogModern_Scale(dpi, 28)) return;

    int x = titleRect->left + DialogModern_Scale(dpi, 2);
    int y = titleRect->bottom - DialogModern_Scale(dpi, 1);
    int softWidth = DialogModern_Scale(dpi, highContrast ? 2 : 7);
    int mainWidth = DialogModern_Scale(dpi, highContrast ? 2 : 4);
    COLORREF leading = highContrast ? accent :
        DialogModernBlendColor(accent, RGB(0xA8, 0xEC, 0xFF),
                               darkMode ? 38 : 58);
    COLORREF glow = highContrast ? accent :
        DialogModernBlendColor(surface, leading, darkMode ? 24 : 34);

    POINT lead[4] = {
        {x, y},
        {x + width * 22 / 100, y + DialogModern_Scale(dpi, 4)},
        {x + width * 48 / 100, y + DialogModern_Scale(dpi, 5)},
        {x + width * 69 / 100, y - DialogModern_Scale(dpi, 1)}
    };
    POINT flourish[4] = {
        {x + width * 67 / 100, y - DialogModern_Scale(dpi, 1)},
        {x + width * 50 / 100, y + DialogModern_Scale(dpi, 11)},
        {x + width * 79 / 100, y + DialogModern_Scale(dpi, 8)},
        {x + width, y - DialogModern_Scale(dpi, 5)}
    };

    POINT airStroke[4] = {
        {x + DialogModern_Scale(dpi, 3),
         y - DialogModern_Scale(dpi, 3)},
        {x + width * 12 / 100,
         y - DialogModern_Scale(dpi, 2)},
        {x + width * 20 / 100,
         y - DialogModern_Scale(dpi, 2)},
        {x + width * 29 / 100,
         y - DialogModern_Scale(dpi, 3)}
    };

    /* Render this small decorative region at 3x and downsample it.  GDI's
     * direct PolyBezier rasterizer is visibly jagged at 96 DPI; supersampling
     * keeps the implementation dependency-free while producing smooth edges. */
    const int sampleScale = highContrast ? 1 : 3;
    int paddingX = DialogModern_Scale(dpi, 5);
    int topPadding = DialogModern_Scale(dpi, 9);
    int bottomPadding = DialogModern_Scale(dpi, 16);
    RECT bounds = {x - paddingX, y - topPadding,
                   x + width + paddingX, y + bottomPadding};
    int outputWidth = bounds.right - bounds.left;
    int outputHeight = bounds.bottom - bounds.top;
    HDC sampleDc = sampleScale > 1 ? CreateCompatibleDC(hdc) : NULL;
    HBITMAP sampleBitmap = sampleDc ? CreateCompatibleBitmap(
        hdc, outputWidth * sampleScale, outputHeight * sampleScale) : NULL;
    HGDIOBJ oldBitmap = sampleBitmap
        ? SelectObject(sampleDc, sampleBitmap) : NULL;
    HDC drawDc = sampleBitmap ? sampleDc : hdc;

    POINT scaledLead[4];
    POINT scaledFlourish[4];
    POINT scaledAirStroke[4];
    const POINT* drawLead = lead;
    const POINT* drawFlourish = flourish;
    const POINT* drawAirStroke = airStroke;
    int drawScale = 1;
    if (sampleBitmap) {
        RECT sampleRect = {0, 0, outputWidth * sampleScale,
                           outputHeight * sampleScale};
        HBRUSH surfaceBrush = CreateSolidBrush(surface);
        FillRect(sampleDc, &sampleRect, surfaceBrush);
        DeleteObject(surfaceBrush);
        DialogModernScaleSignaturePoints(lead, scaledLead,
                                         bounds.left, bounds.top, sampleScale);
        DialogModernScaleSignaturePoints(flourish, scaledFlourish,
                                         bounds.left, bounds.top, sampleScale);
        DialogModernScaleSignaturePoints(airStroke, scaledAirStroke,
                                         bounds.left, bounds.top, sampleScale);
        drawLead = scaledLead;
        drawFlourish = scaledFlourish;
        drawAirStroke = scaledAirStroke;
        drawScale = sampleScale;
    }

    if (!highContrast) {
        DialogModernDrawBezierStroke(
            drawDc, drawAirStroke, DialogModern_Scale(dpi, 2) * drawScale,
            glow);
        DialogModernDrawBezierStroke(drawDc, drawLead,
                                     softWidth * drawScale, glow);
        DialogModernDrawBezierStroke(drawDc, drawFlourish,
                                     softWidth * drawScale, glow);
    }
    DialogModernDrawBezierStroke(drawDc, drawLead,
                                 mainWidth * drawScale, leading);
    DialogModernDrawBezierStroke(drawDc, drawFlourish,
                                 mainWidth * drawScale, accent);

    if (sampleBitmap) {
        int oldMode = SetStretchBltMode(hdc, HALFTONE);
        POINT oldOrigin = {0};
        SetBrushOrgEx(hdc, bounds.left, bounds.top, &oldOrigin);
        StretchBlt(hdc, bounds.left, bounds.top, outputWidth, outputHeight,
                   sampleDc, 0, 0, outputWidth * sampleScale,
                   outputHeight * sampleScale, SRCCOPY);
        SetBrushOrgEx(hdc, oldOrigin.x, oldOrigin.y, NULL);
        SetStretchBltMode(hdc, oldMode);
        SelectObject(sampleDc, oldBitmap);
        DeleteObject(sampleBitmap);
    }
    if (sampleDc) DeleteDC(sampleDc);
}

typedef HRESULT (WINAPI *DialogModernSetWindowThemeFn)(
    HWND hwnd, LPCWSTR subAppName, LPCWSTR subIdList);

static DialogModernSetWindowThemeFn g_setWindowTheme = NULL;
static BOOL g_setWindowThemeResolved = FALSE;

static void DialogModernResolveWindowThemeFunction(void) {
    HMODULE uxtheme = GetModuleHandleW(L"uxtheme.dll");
    if (!uxtheme) uxtheme = LoadLibraryW(L"uxtheme.dll");
    if (uxtheme) {
        CATIME_LOAD_PROC_ADDRESS(uxtheme, "SetWindowTheme", g_setWindowTheme);
    }
    g_setWindowThemeResolved = TRUE;
}

typedef struct {
    BOOL darkMode;
} DialogModernThemeChildrenContext;

static BOOL CALLBACK DialogModernApplyThemeToChild(HWND child, LPARAM data) {
    const DialogModernThemeChildrenContext* context =
        (const DialogModernThemeChildrenContext*)data;
    if (context) {
        DialogModern_ApplyTheme(child, context->darkMode);
    }
    return TRUE;
}

void DialogModern_ApplyTheme(HWND hwnd, BOOL darkMode) {
    INT_PTR desiredMode = darkMode ? 2 : 1;
    if (!hwnd) return;
    BOOL rootWindow = GetAncestor(hwnd, GA_ROOT) == hwnd;
    BOOL modeChanged =
        (INT_PTR)GetPropW(hwnd, MODERN_THEME_MODE_PROP) != desiredMode;
    if (modeChanged) {
        SetPropW(hwnd, MODERN_THEME_MODE_PROP, (HANDLE)desiredMode);
        ApplyNativeMenuThemeToWindow(hwnd);
        if (!g_setWindowThemeResolved) {
            DialogModernResolveWindowThemeFunction();
        }
        if (g_setWindowTheme) {
            const wchar_t* themeName = NULL;
            if (darkMode) {
                wchar_t className[64] = {0};
                GetClassNameW(hwnd, className, (int)_countof(className));
                themeName = (_wcsicmp(className, L"ComboBox") == 0 ||
                             _wcsicmp(className, L"msctls_updown32") == 0)
                    ? L"DarkMode_CFD"
                    : L"DarkMode_Explorer";
            }
            (void)g_setWindowTheme(hwnd, themeName, NULL);
        }
    }

    if (rootWindow) {
        if (modeChanged) {
            BOOL enabled = darkMode;
            HRESULT result = DwmSetWindowAttribute(
                hwnd, 20, &enabled, sizeof(enabled));
            if (FAILED(result)) {
                (void)DwmSetWindowAttribute(hwnd, 19, &enabled,
                                             sizeof(enabled));
            }
        }
        DialogModernThemeChildrenContext context = {darkMode};
        EnumChildWindows(hwnd, DialogModernApplyThemeToChild,
                         (LPARAM)&context);
    }
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
