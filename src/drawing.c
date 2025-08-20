#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../include/drawing.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/timer.h"
#include "../include/config.h"

extern int elapsed_time;

void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps) {
    static wchar_t time_text[50];
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    SetGraphicsMode(memDC, GM_ADVANCED);
    SetBkMode(memDC, TRANSPARENT);
    SetStretchBltMode(memDC, HALFTONE);
    SetBrushOrgEx(memDC, 0, 0, NULL);

    if (CLOCK_SHOW_CURRENT_TIME) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        int hour = tm_info->tm_hour;
        
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;
            } else if (hour > 12) {
                hour -= 12;
            }
        }

        if (CLOCK_SHOW_SECONDS) {
            swprintf(time_text, 50, L"%d:%02d:%02d", 
                    hour, tm_info->tm_min, tm_info->tm_sec);
        } else {
            swprintf(time_text, 50, L"%d:%02d", 
                    hour, tm_info->tm_min);
        }
    } else if (CLOCK_COUNT_UP) {
        int hours = countup_elapsed_time / 3600;
        int minutes = (countup_elapsed_time % 3600) / 60;
        int seconds = countup_elapsed_time % 60;

        if (hours > 0) {
            swprintf(time_text, 50, L"%d:%02d:%02d", hours, minutes, seconds);
        } else if (minutes > 0) {
            swprintf(time_text, 50, L"%d:%02d", minutes, seconds);
        } else {
            swprintf(time_text, 50, L"%d", seconds);
        }
    } else {
        int remaining_time = CLOCK_TOTAL_TIME - countdown_elapsed_time;
        if (remaining_time <= 0) {
            if (CLOCK_TOTAL_TIME == 0 && countdown_elapsed_time == 0) {
                time_text[0] = L'\0';
            } else if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                time_text[0] = L'\0';
            } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_TEXT, -1, time_text, 50);
            } else {
                time_text[0] = L'\0';
            }
        } else {
            int hours = remaining_time / 3600;
            int minutes = (remaining_time % 3600) / 60;
            int seconds = remaining_time % 60;

            if (hours > 0) {
                swprintf(time_text, 50, L"%d:%02d:%02d", hours, minutes, seconds);
            } else if (minutes > 0) {
                swprintf(time_text, 50, L"%d:%02d", minutes, seconds);
            } else {
                swprintf(time_text, 50, L"%d", seconds);
            }
        }
    }

    const char* fontToUse = IS_PREVIEWING ? PREVIEW_FONT_NAME : FONT_FILE_NAME;
    
    const char* fontInternalName = IS_PREVIEWING ? PREVIEW_INTERNAL_NAME : FONT_INTERNAL_NAME;
    wchar_t fontInternalNameW[256];
    MultiByteToWideChar(CP_UTF8, 0, fontInternalName, -1, fontInternalNameW, 256);
    
    HFONT hFont = CreateFontW(
        -CLOCK_BASE_FONT_SIZE * CLOCK_FONT_SCALE_FACTOR,
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,   
        VARIABLE_PITCH | FF_SWISS,
        fontInternalNameW
    );
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

    SetTextAlign(memDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(memDC, 0);
    SetMapMode(memDC, MM_TEXT);

    DWORD quality = SetICMMode(memDC, ICM_ON);
    SetLayout(memDC, 0);

    int r = 255, g = 255, b = 255;
    const char* colorToUse = IS_COLOR_PREVIEWING ? PREVIEW_COLOR : CLOCK_TEXT_COLOR;
    
    if (strlen(colorToUse) > 0) {
        if (colorToUse[0] == '#') {
            if (strlen(colorToUse) == 7) {
                sscanf(colorToUse + 1, "%02x%02x%02x", &r, &g, &b);
            }
        } else {
            sscanf(colorToUse, "%d,%d,%d", &r, &g, &b);
        }
    }
    SetTextColor(memDC, RGB(r, g, b));

    if (CLOCK_EDIT_MODE) {
        HBRUSH hBrush = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(memDC, &rect, hBrush);
        DeleteObject(hBrush);
    } else {
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &rect, hBrush);
        DeleteObject(hBrush);
    }

    if (wcslen(time_text) > 0) {
        SIZE textSize;
        GetTextExtentPoint32W(memDC, time_text, wcslen(time_text), &textSize);

        if (textSize.cx != (rect.right - rect.left) || 
            textSize.cy != (rect.bottom - rect.top)) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            
            SetWindowPos(hwnd, NULL,
                windowRect.left, windowRect.top,
                textSize.cx + WINDOW_HORIZONTAL_PADDING, 
                textSize.cy + WINDOW_VERTICAL_PADDING, 
                SWP_NOZORDER | SWP_NOACTIVATE);
            GetClientRect(hwnd, &rect);
        }

        int x = (rect.right - textSize.cx) / 2;
        int y = (rect.bottom - textSize.cy) / 2;

        if (CLOCK_EDIT_MODE) {
            SetTextColor(memDC, RGB(255, 255, 255));
            
            SetTextColor(memDC, RGB(0, 0, 0));
            TextOutW(memDC, x-1, y, time_text, wcslen(time_text));
            TextOutW(memDC, x+1, y, time_text, wcslen(time_text));
            TextOutW(memDC, x, y-1, time_text, wcslen(time_text));
            TextOutW(memDC, x, y+1, time_text, wcslen(time_text));
            
            SetTextColor(memDC, RGB(255, 255, 255));
            TextOutW(memDC, x, y, time_text, wcslen(time_text));
        } else {
            SetTextColor(memDC, RGB(r, g, b));
            
            for (int i = 0; i < 8; i++) {
                TextOutW(memDC, x, y, time_text, wcslen(time_text));
            }
        }
    }

    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}