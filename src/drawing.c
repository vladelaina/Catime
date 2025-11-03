/**
 * @file drawing.c
 * @brief Window painting with double-buffering and auto-resize
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../include/drawing.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/timer.h"
#include "../include/config.h"
#include "../include/window_procedure.h"

extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];
extern char CLOCK_TEXT_COLOR[10];
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;

/** High-resolution timer state (sub-second precision) */
static DWORD g_timer_start_tick = 0;
static BOOL g_timer_ms_initialized = FALSE;
static int g_paused_milliseconds = 0;

void ResetTimerMilliseconds(void) {
    g_timer_start_tick = GetTickCount();
    g_timer_ms_initialized = TRUE;
    g_paused_milliseconds = 0;
}

/** Capture milliseconds to prevent display jumps on resume */
void PauseTimerMilliseconds(void) {
    if (g_timer_ms_initialized) {
        DWORD current_tick = GetTickCount();
        DWORD elapsed_ms = current_tick - g_timer_start_tick;
        g_paused_milliseconds = (int)(elapsed_ms % 1000);
    }
}

static int GetSystemCentiseconds(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return st.wMilliseconds / 10;
}

/** @return Elapsed centiseconds, frozen during pause to prevent visual jumps */
static int GetElapsedCentiseconds(void) {
    if (CLOCK_IS_PAUSED) {
        return g_paused_milliseconds / 10;
    }
    
    if (!g_timer_ms_initialized) {
        ResetTimerMilliseconds();
        return 0;
    }
    
    DWORD current_tick = GetTickCount();
    DWORD elapsed_ms = current_tick - g_timer_start_tick;
    return (int)((elapsed_ms % 1000) / 10);
}

/**
 * @param use24Hour FALSE converts to 12-hour format
 * @return Current system time components
 */
static TimeComponents GetCurrentTimeComponents(BOOL use24Hour) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    TimeComponents tc;
    tc.hours = st.wHour;
    tc.minutes = st.wMinute;
    tc.seconds = st.wSecond;
    tc.centiseconds = st.wMilliseconds / 10;
    
    if (!use24Hour) {
        if (tc.hours == 0) {
            tc.hours = 12;
        } else if (tc.hours > 12) {
            tc.hours -= 12;
        }
    }
    
    return tc;
}

static TimeComponents GetCountUpComponents(void) {
    TimeComponents tc;
    tc.hours = countup_elapsed_time / 3600;
    tc.minutes = (countup_elapsed_time % 3600) / 60;
    tc.seconds = countup_elapsed_time % 60;
    tc.centiseconds = GetElapsedCentiseconds();
    return tc;
}

/** @return Remaining time, clamped to zero to avoid negative display */
static TimeComponents GetCountDownComponents(void) {
    int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
    if (remaining < 0) remaining = 0;
    
    TimeComponents tc;
    tc.hours = remaining / 3600;
    tc.minutes = (remaining % 3600) / 60;
    tc.seconds = remaining % 60;
    tc.centiseconds = GetElapsedCentiseconds();
    return tc;
}

/**
 * Format time components with adaptive zero-padding
 * @param tc Time components to format
 * @param format Zero-padding strategy
 * @param showMilliseconds TRUE to append centiseconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size in characters
 * @note Hides leading zeros for brevity (9:59 vs 00:09:59)
 */
static void FormatTimeComponents(
    const TimeComponents* tc,
    TimeFormatType format,
    BOOL showMilliseconds,
    wchar_t* buffer,
    size_t bufferSize
) {
    if (!tc || !buffer || bufferSize == 0) return;
    
    if (tc->hours > 0) {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d:%02d:%02d.%02d", 
                            tc->hours, tc->minutes, tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d:%02d:%02d", 
                            tc->hours, tc->minutes, tc->seconds);
                    break;
            }
        }
    } else if (tc->minutes > 0) {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"00:%02d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d:%02d.%02d", 
                            tc->minutes, tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"%02d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"00:%02d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d:%02d", 
                            tc->minutes, tc->seconds);
                    break;
            }
        }
    } else {
        if (showMilliseconds) {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"00:%02d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"00:00:%02d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d.%02d", 
                            tc->seconds, tc->centiseconds);
                    break;
            }
        } else {
            switch (format) {
                case TIME_FORMAT_ZERO_PADDED:
                    swprintf(buffer, bufferSize, L"00:%02d", tc->seconds);
                    break;
                case TIME_FORMAT_FULL_PADDED:
                    swprintf(buffer, bufferSize, L"00:00:%02d", tc->seconds);
                    break;
                default:
                    swprintf(buffer, bufferSize, L"%d", tc->seconds);
                    break;
            }
        }
    }
}

/**
 * Generate display text for current timer mode
 * @param buffer Output buffer
 * @param bufferSize Buffer size
 * @note Uses preview settings if active, otherwise config values
 */
static void GetTimeText(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    TimeFormatType finalFormat = GetActiveTimeFormat();
    BOOL finalShowMs = GetActiveShowMilliseconds();
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        TimeComponents tc = GetCurrentTimeComponents(CLOCK_USE_24HOUR);
        
        if (CLOCK_SHOW_SECONDS) {
            FormatTimeComponents(&tc, finalFormat, finalShowMs, buffer, bufferSize);
        } else {
            /** Milliseconds override seconds hiding */
            if (finalShowMs) {
                FormatTimeComponents(&tc, finalFormat, finalShowMs, buffer, bufferSize);
            } else {
                if (finalFormat == TIME_FORMAT_ZERO_PADDED || finalFormat == TIME_FORMAT_FULL_PADDED) {
                    swprintf(buffer, bufferSize, L"%02d:%02d", tc.hours, tc.minutes);
                } else {
                    swprintf(buffer, bufferSize, L"%d:%02d", tc.hours, tc.minutes);
                }
            }
        }
    } else if (CLOCK_COUNT_UP) {
        TimeComponents tc = GetCountUpComponents();
        FormatTimeComponents(&tc, finalFormat, finalShowMs, buffer, bufferSize);
    } else {
        int remaining = CLOCK_TOTAL_TIME - countdown_elapsed_time;
        
        if (remaining <= 0) {
            /** Empty timeout text hides window */
            if (CLOCK_TOTAL_TIME == 0 && countdown_elapsed_time == 0) {
                buffer[0] = L'\0';
            } else if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                buffer[0] = L'\0';
            } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_TEXT, -1, buffer, (int)bufferSize);
            } else {
                buffer[0] = L'\0';
            }
        } else {
            TimeComponents tc = GetCountDownComponents();
            FormatTimeComponents(&tc, finalFormat, finalShowMs, buffer, bufferSize);
        }
    }
}

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

/**
 * Main paint handler for WM_PAINT
 * @param hwnd Window handle
 * @param ps Paint structure from BeginPaint
 * @note Double-buffering eliminates flicker
 * @note Window auto-resizes to fit text
 */
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
