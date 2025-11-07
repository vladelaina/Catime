/**
 * @file drawing_render.c
 * @brief GDI rendering pipeline with double-buffering
 */

#include <stdio.h>
#include <windows.h>
#include "drawing/drawing_render.h"
#include "drawing/drawing_time_format.h"
#include "drawing.h"
#include "font.h"
#include "color/color.h"
#include "timer/timer.h"
#include "config.h"
#include "window_procedure/window_procedure.h"

extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];
extern char CLOCK_TEXT_COLOR[10];
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;

/**
 * @param colorStr "#RRGGBB" or "R,G,B" format
 * @return COLORREF value, white on parse failure
 */
static COLORREF ParseColorString(const char* colorStr) {
    if (!colorStr || strlen(colorStr) == 0) {
        return RGB(255, 255, 255);
    }
    
    int r = 255, g = 255, b = 255;
    
    if (colorStr[0] == '#' && strlen(colorStr) == 7) {
        sscanf(colorStr + 1, "%02x%02x%02x", &r, &g, &b);
    } else {
        sscanf(colorStr, "%d,%d,%d", &r, &g, &b);
    }
    
    return RGB(r, g, b);
}

/**
 * @return Render context with preview or config settings
 * @note Static buffers avoid per-frame allocation
 */
static RenderContext CreateRenderContext(void) {
    RenderContext ctx;
    
    static char fontFileName[100];
    static char fontInternalName[100];
    static char colorStr[10];
    
    extern void GetActiveFont(char*, char*, size_t);
    extern void GetActiveColor(char*, size_t);
    
    GetActiveFont(fontFileName, fontInternalName, sizeof(fontFileName));
    GetActiveColor(colorStr, sizeof(colorStr));
    
    ctx.fontFileName = fontFileName;
    ctx.fontInternalName = fontInternalName;
    ctx.textColor = ParseColorString(colorStr);
    ctx.fontScaleFactor = CLOCK_FONT_SCALE_FACTOR;
    
    return ctx;
}

/**
 * @param ctx Font configuration
 * @return GDI font handle (must be deleted by caller)
 * @note Negative height = character height (not pixel height)
 */
static HFONT CreateTimerFont(const RenderContext* ctx) {
    wchar_t fontNameW[FONT_NAME_MAX_LEN];
    MultiByteToWideChar(CP_UTF8, 0, ctx->fontInternalName, -1, fontNameW, FONT_NAME_MAX_LEN);
    
    return CreateFontW(
        -(int)(CLOCK_BASE_FONT_SIZE * ctx->fontScaleFactor),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        VARIABLE_PITCH | FF_SWISS,
        fontNameW
    );
}

/** @param editMode TRUE for darker background to improve drag visibility */
static void FillBackground(HDC hdc, const RECT* rect, BOOL editMode) {
    COLORREF bgColor = editMode ? RGB(20, 20, 20) : RGB(0, 0, 0);
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, rect, hBrush);
    DeleteObject(hBrush);
}

/** @note 4-direction outline ensures visibility on any desktop background */
static void RenderTextWithOutline(HDC hdc, const wchar_t* text, int x, int y) {
    size_t textLen = wcslen(text);
    
    SetTextColor(hdc, RGB(0, 0, 0));
    TextOutW(hdc, x - 1, y, text, (int)textLen);
    TextOutW(hdc, x + 1, y, text, (int)textLen);
    TextOutW(hdc, x, y - 1, text, (int)textLen);
    TextOutW(hdc, x, y + 1, text, (int)textLen);
    
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutW(hdc, x, y, text, (int)textLen);
}

/** @note Multiple passes simulate bold when font lacks native bold variant */
static void RenderTextBold(HDC hdc, const wchar_t* text, int x, int y, COLORREF color) {
    size_t textLen = wcslen(text);
    SetTextColor(hdc, color);
    
    for (int i = 0; i < TEXT_RENDER_PASSES; i++) {
        TextOutW(hdc, x, y, text, (int)textLen);
    }
}

static void RenderText(HDC hdc, const RECT* rect, const wchar_t* text, const RenderContext* ctx, BOOL editMode) {
    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &textSize);
    
    int x = (rect->right - textSize.cx) / 2;
    int y = (rect->bottom - textSize.cy) / 2;
    
    if (editMode) {
        RenderTextWithOutline(hdc, text, x, y);
    } else {
        RenderTextBold(hdc, text, x, y, ctx->textColor);
    }
}

/** @note GM_ADVANCED + HALFTONE improve text quality on high-DPI displays */
static void SetupDoubleBuffer(HDC hdc, const RECT* rect, HDC* memDC, HBITMAP* memBitmap, HBITMAP* oldBitmap) {
    *memDC = CreateCompatibleDC(hdc);
    *memBitmap = CreateCompatibleBitmap(hdc, rect->right, rect->bottom);
    *oldBitmap = (HBITMAP)SelectObject(*memDC, *memBitmap);
    
    SetGraphicsMode(*memDC, GM_ADVANCED);
    SetBkMode(*memDC, TRANSPARENT);
    SetStretchBltMode(*memDC, HALFTONE);
    SetBrushOrgEx(*memDC, 0, 0, NULL);
    SetTextAlign(*memDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(*memDC, 0);
    SetMapMode(*memDC, MM_TEXT);
    SetICMMode(*memDC, ICM_ON);
    SetLayout(*memDC, 0);
}

/** @note Skips resize if size unchanged to reduce SetWindowPos overhead */
static void AdjustWindowSize(HWND hwnd, const SIZE* textSize, RECT* rect) {
    if (textSize->cx == (rect->right - rect->left) && 
        textSize->cy == (rect->bottom - rect->top)) {
        return;
    }
    
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);
    
    SetWindowPos(hwnd, NULL,
        windowRect.left, windowRect.top,
        textSize->cx + WINDOW_HORIZONTAL_PADDING,
        textSize->cy + WINDOW_VERTICAL_PADDING,
        SWP_NOZORDER | SWP_NOACTIVATE);
    
    GetClientRect(hwnd, rect);
}

void HandleWindowPaint(HWND hwnd, PAINTSTRUCT* ps) {
    wchar_t timeText[TIME_TEXT_MAX_LEN];
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);
    
    HDC memDC;
    HBITMAP memBitmap, oldBitmap;
    SetupDoubleBuffer(hdc, &rect, &memDC, &memBitmap, &oldBitmap);
    
    GetTimeText(timeText, TIME_TEXT_MAX_LEN);
    
    RenderContext ctx = CreateRenderContext();
    HFONT hFont = CreateTimerFont(&ctx);
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);
    
    FillBackground(memDC, &rect, CLOCK_EDIT_MODE);
    
    if (wcslen(timeText) > 0) {
        SIZE textSize;
        GetTextExtentPoint32W(memDC, timeText, (int)wcslen(timeText), &textSize);
        
        AdjustWindowSize(hwnd, &textSize, &rect);
        RenderText(memDC, &rect, timeText, &ctx, CLOCK_EDIT_MODE);
    }
    
    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
    
    SelectObject(memDC, oldFont);
    DeleteObject(hFont);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

