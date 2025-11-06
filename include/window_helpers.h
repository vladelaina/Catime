/**
 * @file window_helpers.h
 * @brief Window procedure helper functions for dialogs, validation, and timer mode switching
 */

#ifndef WINDOW_HELPERS_H
#define WINDOW_HELPERS_H

#include <windows.h>
#include "timer.h"
#include "language.h"
#include "config_defaults.h"

/* ============================================================================
 * Timer Mode System
 * ============================================================================ */

typedef enum {
    TIMER_MODE_COUNTDOWN,
    TIMER_MODE_COUNTUP,
    TIMER_MODE_SHOW_TIME,
    TIMER_MODE_POMODORO
} TimerMode;

typedef struct {
    int totalSeconds;
    BOOL resetElapsed;
    BOOL showWindow;
    BOOL resetInterval;
} TimerModeParams;

/**
 * @brief Switch timer mode with unified state management
 */
BOOL SwitchTimerMode(HWND hwnd, TimerMode mode, const TimerModeParams* params);

/* ============================================================================
 * Input Validation Framework
 * ============================================================================ */

/** Validator returns TRUE if input valid, writes to output buffer */
typedef BOOL (*InputValidator)(const char* input, void* output);

/**
 * @brief Show input dialog with validation retry loop
 */
BOOL ValidatedInputLoop(HWND hwnd, UINT dialogId, 
                        InputValidator validator, void* output);

/**
 * @brief Time input validator wrapper
 */
BOOL ValidatedTimeInputLoop(HWND hwnd, UINT dialogId, int* outSeconds);

/* ============================================================================
 * Input Dialog System
 * ============================================================================ */

typedef struct {
    const wchar_t* title;
    const wchar_t* prompt;
    const wchar_t* defaultText;
    wchar_t* result;
    size_t maxLen;
} InputBoxParams;

/**
 * @brief Show modal input dialog with customizable parameters
 */
BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen);

/**
 * @brief Dialog procedure for custom input box
 */
INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Show Windows file open dialog
 */
BOOL ShowFilePicker(HWND hwnd, char* selectedPath, size_t bufferSize);

/**
 * @brief Validate file + update config + save to recent files
 */
BOOL ValidateAndSetTimeoutFile(HWND hwnd, const char* filePathUtf8);

/**
 * @brief Get fonts folder path in wide-char format
 */
BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size);

/* ============================================================================
 * System Reset and Initialization
 * ============================================================================ */

/**
 * @brief Reset all timer state to default values
 */
void ResetTimerStateToDefaults(void);

/* DetectSystemLanguage is declared in config_defaults.h */

/**
 * @brief Delete config.ini + create fresh one with defaults
 */
void ResetConfigurationFile(void);

/**
 * @brief Reload font from config
 */
void ReloadDefaultFont(void);

/**
 * @brief Calculate window size (3% of screen height)
 */
void RecalculateWindowSize(HWND hwnd);

/* ============================================================================
 * Timeout Action Helpers
 * ============================================================================ */

/**
 * @brief Set timeout action configuration
 */
void SetTimeoutAction(const char* action);

#endif /* WINDOW_HELPERS_H */

