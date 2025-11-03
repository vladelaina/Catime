/**
 * @file dialog_common.h
 * @brief Common dialog infrastructure and utilities
 * 
 * Provides reusable components for all dialog implementations:
 * - Context management (brushes, subclass procs)
 * - Color message handling (WM_CTLCOLOR*)
 * - Edit control subclassing (Ctrl+A, Enter support)
 * - Dialog positioning and layout
 * - Common validation and formatting utilities
 */

#ifndef DIALOG_COMMON_H
#define DIALOG_COMMON_H

#include <windows.h>
#include <stdbool.h>

/* ============================================================================
 * Dialog Context Management
 * ============================================================================ */

/**
 * @brief Dialog visual context (brushes and subclass data)
 * 
 * @details
 * Manages brushes for consistent dialog styling and stores original
 * window procedures for subclassed controls.
 */
typedef struct {
    HBRUSH hBackgroundBrush;  /**< Dialog background (light gray) */
    HBRUSH hEditBrush;        /**< Edit control background (white) */
    HBRUSH hButtonBrush;      /**< Button background (near white) */
    WNDPROC wpOrigEditProc;   /**< Original edit proc for restoration */
    void* userData;           /**< Dialog-specific data */
} DialogContext;

/**
 * @brief Create dialog context with standard brushes
 * @return Allocated context or NULL on failure
 * 
 * @details Creates RGB(243,243,243) background, white edit, RGB(253,253,253) button
 */
DialogContext* Dialog_CreateContext(void);

/**
 * @brief Free dialog context and release brushes
 * @param ctx Context to free (NULL-safe)
 */
void Dialog_FreeContext(DialogContext* ctx);

/**
 * @brief Attach context to dialog via GWLP_USERDATA
 * @param hwndDlg Dialog handle
 * @param ctx Context to attach
 */
void Dialog_SetContext(HWND hwndDlg, DialogContext* ctx);

/**
 * @brief Retrieve dialog context
 * @param hwndDlg Dialog handle
 * @return Context or NULL if not set
 */
DialogContext* Dialog_GetContext(HWND hwndDlg);

/* ============================================================================
 * Edit Control Subclassing
 * ============================================================================ */

/**
 * @brief Standard edit subclass procedure
 * @return Message result
 * 
 * @details
 * Enhancements:
 * - Ctrl+A: Select all
 * - Enter: Submit parent dialog (CLOCK_IDC_BUTTON_OK)
 * - Auto-select on focus
 * 
 * @note Use Dialog_SubclassEdit() for automatic setup
 */
LRESULT APIENTRY Dialog_EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Subclass edit control with standard enhancements
 * @param hwndEdit Edit control handle
 * @param ctx Dialog context (stores original proc)
 * @return TRUE on success
 * 
 * @details Automatically stores original procedure in ctx->wpOrigEditProc
 */
BOOL Dialog_SubclassEdit(HWND hwndEdit, DialogContext* ctx);

/**
 * @brief Restore original edit control procedure
 * @param hwndEdit Edit control handle
 * @param ctx Dialog context (contains original proc)
 */
void Dialog_UnsubclassEdit(HWND hwndEdit, DialogContext* ctx);

/* ============================================================================
 * Color Message Handling
 * ============================================================================ */

/**
 * @brief Handle WM_CTLCOLOR* messages for consistent styling
 * @param msg Message ID
 * @param wParam HDC
 * @param ctx Dialog context (contains brushes)
 * @param result Output result value
 * @return TRUE if message was handled
 * 
 * @details
 * Handles: WM_CTLCOLORDLG, WM_CTLCOLORSTATIC, WM_CTLCOLOREDIT, WM_CTLCOLORBTN
 * Sets background colors and returns appropriate brush.
 * 
 * Usage:
 * @code
 * case WM_CTLCOLORDLG:
 * case WM_CTLCOLORSTATIC:
 * case WM_CTLCOLOREDIT:
 * case WM_CTLCOLORBTN: {
 *     INT_PTR result;
 *     if (Dialog_HandleColorMessages(msg, wParam, ctx, &result)) {
 *         return result;
 *     }
 *     break;
 * }
 * @endcode
 */
