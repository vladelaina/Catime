/**
 * @file dialog_countdown_internal.h
 * @brief Shared declarations for the custom countdown dialog modules.
 */

#ifndef DIALOG_COUNTDOWN_INTERNAL_H
#define DIALOG_COUNTDOWN_INTERNAL_H

#include "dialog/dialog_input.h"
#include "dialog/dialog_input_internal.h"
#include "dialog/dialog_common.h"
#include "dialog/dialog_modern.h"
#include "dialog/dialog_language.h"
#include "language.h"
#include "timer/timer.h"
#include "utils/localized_duration.h"
#include "utils/string_convert.h"
#include "log.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <stddef.h>
#include <stdlib.h>
#include <strsafe.h>
#include <wchar.h>

#define INPUT_FOCUS_TIMER_ID 9999
#define INPUT_FOCUS_TIMER_DELAY_MS 50
#define COUNTDOWN_WINDOW_CLASS_NAME L"CatimeModernCountdownDialog"
#define COUNTDOWN_CLOSE_BUTTON_ID 0x7F01
#define COUNTDOWN_BUTTON_SUBCLASS_ID 0xC710
#define COUNTDOWN_EDIT_SUBCLASS_ID 0xC711
#define COUNTDOWN_BASE_WIDTH 500
#define COUNTDOWN_BASE_HEIGHT 390

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (WM_USER + 1)
#endif

typedef struct {
    DWORD dialogId;
    int pomodoroTimeIndex;
} CountdownInputState;

typedef enum {
    COUNTDOWN_HOVER_NONE = 0,
    COUNTDOWN_HOVER_CLOSE,
    COUNTDOWN_HOVER_CANCEL,
    COUNTDOWN_HOVER_START,
    COUNTDOWN_HOVER_EDIT
} CountdownHoverPart;

#define COUNTDOWN_TITLE_HOVER_COLOR RGB(0xF7, 0x7D, 0xAA)

typedef struct {
    CountdownInputState input;
    HWND hwndEdit;
    HWND hwndStart;
    HWND hwndCancel;
    HWND hwndClose;
    HFONT titleFont;
    HFONT bodyFont;
    HFONT smallFont;
    HFONT editFont;
    HFONT buttonFont;
    HBRUSH editBrush;
    UINT dpi;
    BOOL darkMode;
    BOOL highContrast;
    BOOL inputValid;
    BOOL showValidationError;
    BOOL selectAllOnNextFocus;
    BOOL sanitizingInput;
    BOOL titleHovered;
    CountdownHoverPart hoverPart;
    CountdownHoverPart pressedPart;
    RECT titleFrame;
    RECT editFrame;
    RECT closeFrame;
    COLORREF backgroundColor;
    COLORREF cardColor;
    COLORREF fieldColor;
    COLORREF borderColor;
    COLORREF textColor;
    COLORREF mutedColor;
    COLORREF accentColor;
    COLORREF accentHoverColor;
    COLORREF dangerColor;
    COLORREF dangerBackgroundColor;
    wchar_t title[128];
    wchar_t formatLabel[64];
    wchar_t fieldLabel[64];
    wchar_t examples[2048];
    wchar_t startText[64];
    wchar_t cancelText[64];
    wchar_t invalidText[256];
    wchar_t previewText[512];
    int exampleColumns;
    int exampleColumnWidths[2];
    int exampleTokenWidths[2];
    int desiredWidth96;
    int exampleVisibleCount;
    BOOL compactLayout;
    BOOL ultraCompactLayout;
} CountdownDialogState;

int CountdownScaleValue(const CountdownDialogState* state, int value);
UINT CountdownGetDpi(HWND hwnd);
void CountdownCopyText(wchar_t* destination, size_t destinationCount,
                       const wchar_t* source, const wchar_t* fallback);
void CountdownCopyEscapedText(wchar_t* destination, size_t destinationCount,
                              const wchar_t* source, const wchar_t* fallback);
void CountdownLoadTexts(CountdownDialogState* state);
void CountdownRefreshPalette(CountdownDialogState* state);
HFONT CountdownCreateFont(UINT dpi, int pixelSize, LONG weight);
void CountdownDestroyFonts(CountdownDialogState* state);
void CountdownBuildFonts(CountdownDialogState* state);

