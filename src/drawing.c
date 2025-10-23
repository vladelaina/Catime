/**
 * @file drawing.c
 * @brief Modular window painting and text rendering system
 * 
 * Refactored rendering pipeline with separated concerns:
 * - Time component extraction
 * - Time formatting logic
 * - Font and color management
 * - Double-buffered rendering
 * - Text drawing with effects
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

/* ============================================================================
 * External Dependencies
 * ============================================================================ */

extern int elapsed_time;
extern TimeFormatType CLOCK_TIME_FORMAT;
extern BOOL CLOCK_SHOW_MILLISECONDS;
extern BOOL IS_MILLISECONDS_PREVIEWING;
extern BOOL PREVIEW_SHOW_MILLISECONDS;
extern BOOL IS_TIME_FORMAT_PREVIEWING;
extern TimeFormatType PREVIEW_TIME_FORMAT;
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[100];
extern char PREVIEW_INTERNAL_NAME[100];
extern char FONT_FILE_NAME[100];
extern char FONT_INTERNAL_NAME[100];
extern BOOL IS_COLOR_PREVIEWING;
extern char PREVIEW_COLOR[10];
extern char CLOCK_TEXT_COLOR[10];
extern BOOL CLOCK_EDIT_MODE;
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;

/* ============================================================================
 * Module State - Millisecond Tracking
 * ============================================================================ */

static DWORD g_timer_start_tick = 0;
static BOOL g_timer_ms_initialized = FALSE;
static int g_paused_milliseconds = 0;

/* ============================================================================
 * Millisecond Tracking Functions
 * ============================================================================ */

void ResetTimerMilliseconds(void) {
    g_timer_start_tick = GetTickCount();
    g_timer_ms_initialized = TRUE;
    g_paused_milliseconds = 0;
}

void PauseTimerMilliseconds(void) {
    if (g_timer_ms_initialized) {
        DWORD current_tick = GetTickCount();
        DWORD elapsed_ms = current_tick - g_timer_start_tick;
        g_paused_milliseconds = (int)(elapsed_ms % 1000);
    }
}

/**
 * @brief Get current centiseconds from system time
 * @return Centiseconds (0-99)
 */
static int GetSystemCentiseconds(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return st.wMilliseconds / 10;
}

/**
 * @brief Get elapsed centiseconds for timer modes
 * @return Current centiseconds component (0-99)
 */
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

/* ============================================================================
 * Time Component Extraction
 * ============================================================================ */

/**
 * @brief Get current system time components
 * @param use24Hour TRUE for 24-hour format, FALSE for 12-hour
 * @return Time components structure
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

/**
 * @brief Get count-up timer components
 * @return Time components structure
 */
static TimeComponents GetCountUpComponents(void) {
    TimeComponents tc;
    tc.hours = countup_elapsed_time / 3600;
    tc.minutes = (countup_elapsed_time % 3600) / 60;
    tc.seconds = countup_elapsed_time % 60;
    tc.centiseconds = GetElapsedCentiseconds();
    return tc;
}

/**
 * @brief Get countdown timer components
 * @return Time components structure
 */
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

/* ============================================================================
 * Time Formatting Logic
 * ============================================================================ */

/**
 * @brief Format time components to display string
 * @param tc Time components to format
 * @param format Time format type
 * @param showMilliseconds TRUE to show centiseconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size in wide characters
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
 * @brief Get final time text for display
 * @param buffer Output buffer for formatted time
 * @param bufferSize Buffer size in wide characters
 */
static void GetTimeText(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    TimeFormatType finalFormat = IS_TIME_FORMAT_PREVIEWING ? PREVIEW_TIME_FORMAT : CLOCK_TIME_FORMAT;
    BOOL finalShowMs = IS_MILLISECONDS_PREVIEWING ? PREVIEW_SHOW_MILLISECONDS : CLOCK_SHOW_MILLISECONDS;
    
    if (CLOCK_SHOW_CURRENT_TIME) {
        TimeComponents tc = GetCurrentTimeComponents(CLOCK_USE_24HOUR);
        
        if (CLOCK_SHOW_SECONDS) {
            FormatTimeComponents(&tc, finalFormat, finalShowMs, buffer, bufferSize);
        } else {
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

/* ============================================================================
 * Rendering Context Management
 * ============================================================================ */

/**
 * @brief Parse color string to COLORREF
 * @param colorStr Color string in "#RRGGBB" or "R,G,B" format
 * @return COLORREF value
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
 * @brief Create rendering context based on preview mode
 * @return Rendering context structure
 */
static RenderContext CreateRenderContext(void) {
    RenderContext ctx;
    
    if (IS_PREVIEWING) {
        ctx.fontFileName = PREVIEW_FONT_NAME;
        ctx.fontInternalName = PREVIEW_INTERNAL_NAME;
    } else {
        ctx.fontFileName = FONT_FILE_NAME;
        ctx.fontInternalName = FONT_INTERNAL_NAME;
    }
    
    const char* colorStr = IS_COLOR_PREVIEWING ? PREVIEW_COLOR : CLOCK_TEXT_COLOR;
    ctx.textColor = ParseColorString(colorStr);
    ctx.fontScaleFactor = CLOCK_FONT_SCALE_FACTOR;
    
    return ctx;
}

/**
 * @brief Create font for timer display
 * @param ctx Rendering context
 * @return Font handle (caller must delete)
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

/* ============================================================================
 * Drawing Functions
 * ============================================================================ */

/**
 * @brief Fill window background
 * @param hdc Device context
 * @param rect Rectangle to fill
 * @param editMode TRUE if in edit mode
 */
static void FillBackground(HDC hdc, const RECT* rect, BOOL editMode) {
    COLORREF bgColor = editMode ? RGB(20, 20, 20) : RGB(0, 0, 0);
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, rect, hBrush);
    DeleteObject(hBrush);
}

/**
 * @brief Render text with outline effect
 * @param hdc Device context
 * @param text Text to render
 * @param x X coordinate
 * @param y Y coordinate
 */
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

/**
 * @brief Render text with bold effect
 * @param hdc Device context
 * @param text Text to render
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Text color
 */
static void RenderTextBold(HDC hdc, const wchar_t* text, int x, int y, COLORREF color) {
    size_t textLen = wcslen(text);
    SetTextColor(hdc, color);
    
    for (int i = 0; i < TEXT_RENDER_PASSES; i++) {
        TextOutW(hdc, x, y, text, (int)textLen);
    }
}

/**
 * @brief Render time text to device context
 * @param hdc Device context
 * @param rect Client rectangle
 * @param text Text to render
 * @param ctx Rendering context
 * @param editMode TRUE if in edit mode
 */
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

/**
 * @brief Setup double buffering context
 * @param hdc Target device context
 * @param rect Client rectangle
 * @param memDC Output memory DC
 * @param memBitmap Output memory bitmap
 * @param oldBitmap Output old bitmap
 */
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

/**
 * @brief Adjust window size to fit text
 * @param hwnd Window handle
 * @param textSize Text dimensions
 * @param rect Updated client rectangle
 */
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

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

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