BOOL Dialog_HandleColorMessages(UINT msg, WPARAM wParam, DialogContext* ctx, INT_PTR* result);

/* ============================================================================
 * Dialog Positioning
 * ============================================================================ */

/**
 * @brief Center dialog on primary monitor
 * @param hwndDlg Dialog handle
 * 
 * @details
 * Uses work area (excludes taskbar), handles multi-monitor correctly.
 * Call in WM_INITDIALOG after setting dialog size.
 */
void Dialog_CenterOnPrimaryScreen(HWND hwndDlg);

/**
 * @brief Make dialog topmost (for settings dialogs)
 * @param hwndDlg Dialog handle
 * 
 * @details Call in WM_INITDIALOG to ensure visibility
 */
void Dialog_ApplyTopmost(HWND hwndDlg);

/* ============================================================================
 * Validation and Utilities
 * ============================================================================ */

/**
 * @brief Check if string is empty or only whitespace
 * @param str Wide string to check
 * @return TRUE if empty/whitespace
 */
BOOL Dialog_IsEmptyOrWhitespace(const wchar_t* str);

/**
 * @brief Check if string is empty or only whitespace (ANSI)
 * @param str ANSI string to check
 * @return TRUE if empty/whitespace
 */
BOOL Dialog_IsEmptyOrWhitespaceA(const char* str);

/**
 * @brief Show error dialog and refocus edit control
 * @param hwndDlg Parent dialog
 * @param editControlId Edit control to refocus
 * 
 * @details Shows localized error, then selects text in edit
 */
void Dialog_ShowErrorAndRefocus(HWND hwndDlg, int editControlId);

/**
 * @brief Format seconds to human-readable string
 * @param totalSeconds Time in seconds
 * @param buffer Output buffer
 * @param bufferSize Buffer size in bytes
 * 
 * @details
 * Formats: "1h30m15s", "25m", "90s" (omits zero components)
 */
void Dialog_FormatSecondsToString(int totalSeconds, char* buffer, size_t bufferSize);

/**
 * @brief Validate number-only input
 * @param str Wide string to validate
 * @return TRUE if contains at least one digit and only digits/whitespace
 * 
 * @details Allows whitespace, rejects empty or non-numeric
 */
BOOL Dialog_IsValidNumberInput(const wchar_t* str);

/* ============================================================================
 * Global Dialog Instance Management
 * ============================================================================ */

/**
 * @brief Dialog types for instance tracking
 */
typedef enum {
    DIALOG_INSTANCE_ERROR,
    DIALOG_INSTANCE_INPUT,
    DIALOG_INSTANCE_ABOUT,
    DIALOG_INSTANCE_POMODORO_LOOP,
    DIALOG_INSTANCE_POMODORO_COMBO,
    DIALOG_INSTANCE_WEBSITE,
    DIALOG_INSTANCE_NOTIFICATION_MSG,
    DIALOG_INSTANCE_NOTIFICATION_DISP,
    DIALOG_INSTANCE_NOTIFICATION_FULL,
    DIALOG_INSTANCE_COUNT
} DialogInstanceType;

/**
 * @brief Register dialog instance (prevents duplicates)
 * @param type Dialog type
 * @param hwnd Dialog handle
 * 
 * @details Call in WM_INITDIALOG
 */
void Dialog_RegisterInstance(DialogInstanceType type, HWND hwnd);

/**
 * @brief Unregister dialog instance
 * @param type Dialog type
 * 
 * @details Call in WM_DESTROY
 */
void Dialog_UnregisterInstance(DialogInstanceType type);

/**
 * @brief Get active dialog instance
 * @param type Dialog type
 * @return Dialog handle or NULL if not open
 */
HWND Dialog_GetInstance(DialogInstanceType type);

/**
 * @brief Check if dialog is already open
 * @param type Dialog type
 * @return TRUE if open
 */
BOOL Dialog_IsOpen(DialogInstanceType type);

#endif /* DIALOG_COMMON_H */

