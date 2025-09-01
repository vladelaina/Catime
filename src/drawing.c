/**
 * @file drawing.c
 * @brief Window painting and text rendering functionality
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

/** @brief External elapsed time variable from timer module */
extern int elapsed_time;

/** @brief External time format setting */
extern TimeFormatType CLOCK_TIME_FORMAT;

/** @brief External milliseconds display setting */
extern BOOL CLOCK_SHOW_MILLISECONDS;

/** @brief External milliseconds preview variables */
extern BOOL IS_MILLISECONDS_PREVIEWING;
extern BOOL PREVIEW_SHOW_MILLISECONDS;

/**
 * @brief Get current milliseconds component
 * @return Current milliseconds (0-999)
 */
int GetCurrentMilliseconds(void) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return st.wMilliseconds;
}

/** @brief Timer-based millisecond tracking */
static DWORD timer_start_tick = 0;
static BOOL timer_ms_initialized = FALSE;
static int paused_milliseconds = 0;

/**
 * @brief Reset timer-based millisecond tracking
 * Should be called when timer starts, resumes, or resets
 */
void ResetTimerMilliseconds(void) {
    timer_start_tick = GetTickCount();
    timer_ms_initialized = TRUE;
    paused_milliseconds = 0;
}

/**
 * @brief Save current milliseconds when pausing
 * Should be called when timer is paused to freeze the display
 */
void PauseTimerMilliseconds(void) {
    if (timer_ms_initialized) {
        DWORD current_tick = GetTickCount();
        DWORD elapsed_ms = current_tick - timer_start_tick;
        paused_milliseconds = (int)(elapsed_ms % 1000);
    }
}

/**
 * @brief Get elapsed milliseconds for timer modes
 * @return Current milliseconds component for timer display
 */
int GetElapsedMillisecondsComponent(void) {
    /** If timer is paused, return frozen milliseconds */
    if (CLOCK_IS_PAUSED) {
        return paused_milliseconds;
    }
    
    /** Initialize timer milliseconds on first call */
    if (!timer_ms_initialized) {
        ResetTimerMilliseconds();
        return 0;
    }
    
    /** Calculate elapsed milliseconds since timer start/resume */
    DWORD current_tick = GetTickCount();
    DWORD elapsed_ms = current_tick - timer_start_tick;
    
    /** Return just the millisecond component (0-999) */
    return (int)(elapsed_ms % 1000);
}

/**
 * @brief Handle window painting with double buffering and text rendering
 * @param hwnd Window handle to paint
 * @param ps Paint structure containing device context and paint area
 */