int CountdownMeasureTextPixels(HDC hdc, HFONT font, const wchar_t* text);
BOOL CountdownUpdateTitleHover(HWND hwnd, CountdownDialogState* state,
                               POINT point);
void CountdownRefreshTitleHoverFromCursor(HWND hwnd,
                                          CountdownDialogState* state);
int CountdownMeasureButtonWidth96(HWND hwnd, HFONT font,
                                  const wchar_t* text, int minimum96);
CountdownHoverPart CountdownPartForButton(const CountdownDialogState* state,
                                          HWND hwndButton);
void CountdownTrackMouse(HWND hwnd);
void CountdownTrackNonClientMouse(HWND hwnd);
void CountdownApplyShape(HWND hwnd, const CountdownDialogState* state);
void CountdownCenterEditText(const CountdownDialogState* state);
void CountdownLayout(HWND hwnd, CountdownDialogState* state);

void CountdownDrawRounded(HDC hdc, const RECT* rect, int radius,
                          COLORREF fill, COLORREF border, int borderWidth);
void CountdownDrawText(HDC hdc, HFONT font, COLORREF color,
                       const RECT* rect, const wchar_t* text, UINT format);
void CountdownDrawWrappedText(HDC hdc, HFONT font, COLORREF color,
                              const RECT* rect, const wchar_t* text);
int CountdownSplitExampleLines(const wchar_t* text, wchar_t lines[][256],
                               int maxLines);
void CountdownUpdateResponsiveMode(HWND hwnd, CountdownDialogState* state);
void CountdownUpdateTextMetrics(HWND hwnd, CountdownDialogState* state);
void CountdownEnsureContentWidth(HWND hwnd, CountdownDialogState* state);

void CountdownDrawExamplesGrid(HDC hdc, const CountdownDialogState* state,
                               const RECT* rect);
void CountdownDrawClockIcon(HDC hdc, int centerX, int centerY, int radius,
                            COLORREF color);
void CountdownDrawCheckIcon(HDC hdc, int centerX, int centerY, int radius,
                            COLORREF color);
void CountdownDrawWarningIcon(HDC hdc, int centerX, int centerY, int radius,
                              COLORREF color);

BOOL CountdownIsAllowedInputChar(wchar_t value);
size_t CountdownCopyAllowedInput(wchar_t* destination, size_t destinationCount,
                                 const wchar_t* source);
DWORD CountdownCountAllowedInput(const wchar_t* source, DWORD limit);
void CountdownNormalizeInputKey(const wchar_t* begin, const wchar_t* end,
                                wchar_t* destination, size_t destinationCount);
BOOL CountdownBuildExamplePreview(const CountdownDialogState* state,
                                  const wchar_t* input, wchar_t* destination,
                                  size_t destinationCount);
void CountdownBuildPreviewText(const CountdownDialogState* state,
                               const wchar_t* input, int totalSeconds,
                               wchar_t* destination, size_t destinationCount);
void CountdownSanitizeEditText(HWND hwnd, CountdownDialogState* state);
void CountdownUpdatePreview(HWND hwnd, CountdownDialogState* state);
BOOL CountdownSubmit(HWND hwnd, CountdownDialogState* state);

void CountdownPaint(HWND hwnd, CountdownDialogState* state, HDC target);
void CountdownDrawButton(const DRAWITEMSTRUCT* drawItem,
                         CountdownDialogState* state);

void CountdownMoveFocus(CountdownDialogState* state, HWND current, BOOL reverse);
void CountdownClearChildFocus(HWND hwnd);
LRESULT CALLBACK CountdownEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam, UINT_PTR subclassId,
                                           DWORD_PTR refData);
LRESULT CALLBACK CountdownButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR subclassId,
                                             DWORD_PTR refData);

void CountdownReleaseModifiers(void);
BOOL CountdownRegisterWindowClass(void);
BOOL CountdownCreateControls(HWND hwnd, CountdownDialogState* state);
HWND CreateCustomCountdownDialog(HWND hwndParent);
LRESULT CALLBACK CountdownDialogProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam);

#endif /* DIALOG_COUNTDOWN_INTERNAL_H */