void HandleWindowPaint(HWND hwnd, PAINTSTRUCT *ps) {
    static wchar_t time_text[50];
    HDC hdc = ps->hdc;
    RECT rect;
    GetClientRect(hwnd, &rect);

    /** Create double buffer for flicker-free rendering */
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    /** Configure graphics mode for high-quality rendering */
    SetGraphicsMode(memDC, GM_ADVANCED);
    SetBkMode(memDC, TRANSPARENT);
    SetStretchBltMode(memDC, HALFTONE);
    SetBrushOrgEx(memDC, 0, 0, NULL);

    /** Format time text based on clock mode */
    if (CLOCK_SHOW_CURRENT_TIME) {
        /** Display current system time - use GetLocalTime for accurate synchronization */
        SYSTEMTIME st;
        GetLocalTime(&st);
        
        int hour = st.wHour;
        int minute = st.wMinute;
        int second = st.wSecond;
        int milliseconds = st.wMilliseconds;
        
        /** Convert to 12-hour format if needed */
        if (!CLOCK_USE_24HOUR) {
            if (hour == 0) {
                hour = 12;
            } else if (hour > 12) {
                hour -= 12;
            }
        }

        /** Determine which time format to use (preview or current) */
        TimeFormatType formatToUse = IS_TIME_FORMAT_PREVIEWING ? PREVIEW_TIME_FORMAT : CLOCK_TIME_FORMAT;
        
        /** Determine whether to show milliseconds (preview or current) */
        BOOL showMilliseconds = IS_MILLISECONDS_PREVIEWING ? PREVIEW_SHOW_MILLISECONDS : CLOCK_SHOW_MILLISECONDS;
        
        /** Format with or without seconds */
        if (CLOCK_SHOW_SECONDS) {
            if (showMilliseconds) {
                /** Format with seconds and milliseconds */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                }
            } else {
                /** Format with seconds only */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d", 
                                hour, minute, second);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d", 
                                hour, minute, second);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d:%02d", 
                                hour, minute, second);
                        break;
                }
            }
        } else {
            if (showMilliseconds) {
                /** Format without seconds setting but with milliseconds - show seconds for context */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d:%02d.%03d", 
                                hour, minute, second, milliseconds);
                        break;
                }
            } else {
                /** Format without seconds and without milliseconds */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d", 
                                hour, minute);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d", 
                                hour, minute);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d", 
                                hour, minute);
                        break;
                }
            }
        }
    } else if (CLOCK_COUNT_UP) {
        /** Display count-up timer */
        int hours = countup_elapsed_time / 3600;
        int minutes = (countup_elapsed_time % 3600) / 60;
        int seconds = countup_elapsed_time % 60;

        /** Determine which time format to use (preview or current) */
        TimeFormatType formatToUse = IS_TIME_FORMAT_PREVIEWING ? PREVIEW_TIME_FORMAT : CLOCK_TIME_FORMAT;
        
        /** Determine whether to show milliseconds (preview or current) */
        BOOL showMilliseconds = IS_MILLISECONDS_PREVIEWING ? PREVIEW_SHOW_MILLISECONDS : CLOCK_SHOW_MILLISECONDS;
        
        /** Get milliseconds for timer display */
        int milliseconds = GetElapsedMillisecondsComponent();
        
        /** Format time with appropriate precision */
        if (hours > 0) {
            if (showMilliseconds) {
                /** Format with hours and milliseconds */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                        break;
                }
            } else {
                /** Format with hours only */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d", hours, minutes, seconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d:%02d", hours, minutes, seconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d:%02d", hours, minutes, seconds);
                        break;
                }
            }
        } else if (minutes > 0) {
            if (showMilliseconds) {
                /** Format with minutes and milliseconds */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d.%03d", minutes, seconds, milliseconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"00:%02d:%02d.%03d", minutes, seconds, milliseconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d.%03d", minutes, seconds, milliseconds);
                        break;
                }
            } else {
                /** Format with minutes only */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"%02d:%02d", minutes, seconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"00:%02d:%02d", minutes, seconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d:%02d", minutes, seconds);
                        break;
                }
            }
        } else {
            if (showMilliseconds) {
                /** Format with seconds and milliseconds */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"00:%02d.%03d", seconds, milliseconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"00:00:%02d.%03d", seconds, milliseconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d.%03d", seconds, milliseconds);
                        break;
                }
            } else {
                /** Format with seconds only */
                switch (formatToUse) {
                    case TIME_FORMAT_ZERO_PADDED:
                        swprintf(time_text, 50, L"00:%02d", seconds);
                        break;
                    case TIME_FORMAT_FULL_PADDED:
                        swprintf(time_text, 50, L"00:00:%02d", seconds);
                        break;
                    default: // TIME_FORMAT_DEFAULT
                        swprintf(time_text, 50, L"%d", seconds);
                        break;
                }
            }
        }
    } else {
        /** Display countdown timer */
        int remaining_time = CLOCK_TOTAL_TIME - countdown_elapsed_time;
        if (remaining_time <= 0) {
            /** Handle timeout state */
            if (CLOCK_TOTAL_TIME == 0 && countdown_elapsed_time == 0) {
                time_text[0] = L'\0';
            } else if (strcmp(CLOCK_TIMEOUT_TEXT, "0") == 0) {
                time_text[0] = L'\0';
            } else if (strlen(CLOCK_TIMEOUT_TEXT) > 0) {
                /** Display custom timeout message */
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_TEXT, -1, time_text, 50);
            } else {
                time_text[0] = L'\0';
            }
        } else {
            /** Format remaining time */
            int hours = remaining_time / 3600;
            int minutes = (remaining_time % 3600) / 60;
            int seconds = remaining_time % 60;

            /** Determine which time format to use (preview or current) */
            TimeFormatType formatToUse = IS_TIME_FORMAT_PREVIEWING ? PREVIEW_TIME_FORMAT : CLOCK_TIME_FORMAT;
            
            /** Determine whether to show milliseconds (preview or current) */
            BOOL showMilliseconds = IS_MILLISECONDS_PREVIEWING ? PREVIEW_SHOW_MILLISECONDS : CLOCK_SHOW_MILLISECONDS;
            
            /** Get milliseconds for countdown timer display */
            int milliseconds = GetElapsedMillisecondsComponent();
            
            /** Format with appropriate precision */
            if (hours > 0) {
                if (showMilliseconds) {
                    /** Format with hours and milliseconds */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
                            break;
                    }
                } else {
                    /** Format with hours only */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d:%02d", hours, minutes, seconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d:%02d", hours, minutes, seconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d:%02d:%02d", hours, minutes, seconds);
                            break;
                    }
                }
            } else if (minutes > 0) {
                if (showMilliseconds) {
                    /** Format with minutes and milliseconds */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d.%03d", minutes, seconds, milliseconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"00:%02d:%02d.%03d", minutes, seconds, milliseconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d:%02d.%03d", minutes, seconds, milliseconds);
                            break;
                    }
                } else {
                    /** Format with minutes only */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"%02d:%02d", minutes, seconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"00:%02d:%02d", minutes, seconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d:%02d", minutes, seconds);
                            break;
                    }
                }
            } else {
                if (showMilliseconds) {
                    /** Format with seconds and milliseconds */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"00:%02d.%03d", seconds, milliseconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"00:00:%02d.%03d", seconds, milliseconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d.%03d", seconds, milliseconds);
                            break;
                    }
                } else {
                    /** Format with seconds only */
                    switch (formatToUse) {
                        case TIME_FORMAT_ZERO_PADDED:
                            swprintf(time_text, 50, L"00:%02d", seconds);
                            break;
                        case TIME_FORMAT_FULL_PADDED:
                            swprintf(time_text, 50, L"00:00:%02d", seconds);
                            break;
                        default: // TIME_FORMAT_DEFAULT
                            swprintf(time_text, 50, L"%d", seconds);
                            break;
                    }
                }
            }
        }
    }

    /** Select font based on preview mode */
    const char* fontToUse = IS_PREVIEWING ? PREVIEW_FONT_NAME : FONT_FILE_NAME;
    
    /** Get internal font name and convert to wide char */
    const char* fontInternalName = IS_PREVIEWING ? PREVIEW_INTERNAL_NAME : FONT_INTERNAL_NAME;
    wchar_t fontInternalNameW[256];
    MultiByteToWideChar(CP_UTF8, 0, fontInternalName, -1, fontInternalNameW, 256);
    
    /** Create font with scaling factor applied */
    HFONT hFont = CreateFontW(
        -CLOCK_BASE_FONT_SIZE * CLOCK_FONT_SCALE_FACTOR,
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,   
        VARIABLE_PITCH | FF_SWISS,
        fontInternalNameW
    );
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

    /** Configure text rendering parameters */
    SetTextAlign(memDC, TA_LEFT | TA_TOP);
    SetTextCharacterExtra(memDC, 0);
    SetMapMode(memDC, MM_TEXT);

    /** Enable color management and reset layout */
    DWORD quality = SetICMMode(memDC, ICM_ON);
    SetLayout(memDC, 0);

    /** Parse text color from configuration */
    int r = 255, g = 255, b = 255;
    const char* colorToUse = IS_COLOR_PREVIEWING ? PREVIEW_COLOR : CLOCK_TEXT_COLOR;
    
    if (strlen(colorToUse) > 0) {
        if (colorToUse[0] == '#') {
            /** Parse hex color format (#RRGGBB) */
            if (strlen(colorToUse) == 7) {
                sscanf(colorToUse + 1, "%02x%02x%02x", &r, &g, &b);
            }
        } else {
            /** Parse RGB comma-separated format */
            sscanf(colorToUse, "%d,%d,%d", &r, &g, &b);
        }
    }
    SetTextColor(memDC, RGB(r, g, b));

    /** Fill background based on edit mode */
    if (CLOCK_EDIT_MODE) {
        /** Dark gray background for edit mode visibility */
        HBRUSH hBrush = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(memDC, &rect, hBrush);
        DeleteObject(hBrush);
    } else {
        /** Black background for transparent effect */
        HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &rect, hBrush);
        DeleteObject(hBrush);
    }

    /** Render text if available */
    if (wcslen(time_text) > 0) {
        /** Calculate text dimensions */
        SIZE textSize;
        GetTextExtentPoint32W(memDC, time_text, wcslen(time_text), &textSize);

        /** Resize window to fit text if needed */
        if (textSize.cx != (rect.right - rect.left) || 
            textSize.cy != (rect.bottom - rect.top)) {
            RECT windowRect;
            GetWindowRect(hwnd, &windowRect);
            
            /** Adjust window size with padding */
            SetWindowPos(hwnd, NULL,
                windowRect.left, windowRect.top,
                textSize.cx + WINDOW_HORIZONTAL_PADDING, 
                textSize.cy + WINDOW_VERTICAL_PADDING, 
                SWP_NOZORDER | SWP_NOACTIVATE);
            GetClientRect(hwnd, &rect);
        }

        /** Center text in window */
        int x = (rect.right - textSize.cx) / 2;
        int y = (rect.bottom - textSize.cy) / 2;

        /** Render text with different effects based on mode */
        if (CLOCK_EDIT_MODE) {
            /** Edit mode: white text with black outline */
            SetTextColor(memDC, RGB(255, 255, 255));
            
            /** Draw black outline in four directions */
            SetTextColor(memDC, RGB(0, 0, 0));
            TextOutW(memDC, x-1, y, time_text, wcslen(time_text));
            TextOutW(memDC, x+1, y, time_text, wcslen(time_text));
            TextOutW(memDC, x, y-1, time_text, wcslen(time_text));
            TextOutW(memDC, x, y+1, time_text, wcslen(time_text));
            
            /** Draw white text on top */
            SetTextColor(memDC, RGB(255, 255, 255));
            TextOutW(memDC, x, y, time_text, wcslen(time_text));
        } else {
            /** Normal mode: multiple passes for bold effect */
            SetTextColor(memDC, RGB(r, g, b));
            
            /** Render text multiple times for thickness */
            for (int i = 0; i < 8; i++) {
                TextOutW(memDC, x, y, time_text, wcslen(time_text));
            }
        }
    }

    /** Copy double buffer to screen */
    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    /** Clean up GDI resources */
    SelectObject(memDC, oldFont);
    DeleteObject(hFont);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}