/**
 * @file window_procedure.c
 * @brief Main window procedure handling all messages and user interactions
 * Implements comprehensive message processing for timer, hotkeys, dialogs, and menu commands
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include "../resource/resource.h"
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/tray.h"
#include "../include/tray_menu.h"
#include "../include/timer.h"
#include "../include/window.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/window_events.h"
#include "../include/drag_scale.h"
#include "../include/drawing.h"
#include "../include/timer_events.h"
#include "../include/tray_events.h"
#include "../include/dialog_procedure.h"
#include "../include/pomodoro.h"
#include "../include/update_checker.h"
#include "../include/async_update_checker.h"
#include "../include/hotkey.h"
#include "../include/notification.h"
#include "../include/cli.h"

/** @brief Global input text buffer for dialog operations */
extern wchar_t inputText[256];
extern int elapsed_time;
extern int message_shown;
extern TimeFormatType CLOCK_TIME_FORMAT;
extern BOOL CLOCK_SHOW_MILLISECONDS;
extern BOOL IS_MILLISECONDS_PREVIEWING;
extern BOOL PREVIEW_SHOW_MILLISECONDS;

extern void ShowNotification(HWND hwnd, const wchar_t* message);
extern void PauseMediaPlayback(void);

/** @brief Pomodoro timer configuration */
extern int POMODORO_TIMES[10];
extern int POMODORO_TIMES_COUNT;
extern int current_pomodoro_time_index;
extern int complete_pomodoro_cycles;

extern BOOL ShowInputDialog(HWND hwnd, wchar_t* text);

extern void WriteConfigPomodoroTimeOptions(int* times, int count);

/**
 * @brief Input dialog parameter structure for custom dialogs
 * Encapsulates dialog state for modal input operations
 */
typedef struct {
    const wchar_t* title;          /**< Dialog window title */
    const wchar_t* prompt;         /**< Prompt text displayed to user */
    const wchar_t* defaultText;    /**< Default input text */
    wchar_t* result;               /**< Buffer to store user input */
    size_t maxLen;                 /**< Maximum length of input */
} INPUTBOX_PARAMS;

extern void ShowPomodoroLoopDialog(HWND hwndParent);

extern void OpenUserGuide(void);

extern void OpenSupportPage(void);

extern void OpenFeedbackPage(void);

/**
 * @brief Check if string contains only whitespace characters
 * @param str String to check
 * @return TRUE if string is empty or contains only spaces, FALSE otherwise
 */
static BOOL isAllSpacesOnly(const wchar_t* str) {
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Dialog procedure for custom input box dialog
 * @param hwndDlg Dialog window handle
 * @param uMsg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter containing INPUTBOX_PARAMS on init
 * @return Message processing result
 *
 * Handles initialization, input validation, and OK/Cancel responses
 */
INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            /** Extract parameters and initialize dialog controls */
            INPUTBOX_PARAMS* params = (INPUTBOX_PARAMS*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            SetWindowTextW(hwndDlg, params->title);
            
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            
            /** Select all text for easy replacement */
            SendDlgItemMessageW(hwndDlg, IDC_EDIT_INPUT, EM_SETSEL, 0, -1);
            
            SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_INPUT));
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    /** Retrieve user input and close dialog */
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                }
                
                case IDCANCEL:
                    /** Cancel operation without saving input */
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display custom input dialog box with specified parameters
 * @param hwndParent Parent window handle
 * @param title Dialog window title
 * @param prompt Text prompt for user input
 * @param defaultText Default value in input field
 * @param result Buffer to store user input
 * @param maxLen Maximum length of input buffer
 * @return TRUE if user clicked OK, FALSE if cancelled
 *
 * Creates modal dialog for user text input with customizable appearance
 */
BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    INPUTBOX_PARAMS params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}

/**
 * @brief Gracefully exit the application
 * @param hwnd Main window handle
 *
 * Removes tray icon and posts quit message to message loop
 */
void ExitProgram(HWND hwnd) {
    RemoveTrayIcon();

    PostQuitMessage(0);
}

/** @brief Global hotkey identifiers for system-wide keyboard shortcuts */
#define HOTKEY_ID_SHOW_TIME       100    /**< Toggle system time display */
#define HOTKEY_ID_COUNT_UP        101    /**< Start count-up timer */
#define HOTKEY_ID_COUNTDOWN       102    /**< Start default countdown */
#define HOTKEY_ID_QUICK_COUNTDOWN1 103   /**< Quick countdown option 1 */
#define HOTKEY_ID_QUICK_COUNTDOWN2 104   /**< Quick countdown option 2 */
#define HOTKEY_ID_QUICK_COUNTDOWN3 105   /**< Quick countdown option 3 */
#define HOTKEY_ID_POMODORO        106    /**< Start Pomodoro timer */
#define HOTKEY_ID_TOGGLE_VISIBILITY 107  /**< Toggle window visibility */
#define HOTKEY_ID_EDIT_MODE       108    /**< Toggle edit mode */
#define HOTKEY_ID_PAUSE_RESUME    109    /**< Pause/resume current timer */
#define HOTKEY_ID_RESTART_TIMER   110    /**< Restart current timer */
#define HOTKEY_ID_CUSTOM_COUNTDOWN 111   /**< Custom countdown input */

/**
 * @brief Register all global hotkeys with the system
 * @param hwnd Window handle to receive hotkey messages
 * @return TRUE if any hotkeys were successfully registered, FALSE if none
 *
 * Loads hotkey configuration and attempts registration with conflict handling
 */
BOOL RegisterGlobalHotkeys(HWND hwnd) {
    UnregisterGlobalHotkeys(hwnd);
    
    /** Hotkey configuration variables */
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    /** Load hotkey configuration from config file */
    ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                     &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                     &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                     &pauseResumeHotkey, &restartTimerHotkey);
    
    BOOL success = FALSE;
    BOOL configChanged = FALSE;
    
    /** Register show time hotkey with conflict detection */
    if (showTimeHotkey != 0) {
        BYTE vk = LOBYTE(showTimeHotkey);
        BYTE mod = HIBYTE(showTimeHotkey);
        
        /** Convert modifier flags to Windows API format */
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_SHOW_TIME, fsModifiers, vk)) {
            success = TRUE;
        } else {
            /** Clear conflicting hotkey configuration */
            showTimeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (countUpHotkey != 0) {
        BYTE vk = LOBYTE(countUpHotkey);
        BYTE mod = HIBYTE(countUpHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_COUNT_UP, fsModifiers, vk)) {
            success = TRUE;
        } else {
            countUpHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (countdownHotkey != 0) {
        BYTE vk = LOBYTE(countdownHotkey);
        BYTE mod = HIBYTE(countdownHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_COUNTDOWN, fsModifiers, vk)) {
            success = TRUE;
        } else {
            countdownHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (quickCountdown1Hotkey != 0) {
        BYTE vk = LOBYTE(quickCountdown1Hotkey);
        BYTE mod = HIBYTE(quickCountdown1Hotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN1, fsModifiers, vk)) {
            success = TRUE;
        } else {
            quickCountdown1Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (quickCountdown2Hotkey != 0) {
        BYTE vk = LOBYTE(quickCountdown2Hotkey);
        BYTE mod = HIBYTE(quickCountdown2Hotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN2, fsModifiers, vk)) {
            success = TRUE;
        } else {
            quickCountdown2Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (quickCountdown3Hotkey != 0) {
        BYTE vk = LOBYTE(quickCountdown3Hotkey);
        BYTE mod = HIBYTE(quickCountdown3Hotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN3, fsModifiers, vk)) {
            success = TRUE;
        } else {
            quickCountdown3Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (pomodoroHotkey != 0) {
        BYTE vk = LOBYTE(pomodoroHotkey);
        BYTE mod = HIBYTE(pomodoroHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_POMODORO, fsModifiers, vk)) {
            success = TRUE;
        } else {
            pomodoroHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (toggleVisibilityHotkey != 0) {
        BYTE vk = LOBYTE(toggleVisibilityHotkey);
        BYTE mod = HIBYTE(toggleVisibilityHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY, fsModifiers, vk)) {
            success = TRUE;
        } else {
            toggleVisibilityHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (editModeHotkey != 0) {
        BYTE vk = LOBYTE(editModeHotkey);
        BYTE mod = HIBYTE(editModeHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_EDIT_MODE, fsModifiers, vk)) {
            success = TRUE;
        } else {
            editModeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (pauseResumeHotkey != 0) {
        BYTE vk = LOBYTE(pauseResumeHotkey);
        BYTE mod = HIBYTE(pauseResumeHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_PAUSE_RESUME, fsModifiers, vk)) {
            success = TRUE;
        } else {
            pauseResumeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (restartTimerHotkey != 0) {
        BYTE vk = LOBYTE(restartTimerHotkey);
        BYTE mod = HIBYTE(restartTimerHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_RESTART_TIMER, fsModifiers, vk)) {
            success = TRUE;
        } else {
            restartTimerHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    if (configChanged) {
        WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                           quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                           pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                           pauseResumeHotkey, restartTimerHotkey);
        
        if (customCountdownHotkey == 0) {
            WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", "None");
        }
    }
    
    ReadCustomCountdownHotkey(&customCountdownHotkey);
    
    if (customCountdownHotkey != 0) {
        BYTE vk = LOBYTE(customCountdownHotkey);
        BYTE mod = HIBYTE(customCountdownHotkey);
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_CUSTOM_COUNTDOWN, fsModifiers, vk)) {
            success = TRUE;
        } else {
            customCountdownHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    return success;
}

/**
 * @brief Unregister all global hotkeys from the system
 * @param hwnd Window handle that owns the hotkeys
 *
 * Safely removes all registered hotkeys to prevent conflicts on exit
 */
void UnregisterGlobalHotkeys(HWND hwnd) {
    UnregisterHotKey(hwnd, HOTKEY_ID_SHOW_TIME);
    UnregisterHotKey(hwnd, HOTKEY_ID_COUNT_UP);
    UnregisterHotKey(hwnd, HOTKEY_ID_COUNTDOWN);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN1);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN2);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN3);
    UnregisterHotKey(hwnd, HOTKEY_ID_POMODORO);
    UnregisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY);
    UnregisterHotKey(hwnd, HOTKEY_ID_EDIT_MODE);
    UnregisterHotKey(hwnd, HOTKEY_ID_PAUSE_RESUME);
    UnregisterHotKey(hwnd, HOTKEY_ID_RESTART_TIMER);
    UnregisterHotKey(hwnd, HOTKEY_ID_CUSTOM_COUNTDOWN);
}

/**
 * @brief Main window procedure handling all window messages
 * @param hwnd Window handle receiving the message
 * @param msg Message identifier
 * @param wp First message parameter
 * @param lp Second message parameter
 * @return Result of message processing
 *
 * Central message dispatcher for timer, UI, hotkey, and system events
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static wchar_t time_text[50];
    UINT uID;
    UINT uMouseMsg;

    /** Handle taskbar recreation (e.g., Explorer restart) */
    if (msg == WM_TASKBARCREATED) {
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        return 0;
    }

    switch(msg)
    {
        /** Custom application messages */
        case WM_APP_SHOW_CLI_HELP: {
            ShowCliHelpDialog(hwnd);
            return 0;
        }
        
        /** Inter-process communication for CLI arguments */
        case WM_COPYDATA: {
            PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
            if (pcds && pcds->dwData == COPYDATA_ID_CLI_TEXT && pcds->lpData && pcds->cbData > 0) {
                const size_t maxLen = 255;
                char buf[256];
                size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
                memcpy(buf, pcds->lpData, n);
                buf[maxLen] = '\0';
                buf[n] = '\0';
                HandleCliArguments(hwnd, buf);
                return 0;
            }
            break;
        }
        
        /** Quick countdown selection from CLI or other sources */
        case WM_APP_QUICK_COUNTDOWN_INDEX: {
            int idx = (int)lp;
            if (idx >= 1) {
                StartQuickCountdownByIndex(hwnd, idx);
            } else {
                StartDefaultCountDown(hwnd);
            }
            return 0;
        }
        
        /** Window lifecycle events */
        case WM_CREATE: {
            RegisterGlobalHotkeys(hwnd);
            HandleWindowCreate(hwnd);
            break;
        }

        /** Cursor management for edit mode */
        case WM_SETCURSOR: {
            if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            
            if (LOWORD(lp) == HTCLIENT || msg == CLOCK_WM_TRAYICON) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        }

        /** Mouse interaction for window dragging */
        case WM_LBUTTONDOWN: {
            StartDragWindow(hwnd);
            break;
        }

        case WM_LBUTTONUP: {
            EndDragWindow(hwnd);
            break;
        }

        /** Mouse wheel scaling */
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            HandleScaleWindow(hwnd, delta);
            break;
        }

        /** Window dragging during mouse movement */
        case WM_MOUSEMOVE: {
            if (HandleDragWindow(hwnd)) {
                return 0;
            }
            break;
        }

        /** Window painting and rendering */
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            HandleWindowPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
        
        /** Timer events for countdown/countup functionality */
        case WM_TIMER: {
            if (HandleTimerEvent(hwnd, wp)) {
                break;
            }
            break;
        }
        
        /** Window destruction cleanup */
        case WM_DESTROY: {
            UnregisterGlobalHotkeys(hwnd);
            HandleWindowDestroy(hwnd);
            return 0;
        }
        
        /** System tray icon messages */
        case CLOCK_WM_TRAYICON: {
            HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
            break;
        }
        
        /** Menu and command message processing */
        case WM_COMMAND: {
            /** Handle color selection from menu (IDs 201+) */
            if (LOWORD(wp) >= 201 && LOWORD(wp) < 201 + COLOR_OPTIONS_COUNT) {
                int colorIndex = LOWORD(wp) - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(CLOCK_TEXT_COLOR, COLOR_OPTIONS[colorIndex].hexColor, 
                            sizeof(CLOCK_TEXT_COLOR) - 1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
                    
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    WriteConfig(config_path);
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }
            WORD cmd = LOWORD(wp);
            switch (cmd) {
                /** Custom countdown timer setup with user input */
                case 101: {
                    /** Stop current time display if active */
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        KillTimer(hwnd, 1);
                    }
                    
                    /** Input validation loop until valid time or cancellation */
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);

                        /** Exit if user cancelled or provided empty input */
                        if (inputText[0] == L'\0') {
                            break;
                        }

                        /** Check for whitespace-only input */
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!iswspace(inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
                            break;
                        }

                        /** Parse and validate time input */
                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            /** Valid input: setup countdown timer */
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            CloseAllNotifications();
                            
                            /** Initialize countdown state */
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = total_seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            /** Reset timer flags and counters */
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            /** Reset Pomodoro state if active */
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            /** Start countdown display and timer */
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            ResetTimerWithInterval(hwnd);
                            break;
                        } else {
                            /** Invalid input: show error dialog */
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }

                /** Quick countdown timer options (5min, 10min, etc.) */
                case 102: case 103: case 104: case 105: case 106:
                case 107: case 108: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();
                    
                    int index = cmd - 102;
                    if (index >= 0 && index < time_options_count) {
                        int seconds = time_options[index];
                        if (seconds > 0) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            ResetTimerWithInterval(hwnd);
                        }
                    }
                    break;
                }

                /** Dynamic menu options */
                default: {
                    /** Handle dynamic quick time options */
                    if (cmd >= CLOCK_IDM_QUICK_TIME_BASE && cmd < CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS) {
                        extern void StopNotificationSound(void);
                        StopNotificationSound();

                        CloseAllNotifications();

                        int index = cmd - CLOCK_IDM_QUICK_TIME_BASE;
                        if (index >= 0 && index < time_options_count) {
                            int seconds = time_options[index];
                            if (seconds > 0) {
                                KillTimer(hwnd, 1);
                                CLOCK_TOTAL_TIME = seconds;
                                countdown_elapsed_time = 0;
                                countdown_message_shown = FALSE;
                                CLOCK_COUNT_UP = FALSE;
                                CLOCK_SHOW_CURRENT_TIME = FALSE;

                                CLOCK_IS_PAUSED = FALSE;
                                elapsed_time = 0;
                                message_shown = FALSE;
                                countup_message_shown = FALSE;

                                if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                    current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                    current_pomodoro_time_index = 0;
                                    complete_pomodoro_cycles = 0;
                                }

                                ShowWindow(hwnd, SW_SHOW);
                                InvalidateRect(hwnd, NULL, TRUE);
                                ResetTimerWithInterval(hwnd);
                            }
                        }
                        return 0;
                    }
                    
                    /** Handle dynamic advanced font selection from fonts folder */
                    else if (cmd >= 2000 && cmd < 3000) {
                        /** Helper function to recursively find font by ID */
                        BOOL FindFontByIdRecursive(const char* folderPath, int targetId, int* currentId, char* foundFontName, const char* fontsFolderPath) {
                            char searchPath[MAX_PATH];
                            snprintf(searchPath, MAX_PATH, "%s\\*", folderPath);
                            
                            WIN32_FIND_DATAA findData;
                            HANDLE hFind = FindFirstFileA(searchPath, &findData);
                            
                            if (hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    /** Skip . and .. entries */
                                    if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                                        continue;
                                    }
                                    
                                    char fullItemPath[MAX_PATH];
                                    snprintf(fullItemPath, MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
                                    
                                    /** Handle regular font files */
                                    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                        char* ext = strrchr(findData.cFileName, '.');
                                        if (ext && (stricmp(ext, ".ttf") == 0 || stricmp(ext, ".otf") == 0)) {
                                            if (*currentId == targetId) {
                                                /** Calculate relative path from fonts folder */
                                                if (_strnicmp(fullItemPath, fontsFolderPath, strlen(fontsFolderPath)) == 0) {
                                                    const char* relativePath = fullItemPath + strlen(fontsFolderPath);
                                                    if (relativePath[0] == '\\') relativePath++; // Skip leading backslash
                                                    strcpy(foundFontName, relativePath);
                                                } else {
                                                    strcpy(foundFontName, findData.cFileName);
                                                }
                                                FindClose(hFind);
                                                return TRUE;
                                            }
                                            (*currentId)++;
                                        }
                                    }
                                    /** Handle subdirectories recursively */
                                    else if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                        if (FindFontByIdRecursive(fullItemPath, targetId, currentId, foundFontName, fontsFolderPath)) {
                                            FindClose(hFind);
                                            return TRUE;
                                        }
                                    }
                                } while (FindNextFileA(hFind, &findData));
                                FindClose(hFind);
                            }
                            
                            return FALSE;
                        }
                        
                        /** Get font filename from fonts folder by ID using recursive search */
                        char fontsFolderPath[MAX_PATH];
                        char* appdata_path = getenv("LOCALAPPDATA");
                        if (appdata_path) {
                            snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
                            
                            int currentIndex = 2000;
                            char foundFontName[MAX_PATH] = {0};
                            
                            if (FindFontByIdRecursive(fontsFolderPath, cmd, &currentIndex, foundFontName, fontsFolderPath)) {
                                /** Use SwitchFont to properly load and get real font name */
                                HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                                if (SwitchFont(hInstance, foundFontName)) {
                                    /** Force complete window refresh */
                                    InvalidateRect(hwnd, NULL, TRUE);
                                    UpdateWindow(hwnd);
                                    return 0;
                                }
                            }
                        }
                        return 0;
                    }
                    break;
                }
                
                /** Application exit command */
                case 109: {
                    ExitProgram(hwnd);
                    break;
                }
                
                /** Modify quick countdown time options */
                case CLOCK_IDC_MODIFY_TIME_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);

                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!iswspace(inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        
                        if (inputText[0] == L'\0' || isAllSpaces) {
                            break;
                        }

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        
                        char* token = strtok(inputTextA, " ");
                        char options[256] = {0};
                        int valid = 1;
                        int count = 0;
                        
                        while (token && count < MAX_TIME_OPTIONS) {
                            int seconds = 0;

                            extern BOOL ParseTimeInput(const char* input, int* seconds);
                            if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                                valid = 0;
                                break;
                            }
                            
                            if (count > 0) {
                                strcat(options, ",");
                            }
                            
                            char secondsStr[32];
                            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                            strcat(options, secondsStr);
                            count++;
                            token = strtok(NULL, " ");
                        }

                        if (valid && count > 0) {
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            WriteConfigTimeOptions(options);
                            ReadConfig();
                            break;
                        } else {
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }
                
                /** Modify default startup countdown time */
                case CLOCK_IDC_MODIFY_DEFAULT_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_STARTUP_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_STARTUP_DIALOG);

                        if (inputText[0] == L'\0') {
                            break;
                        }

                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!iswspace(inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        if (isAllSpaces) {
                            break;
                        }

                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            ReadConfig();
                            break;
                        } else {
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }
                
                /** Reset application to default settings */
                case 200: {   
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    /** Stop all timers and unregister hotkeys */
                    KillTimer(hwnd, 1);
                    
                    UnregisterGlobalHotkeys(hwnd);
                    
                    extern int elapsed_time;
                    extern int countdown_elapsed_time;
                    extern int countup_elapsed_time;
                    extern BOOL message_shown;
                    extern BOOL countdown_message_shown;
                    extern BOOL countup_message_shown;
                    
                    extern BOOL InitializeHighPrecisionTimer(void);
                    extern void ResetTimer(void);
                    extern void ReadNotificationMessagesConfig(void);
                    
                    /** Reset to default 25-minute Pomodoro timer */
                    CLOCK_TOTAL_TIME = 25 * 60;
                    elapsed_time = 0;
                    countdown_elapsed_time = 0;
                    countup_elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    /** Reset all timer and display modes */
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    current_pomodoro_phase = POMODORO_PHASE_IDLE;
                    current_pomodoro_time_index = 0;
                    complete_pomodoro_cycles = 0;
                    
                    ResetTimer();
                    
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                    
                    /** Detect and set system default language */
                    AppLanguage defaultLanguage;
                    LANGID langId = GetUserDefaultUILanguage();
                    WORD primaryLangId = PRIMARYLANGID(langId);
                    WORD subLangId = SUBLANGID(langId);
                    
                    switch (primaryLangId) {
                        case LANG_CHINESE:
                            defaultLanguage = (subLangId == SUBLANG_CHINESE_SIMPLIFIED) ? 
                                             APP_LANG_CHINESE_SIMP : APP_LANG_CHINESE_TRAD;
                            break;
                        case LANG_SPANISH:
                            defaultLanguage = APP_LANG_SPANISH;
                            break;
                        case LANG_FRENCH:
                            defaultLanguage = APP_LANG_FRENCH;
                            break;
                        case LANG_GERMAN:
                            defaultLanguage = APP_LANG_GERMAN;
                            break;
                        case LANG_RUSSIAN:
                            defaultLanguage = APP_LANG_RUSSIAN;
                            break;
                        case LANG_PORTUGUESE:
                            defaultLanguage = APP_LANG_PORTUGUESE;
                            break;
                        case LANG_JAPANESE:
                            defaultLanguage = APP_LANG_JAPANESE;
                            break;
                        case LANG_KOREAN:
                            defaultLanguage = APP_LANG_KOREAN;
                            break;
                        default:
                            defaultLanguage = APP_LANG_ENGLISH;
                            break;
                    }
                    
                    if (CURRENT_LANGUAGE != defaultLanguage) {
                        CURRENT_LANGUAGE = defaultLanguage;
                    }
                    
                    /** Remove existing config and create fresh default */
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    
                    wchar_t wconfig_path[MAX_PATH];
                    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
                    
                    FILE* test = _wfopen(wconfig_path, L"r");
                    if (test) {
                        fclose(test);
                        remove(config_path);
                    }
                    
                    CreateDefaultConfig(config_path);
                    
                    ReadConfig();
                    
                    ReadNotificationMessagesConfig();
                    
                    /** Extract embedded fonts to restore missing default fonts */
                    extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);
                    ExtractEmbeddedFontsToFolder(GetModuleHandle(NULL));
                    
                    /** Reload font after config reset to ensure immediate effect */
                    extern BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, char* realFontName, size_t realFontNameSize);
                    char actualFontFileName[MAX_PATH];
                    const char* localappdata_prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
                    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
                        /** Extract just the filename for loading */
                        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
                        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
                        LoadFontByNameAndGetRealName(GetModuleHandle(NULL), actualFontFileName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
                    }
                    
                    /** Force immediate window refresh with new font */
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    /** Reset window and font scaling to defaults */
                    CLOCK_WINDOW_SCALE = 1.0f;
                    CLOCK_FONT_SCALE_FACTOR = 1.0f;
                    
                    /** Calculate optimal window size based on text dimensions */
                    HDC hdc = GetDC(hwnd);
                    
                    wchar_t fontNameW[256];
                    MultiByteToWideChar(CP_UTF8, 0, FONT_INTERNAL_NAME, -1, fontNameW, 256);
                    
                    HFONT hFont = CreateFontW(
                        -CLOCK_BASE_FONT_SIZE,   
                        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, fontNameW
                    );
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    char time_text[50];
                    FormatTime(CLOCK_TOTAL_TIME, time_text);
                    
                    wchar_t time_textW[50];
                    MultiByteToWideChar(CP_UTF8, 0, time_text, -1, time_textW, 50);
                    
                    SIZE textSize;
                    GetTextExtentPoint32(hdc, time_textW, wcslen(time_textW), &textSize);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    /** Calculate screen-proportional default scaling */
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    float defaultScale = (screenHeight * 0.03f) / 20.0f;
                    CLOCK_WINDOW_SCALE = defaultScale;
                    CLOCK_FONT_SCALE_FACTOR = defaultScale;
                    
                    SetWindowPos(hwnd, NULL, 
                        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        textSize.cx * defaultScale, textSize.cy * defaultScale,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    );
                    
                    /** Complete reset: show window, restart timer, refresh display */
                    ShowWindow(hwnd, SW_SHOW);
                    
                    ResetTimerWithInterval(hwnd);
                    
                    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
                    RedrawWindow(hwnd, NULL, NULL, 
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    
                    RegisterGlobalHotkeys(hwnd);
                    
                    break;
                }
                
                /** Timer control commands */
                case CLOCK_IDM_TIMER_PAUSE_RESUME: {
                    PauseResumeTimer(hwnd);
                    break;
                }
                case CLOCK_IDM_TIMER_RESTART: {
                    CloseAllNotifications();
                    RestartTimer(hwnd);
                    break;
                }
                
                /** Language selection menu handlers */
                case CLOCK_IDM_LANG_CHINESE: {
                    SetLanguage(APP_LANG_CHINESE_SIMP);
                    WriteConfigLanguage(APP_LANG_CHINESE_SIMP);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE_TRAD: {
                    SetLanguage(APP_LANG_CHINESE_TRAD);
                    WriteConfigLanguage(APP_LANG_CHINESE_TRAD);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_ENGLISH: {
                    SetLanguage(APP_LANG_ENGLISH);
                    WriteConfigLanguage(APP_LANG_ENGLISH);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_SPANISH: {
                    SetLanguage(APP_LANG_SPANISH);
                    WriteConfigLanguage(APP_LANG_SPANISH);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_FRENCH: {
                    SetLanguage(APP_LANG_FRENCH);
                    WriteConfigLanguage(APP_LANG_FRENCH);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_GERMAN: {
                    SetLanguage(APP_LANG_GERMAN);
                    WriteConfigLanguage(APP_LANG_GERMAN);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_RUSSIAN: {
                    SetLanguage(APP_LANG_RUSSIAN);
                    WriteConfigLanguage(APP_LANG_RUSSIAN);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_PORTUGUESE: {
                    SetLanguage(APP_LANG_PORTUGUESE);
                    WriteConfigLanguage(APP_LANG_PORTUGUESE);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_JAPANESE: {
                    SetLanguage(APP_LANG_JAPANESE);
                    WriteConfigLanguage(APP_LANG_JAPANESE);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_KOREAN: {
                    SetLanguage(APP_LANG_KOREAN);
                    WriteConfigLanguage(APP_LANG_KOREAN);

                    InvalidateRect(hwnd, NULL, TRUE);

                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                
                /** About dialog */
                case CLOCK_IDM_ABOUT:
                    ShowAboutDialog(hwnd);
                    return 0;
                
                /** Toggle window always-on-top state */
                case CLOCK_IDM_TOPMOST: {
                    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
                    
                    /** Handle topmost toggle differently in edit mode */
                    if (CLOCK_EDIT_MODE) {
                        PREVIOUS_TOPMOST_STATE = newTopmost;
                        CLOCK_WINDOW_TOPMOST = newTopmost;
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                    } else {
                        SetWindowTopmost(hwnd, newTopmost);
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");

                        InvalidateRect(hwnd, NULL, TRUE);
                        if (newTopmost) {
                            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        } else {
                            extern void ReattachToDesktop(HWND);
                            ReattachToDesktop(hwnd);
                            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

                            InvalidateRect(hwnd, NULL, TRUE);
                            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
                            KillTimer(hwnd, 1002);
                            SetTimer(hwnd, 1002, 150, NULL);
                        }
                    }
                    break;
                }
                
                /** Time format selection */
                case CLOCK_IDM_TIME_FORMAT_DEFAULT: {
                    WriteConfigTimeFormat(TIME_FORMAT_DEFAULT);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_ZERO_PADDED: {
                    WriteConfigTimeFormat(TIME_FORMAT_ZERO_PADDED);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_FULL_PADDED: {
                    WriteConfigTimeFormat(TIME_FORMAT_FULL_PADDED);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS: {
                    WriteConfigShowMilliseconds(!CLOCK_SHOW_MILLISECONDS);
                    /** Reset timer with new interval based on milliseconds display setting */
                    ResetTimerWithInterval(hwnd);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Reset countdown timer to initial state */
                case CLOCK_IDM_COUNTDOWN_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    if (CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = FALSE;
                    }
                    
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    ResetTimerWithInterval(hwnd);
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    HandleWindowReset(hwnd);
                    break;
                }
                
                /** Toggle edit mode for window positioning */
                case CLOCK_IDC_EDIT_MODE: {
                    if (CLOCK_EDIT_MODE) {
                        EndEditMode(hwnd);
                    } else {
                        StartEditMode(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
                
                /** Toggle window visibility */
                case CLOCK_IDC_TOGGLE_VISIBILITY: {
                    PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0);
                    return 0;
                }
                
                /** Custom color picker dialog */
                case CLOCK_IDC_CUSTOMIZE_LEFT: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        char hex_color[10];
                        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                                GetRValue(color), GetGValue(color), GetBValue(color));
                        WriteConfigColor(hex_color);
                        ReadConfig();
                    }
                    break;
                }
                

                
                /** Font license agreement dialog */
                case CLOCK_IDC_FONT_LICENSE_AGREE: {
                    extern INT_PTR ShowFontLicenseDialog(HWND hwndParent);
                    extern void SetFontLicenseAccepted(BOOL accepted);
                    extern void SetFontLicenseVersionAccepted(const char* version);
                    extern const char* GetCurrentFontLicenseVersion(void);
                    
                    INT_PTR result = ShowFontLicenseDialog(hwnd);
                    if (result == IDOK) {
                        /** User agreed to license terms, save to config with version and refresh menu */
                        SetFontLicenseAccepted(TRUE);
                        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                /** Advanced font selection - open fonts folder in explorer */
                case CLOCK_IDC_FONT_ADVANCED: {
                    char fontsFolderPath[MAX_PATH];
                    char* appdata_path = getenv("LOCALAPPDATA");
                    if (appdata_path) {
                        snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
                        
                        /** Check if fonts folder exists, create if not */
                        wchar_t wFontsFolderPath[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, fontsFolderPath, -1, wFontsFolderPath, MAX_PATH);
                        
                        DWORD attrs = GetFileAttributesW(wFontsFolderPath);
                        if (attrs == INVALID_FILE_ATTRIBUTES) {
                            /** Create fonts folder if it doesn't exist */
                            CreateDirectoryW(wFontsFolderPath, NULL);
                        }
                        
                        /** Open fonts folder in Windows Explorer */
                        ShellExecuteW(hwnd, L"open", wFontsFolderPath, NULL, NULL, SW_SHOWNORMAL);
                    }
                    break;
                }
                
                /** Toggle between timer and current time display */
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        /** Switch to current time mode with faster refresh */
                        ShowWindow(hwnd, SW_SHOW);  
                        
                        CLOCK_COUNT_UP = FALSE;
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0;
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, GetTimerInterval(), NULL);
                    } else {
                        /** Switch back to timer mode */
                        KillTimer(hwnd, 1);   

                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0;
                        message_shown = 0;

                        ResetTimerWithInterval(hwnd); 
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Toggle 12/24 hour time format */
                case CLOCK_IDM_24HOUR_FORMAT: {  
                    CLOCK_USE_24HOUR = !CLOCK_USE_24HOUR;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        wchar_t wconfig_path[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
                        
                        FILE *fp = _wfopen(wconfig_path, L"r");
                        if (fp) {
                            char line[256];
                            while (fgets(line, sizeof(line), fp)) {
                                if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                                    sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                                    break;
                                }
                            }
                            fclose(fp);
                            
                            WriteConfig(config_path);
                            
                            WriteConfigStartupMode(currentStartupMode);
                        } else {
                            WriteConfig(config_path);
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Toggle seconds display in time format */
                case CLOCK_IDM_SHOW_SECONDS: {  
                    CLOCK_SHOW_SECONDS = !CLOCK_SHOW_SECONDS;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        wchar_t wconfig_path[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
                        
                        FILE *fp = _wfopen(wconfig_path, L"r");
                        if (fp) {
                            char line[256];
                            while (fgets(line, sizeof(line), fp)) {
                                if (strncmp(line, "STARTUP_MODE=", 13) == 0) {
                                    sscanf(line, "STARTUP_MODE=%19s", currentStartupMode);
                                    break;
                                }
                            }
                            fclose(fp);
                            
                            WriteConfig(config_path);
                            
                            WriteConfigStartupMode(currentStartupMode);
                        } else {
                            WriteConfig(config_path);
                        }
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Recent file selection for timeout actions */
                case CLOCK_IDM_RECENT_FILE_1:
                case CLOCK_IDM_RECENT_FILE_2:
                case CLOCK_IDM_RECENT_FILE_3:
                case CLOCK_IDM_RECENT_FILE_4:
                case CLOCK_IDM_RECENT_FILE_5: {
                    int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                    if (index < CLOCK_RECENT_FILES_COUNT) {
                        /** Validate selected recent file exists */
                        wchar_t wPath[MAX_PATH] = {0};
                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                        
                        if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                            WriteConfigTimeoutFile(CLOCK_RECENT_FILES[index].path);
                            
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                        } else {
                            /** File no longer exists: show error and cleanup */
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                            
                            /** Reset timeout action to default */
                            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                            WriteConfigTimeoutAction("MESSAGE");
                            
                            /** Remove invalid file from recent list */
                            for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                            }
                            CLOCK_RECENT_FILES_COUNT--;
                            
                            char config_path[MAX_PATH];
                            GetConfigPath(config_path, MAX_PATH);
                            WriteConfig(config_path);
                        }
                    }
                    break;
                }
                
                /** File browser dialog for timeout actions */
                case CLOCK_IDM_BROWSE_FILE: {
                    wchar_t szFile[MAX_PATH] = {0};
                    
                    OPENFILENAMEW ofn = {0};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
                    ofn.lpstrFilter = L"所有文件\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn)) {
                        char utf8Path[MAX_PATH * 3] = {0};
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
                        
                        if (GetFileAttributesW(szFile) != INVALID_FILE_ATTRIBUTES) {
                            WriteConfigTimeoutFile(utf8Path);
                            
                            SaveRecentFile(utf8Path);
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                        }
                    }
                    break;
                }
                
                /** Another file browser variant for timeout actions */
                case CLOCK_IDC_TIMEOUT_BROWSE: {
                    OPENFILENAMEW ofn;
                    wchar_t szFile[MAX_PATH] = L"";
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        char utf8Path[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, 
                                           utf8Path, 
                                           sizeof(utf8Path), 
                                           NULL, NULL);
                        
                        WriteConfigTimeoutFile(utf8Path);
                        
                        SaveRecentFile(utf8Path);
                    }
                    break;
                }
                
                /** Count-up timer mode controls */
                case CLOCK_IDM_COUNT_UP: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    CLOCK_COUNT_UP = !CLOCK_COUNT_UP;
                    if (CLOCK_COUNT_UP) {
                        /** Switch to count-up mode */
                        ShowWindow(hwnd, SW_SHOW);
                        
                        elapsed_time = 0;
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_START: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    if (!CLOCK_COUNT_UP) {
                        /** Start count-up timer from zero */
                        CLOCK_COUNT_UP = TRUE;
                        
                        countup_elapsed_time = 0;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    } else {
                        /** Toggle pause state if already counting up */
                        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    extern void ResetTimer(void);
                    ResetTimer();
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Configure startup countdown time */
                case CLOCK_IDC_SET_COUNTDOWN_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));

                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), hwnd, DlgProc, (LPARAM)CLOCK_IDD_STARTUP_DIALOG);

                        if (inputText[0] == L'\0') {
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        }

                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            CLOCK_DEFAULT_START_TIME = total_seconds;
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                
                /** Startup mode configuration options */
                case CLOCK_IDC_START_SHOW_TIME: {
                    WriteConfigStartupMode("SHOW_TIME");
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_CHECKED);
                    break;
                }
                case CLOCK_IDC_START_COUNT_UP: {
                    WriteConfigStartupMode("COUNT_UP");
                    break;
                }
                case CLOCK_IDC_START_NO_DISPLAY: {
                    WriteConfigStartupMode("NO_DISPLAY");
                    
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_CHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                    break;
                }
                
                /** Toggle Windows startup shortcut */
                case CLOCK_IDC_AUTO_START: {
                    BOOL isEnabled = IsAutoStartEnabled();
                    if (isEnabled) {
                        if (RemoveShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
                        }
                    } else {
                        if (CreateShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
                        }
                    }
                    break;
                }
                
                /** Color configuration dialogs */
                case CLOCK_IDC_COLOR_VALUE: {
                    DialogBoxW(GetModuleHandle(NULL), 
                             MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG), 
                             hwnd, 
                             (DLGPROC)ColorDlgProc);
                    break;
                }
                case CLOCK_IDC_COLOR_PANEL: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                /** Pomodoro timer functionality */
                case CLOCK_IDM_POMODORO_START: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();
                    
                    /** Ensure window is visible for Pomodoro session */
                    if (!IsWindowVisible(hwnd)) {
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    
                    /** Initialize Pomodoro work session */
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    countdown_elapsed_time = 0;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                    
                    extern void InitializePomodoro(void);
                    InitializePomodoro();
                    
                    /** Force message notification for Pomodoro transitions */
                    TimeoutActionType originalAction = CLOCK_TIMEOUT_ACTION;
                    
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                    
                    KillTimer(hwnd, 1);
                    ResetTimerWithInterval(hwnd);
                    
                    elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Configure Pomodoro session durations */
                case CLOCK_IDM_POMODORO_WORK:
                case CLOCK_IDM_POMODORO_BREAK:
                case CLOCK_IDM_POMODORO_LBREAK:

                    {
                        /** Map menu selection to Pomodoro phase index */
                        int selectedIndex = 0;
                        if (LOWORD(wp) == CLOCK_IDM_POMODORO_WORK) {
                            selectedIndex = 0;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_BREAK) {
                            selectedIndex = 1;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_LBREAK) {
                            selectedIndex = 2;
                        }
                        

                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), 
                                 MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
                                 hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
                        
                        if (inputText[0] && !isAllSpacesOnly(inputText)) {
                            int total_seconds = 0;
    
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                                POMODORO_TIMES[selectedIndex] = total_seconds;
                                
                                WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
                                
                                if (selectedIndex == 0) POMODORO_WORK_TIME = total_seconds;
                                else if (selectedIndex == 1) POMODORO_SHORT_BREAK = total_seconds;
                                else if (selectedIndex == 2) POMODORO_LONG_BREAK = total_seconds;
                            }
                        }
                    }
                    break;


                /** Dynamic Pomodoro time configuration by index */
                case 600: case 601: case 602: case 603: case 604:
                case 605: case 606: case 607: case 608: case 609:
                    {
                        int selectedIndex = LOWORD(wp) - CLOCK_IDM_POMODORO_TIME_BASE;
                        
                        if (selectedIndex >= 0 && selectedIndex < POMODORO_TIMES_COUNT) {
                            memset(inputText, 0, sizeof(inputText));
                            DialogBoxParamW(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
                                     hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
                            
                            if (inputText[0] && !isAllSpacesOnly(inputText)) {
                                int total_seconds = 0;
        
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                                    POMODORO_TIMES[selectedIndex] = total_seconds;
                                    
                                    WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
                                    
                                    if (selectedIndex == 0) POMODORO_WORK_TIME = total_seconds;
                                    else if (selectedIndex == 1) POMODORO_SHORT_BREAK = total_seconds;
                                    else if (selectedIndex == 2) POMODORO_LONG_BREAK = total_seconds;
                                }
                            }
                        }
                    }
                    break;
                case CLOCK_IDM_POMODORO_LOOP_COUNT:
                    ShowPomodoroLoopDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME || 
                        CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK || 
                        CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK) {
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    HandleWindowReset(hwnd);
                    break;
                }
                
                /** Timer timeout action configurations */
                case CLOCK_IDM_TIMEOUT_SHOW_TIME: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
                    WriteConfigTimeoutAction("SHOW_TIME");
                    break;
                }
                case CLOCK_IDM_TIMEOUT_COUNT_UP: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
                    WriteConfigTimeoutAction("COUNT_UP");
                    break;
                }
                case CLOCK_IDM_SHOW_MESSAGE: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                    WriteConfigTimeoutAction("MESSAGE");
                    break;
                }
                case CLOCK_IDM_LOCK_SCREEN: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
                    WriteConfigTimeoutAction("LOCK");
                    break;
                }
                case CLOCK_IDM_SHUTDOWN: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHUTDOWN;
                    WriteConfigTimeoutAction("SHUTDOWN");
                    break;
                }
                case CLOCK_IDM_RESTART: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RESTART;
                    WriteConfigTimeoutAction("RESTART");
                    break;
                }
                case CLOCK_IDM_SLEEP: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SLEEP;
                    WriteConfigTimeoutAction("SLEEP");
                    break;
                }
                
                /** Application updates and external links */
                case CLOCK_IDM_CHECK_UPDATE: {
                    CheckForUpdateAsync(hwnd, FALSE);
                    break;
                }
                case CLOCK_IDM_OPEN_WEBSITE:
                    ShowWebsiteDialog(hwnd);
                    break;
                
                case CLOCK_IDM_CURRENT_WEBSITE:
                    ShowWebsiteDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_COMBINATION:
                    ShowPomodoroComboDialog(hwnd);
                    break;
                
                /** Notification configuration dialogs */
                case CLOCK_IDM_NOTIFICATION_CONTENT: {
                    ShowNotificationMessagesDialog(hwnd);
                    break;
                }
                case CLOCK_IDM_NOTIFICATION_DISPLAY: {
                    ShowNotificationDisplayDialog(hwnd);
                    break;
                }
                case CLOCK_IDM_NOTIFICATION_SETTINGS: {
                    ShowNotificationSettingsDialog(hwnd);
                    break;
                }
                
                /** Configuration dialogs and help functions */
                case CLOCK_IDM_HOTKEY_SETTINGS: {
                    ShowHotkeySettingsDialog(hwnd);

                    /** Re-register hotkeys after configuration changes */
                    RegisterGlobalHotkeys(hwnd);
                    break;
                }
                case CLOCK_IDM_HELP: {
                    OpenUserGuide();
                    break;
                }
                case CLOCK_IDM_SUPPORT: {
                    OpenSupportPage();
                    break;
                }
                case CLOCK_IDM_FEEDBACK: {
                    OpenFeedbackPage();
                    break;
                }
            }
            break;

        /** Common window refresh point for font changes */
refresh_window:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        
        /** Window position and state change events */
        case WM_WINDOWPOSCHANGED: {
            if (CLOCK_EDIT_MODE) {
                SaveWindowSettings(hwnd);
            }
            break;
        }
        
        /** Right-click menu and edit mode handling */
        case WM_RBUTTONUP: {
            if (CLOCK_EDIT_MODE) {
                EndEditMode(hwnd);
                return 0;
            }
            break;
        }
        
        /** Owner-drawn menu item measurement */
        case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
            if (lpmis->CtlType == ODT_MENU) {
                lpmis->itemHeight = 25;
                lpmis->itemWidth = 100;
                return TRUE;
            }
            return FALSE;
        }
        
        /** Owner-drawn menu item rendering */
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
            if (lpdis->CtlType == ODT_MENU) {
                int colorIndex = lpdis->itemID - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    /** Draw color swatch for menu item */
                    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                    int r, g, b;
                    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
                    
                    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                    
                    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
                    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
                    
                    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                             lpdis->rcItem.right, lpdis->rcItem.bottom);
                    
                    SelectObject(lpdis->hDC, oldPen);
                    SelectObject(lpdis->hDC, oldBrush);
                    DeleteObject(hPen);
                    DeleteObject(hBrush);
                    
                    if (lpdis->itemState & ODS_SELECTED) {
                        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
                    }
                    
                    return TRUE;
                }
            }
            return FALSE;
        }
        
        /** Menu item selection and preview handling */
        case WM_MENUSELECT: {
            UINT menuItem = LOWORD(wp);
            UINT flags = HIWORD(wp);
            HMENU hMenu = (HMENU)lp;

            if (!(flags & MF_POPUP) && hMenu != NULL) {
                /** Handle color preview on hover */
                int colorIndex = menuItem - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(PREVIEW_COLOR, COLOR_OPTIONS[colorIndex].hexColor, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }


                
                /** Handle fonts folder font preview on hover (IDs 2000+) */
                if (menuItem >= 2000 && menuItem < 3000) {
                    /** Helper function to find font by ID recursively */
                    BOOL FindFontNameByIdRecursive(const char* folderPath, int targetId, int* currentId, char* foundFontName, const char* fontsFolderPath) {
                        char searchPath[MAX_PATH];
                        snprintf(searchPath, MAX_PATH, "%s\\*", folderPath);
                        
                        WIN32_FIND_DATAA findData;
                        HANDLE hFind = FindFirstFileA(searchPath, &findData);
                        
                        if (hFind != INVALID_HANDLE_VALUE) {
                            do {
                                /** Skip . and .. entries */
                                if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                                    continue;
                                }
                                
                                char fullItemPath[MAX_PATH];
                                snprintf(fullItemPath, MAX_PATH, "%s\\%s", folderPath, findData.cFileName);
                                
                                /** Handle regular font files */
                                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                    char* ext = strrchr(findData.cFileName, '.');
                                    if (ext && (stricmp(ext, ".ttf") == 0 || stricmp(ext, ".otf") == 0)) {
                                        if (*currentId == targetId) {
                                            /** Calculate relative path from fonts folder */
                                            if (_strnicmp(fullItemPath, fontsFolderPath, strlen(fontsFolderPath)) == 0) {
                                                const char* relativePath = fullItemPath + strlen(fontsFolderPath);
                                                if (relativePath[0] == '\\') relativePath++; // Skip leading backslash
                                                strcpy(foundFontName, relativePath);
                                            } else {
                                                strcpy(foundFontName, findData.cFileName);
                                            }
                                            FindClose(hFind);
                                            return TRUE;
                                        }
                                        (*currentId)++;
                                    }
                                }
                                /** Handle subdirectories recursively */
                                else if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                                    if (FindFontNameByIdRecursive(fullItemPath, targetId, currentId, foundFontName, fontsFolderPath)) {
                                        FindClose(hFind);
                                        return TRUE;
                                    }
                                }
                            } while (FindNextFileA(hFind, &findData));
                            FindClose(hFind);
                        }
                        
                        return FALSE;
                    }
                    
                    /** Find font name for preview */
                    char fontsFolderPath[MAX_PATH];
                    char* appdata_path = getenv("LOCALAPPDATA");
                    if (appdata_path) {
                        snprintf(fontsFolderPath, MAX_PATH, "%s\\Catime\\resources\\fonts", appdata_path);
                        
                        int currentIndex = 2000;
                        char foundFontName[MAX_PATH] = {0};
                        
                        if (FindFontNameByIdRecursive(fontsFolderPath, menuItem, &currentIndex, foundFontName, fontsFolderPath)) {
                            /** Set up preview */
                            strncpy(PREVIEW_FONT_NAME, foundFontName, 99);
                            PREVIEW_FONT_NAME[99] = '\0';
                            
                            /** Extract filename for internal name */
                            char* lastSlash = strrchr(foundFontName, '\\');
                            const char* filenameOnly = lastSlash ? (lastSlash + 1) : foundFontName;
                            strncpy(PREVIEW_INTERNAL_NAME, filenameOnly, 99);
                            PREVIEW_INTERNAL_NAME[99] = '\0';
                            char* dot = strrchr(PREVIEW_INTERNAL_NAME, '.');
                            if (dot) *dot = '\0';
                            
                            /** Load font for preview and get real font name */
                            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                            LoadFontByNameAndGetRealName(hInstance, foundFontName, PREVIEW_INTERNAL_NAME, sizeof(PREVIEW_INTERNAL_NAME));
                            
                            IS_PREVIEWING = TRUE;
                            InvalidateRect(hwnd, NULL, TRUE);
                            return 0;
                        }
                    }
                }

                /** Handle time format preview on hover */
                if (menuItem == CLOCK_IDM_TIME_FORMAT_DEFAULT ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_FULL_PADDED ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
                    
                    if (menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
                        /** Handle milliseconds preview */
                        PREVIEW_SHOW_MILLISECONDS = !CLOCK_SHOW_MILLISECONDS;
                        IS_MILLISECONDS_PREVIEWING = TRUE;
                        
                        /** Adjust timer frequency for smooth preview */
                        ResetTimerWithInterval(hwnd);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    } else {
                        /** Handle format preview */
                        TimeFormatType previewFormat = TIME_FORMAT_DEFAULT;
                        switch (menuItem) {
                            case CLOCK_IDM_TIME_FORMAT_DEFAULT:
                                previewFormat = TIME_FORMAT_DEFAULT;
                                break;
                            case CLOCK_IDM_TIME_FORMAT_ZERO_PADDED:
                                previewFormat = TIME_FORMAT_ZERO_PADDED;
                                break;
                            case CLOCK_IDM_TIME_FORMAT_FULL_PADDED:
                                previewFormat = TIME_FORMAT_FULL_PADDED;
                                break;
                        }
                        
                        PREVIEW_TIME_FORMAT = previewFormat;
                        IS_TIME_FORMAT_PREVIEWING = TRUE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
                
                /** Clear preview if no matching item found */
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                    if (IS_PREVIEWING) {
                        CancelFontPreview();
                    }
                    IS_COLOR_PREVIEWING = FALSE;
                    IS_TIME_FORMAT_PREVIEWING = FALSE;
                    
                    /** Reset timer frequency when exiting milliseconds preview */
                    if (IS_MILLISECONDS_PREVIEWING) {
                        IS_MILLISECONDS_PREVIEWING = FALSE;
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (flags & MF_POPUP) {
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                    if (IS_PREVIEWING) {
                        CancelFontPreview();
                    }
                    IS_COLOR_PREVIEWING = FALSE;
                    IS_TIME_FORMAT_PREVIEWING = FALSE;
                    
                    /** Reset timer frequency when exiting milliseconds preview */
                    if (IS_MILLISECONDS_PREVIEWING) {
                        IS_MILLISECONDS_PREVIEWING = FALSE;
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        
        /** Menu loop exit cleanup */
        case WM_EXITMENULOOP: {
            if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                if (IS_PREVIEWING) {
                    CancelFontPreview();
                }
                IS_COLOR_PREVIEWING = FALSE;
                IS_TIME_FORMAT_PREVIEWING = FALSE;
                
                /** Reset timer frequency when exiting milliseconds preview */
                if (IS_MILLISECONDS_PREVIEWING) {
                    IS_MILLISECONDS_PREVIEWING = FALSE;
                    ResetTimerWithInterval(hwnd);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        
        /** Ctrl+Right-click edit mode toggle */
        case WM_RBUTTONDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                
                if (CLOCK_EDIT_MODE) {
                    SetClickThrough(hwnd, FALSE);
                } else {
                    SetClickThrough(hwnd, TRUE);
                    SaveWindowSettings(hwnd);
                    WriteConfigColor(CLOCK_TEXT_COLOR);
                }
                
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            break;
        }
        
        /** Window lifecycle events */
        case WM_CLOSE: {
            SaveWindowSettings(hwnd);
            DestroyWindow(hwnd);
            break;
        }
        
        /** Double-click to enter edit mode */
        case WM_LBUTTONDBLCLK: {
            if (!CLOCK_EDIT_MODE) {
                StartEditMode(hwnd);
                return 0;
            }
            break;
        }
        
        /** Global hotkey message processing */
        case WM_HOTKEY: {
            if (wp == HOTKEY_ID_SHOW_TIME) {
                ToggleShowTimeMode(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_COUNT_UP) {
                StartCountUp(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_COUNTDOWN) {
                StartDefaultCountDown(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_CUSTOM_COUNTDOWN) {
                /** Close existing input dialog if open */
                if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
                    SendMessage(g_hwndInputDialog, WM_CLOSE, 0, 0);
                    return 0;
                }
                
                /** Reset notification state for new countdown */
                extern BOOL countdown_message_shown;
                countdown_message_shown = FALSE;
                
                extern void ReadNotificationTypeConfig(void);
                ReadNotificationTypeConfig();
                
                extern int elapsed_time;
                extern BOOL message_shown;
                
                memset(inputText, 0, sizeof(inputText));
                
                INT_PTR result = DialogBoxParamW(GetModuleHandle(NULL), 
                                         MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), 
                                         hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);
                
                if (inputText[0] != L'\0') {
                    int total_seconds = 0;

                    char inputTextA[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                    if (ParseInput(inputTextA, &total_seconds)) {
                        extern void StopNotificationSound(void);
                        StopNotificationSound();
                        
                        CloseAllNotifications();
                        
                        CLOCK_TOTAL_TIME = total_seconds;
                        countdown_elapsed_time = 0;
                        elapsed_time = 0;
                        message_shown = FALSE;
                        countdown_message_shown = FALSE;
                        
                        CLOCK_COUNT_UP = FALSE;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_IS_PAUSED = FALSE;
                        
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN1) {
                StartQuickCountdown1(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN2) {
                StartQuickCountdown2(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN3) {
                StartQuickCountdown3(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_POMODORO) {

                StartPomodoroTimer(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_TOGGLE_VISIBILITY) {
                /** Toggle window visibility */
                if (IsWindowVisible(hwnd)) {
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                }
                return 0;
            } else if (wp == HOTKEY_ID_EDIT_MODE) {

                ToggleEditMode(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_PAUSE_RESUME) {

                TogglePauseResume(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_RESTART_TIMER) {
                /** Restart current timer from beginning */
                CloseAllNotifications();

                RestartCurrentTimer(hwnd);
                return 0;
            }
            break;
        }

        /** Custom application message for hotkey re-registration */
        case WM_APP+1: {
            RegisterGlobalHotkeys(hwnd);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

/** @brief External timer state variables */
extern int CLOCK_DEFAULT_START_TIME;
extern int countdown_elapsed_time;
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern int CLOCK_TOTAL_TIME;

/** @brief Menu manipulation helper functions */
void RemoveMenuItems(HMENU hMenu, int count);

void AddMenuItem(HMENU hMenu, UINT id, const wchar_t* text, BOOL isEnabled);

void ModifyMenuItemText(HMENU hMenu, UINT id, const wchar_t* text);



/**
 * @brief Toggle between timer display and current time display
 * @param hwnd Main window handle
 *
 * Switches to current time mode with 100ms refresh rate
 */
void ToggleShowTimeMode(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        
        ResetTimerWithInterval(hwnd);
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Start count-up timer from zero
 * @param hwnd Main window handle
 *
 * Initializes count-up mode and adjusts timer refresh rate if needed
 */
void StartCountUp(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern int countup_elapsed_time;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    countup_elapsed_time = 0;
    
    CLOCK_COUNT_UP = TRUE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_IS_PAUSED = FALSE;
    
    if (wasShowingTime) {
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start default countdown timer
 * @param hwnd Main window handle
 *
 * Uses configured default time or prompts for input if not set
 */
void StartDefaultCountDown(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (CLOCK_DEFAULT_START_TIME > 0) {
        CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
            } else {
            /** Prompt for time input if no default set */
            PostMessage(hwnd, WM_COMMAND, 101, 0);
        }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start Pomodoro timer session
 * @param hwnd Main window handle
 *
 * Initializes Pomodoro mode with work phase timing
 */
void StartPomodoroTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    if (wasShowingTime) {
        KillTimer(hwnd, 1);
    }
    
    PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
}

/**
 * @brief Toggle window edit mode for positioning and configuration
 * @param hwnd Main window handle
 *
 * Switches between transparent click-through mode and interactive edit mode
 */
void ToggleEditMode(HWND hwnd) {
    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
    
    if (CLOCK_EDIT_MODE) {
        /** Enter edit mode: make window interactive */
        PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
        
        if (!CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
        }
        
        SetBlurBehind(hwnd, TRUE);
        
        SetClickThrough(hwnd, FALSE);
        
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else {
        /** Exit edit mode: restore transparency and click-through */
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        SetClickThrough(hwnd, TRUE);
        
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, 1002);
            SetTimer(hwnd, 1002, 150, NULL);
            return;
        }
        
        SaveWindowSettings(hwnd);
        WriteConfigColor(CLOCK_TEXT_COLOR);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Toggle pause/resume state of current timer
 * @param hwnd Main window handle
 *
 * Only affects countdown/count-up timers, not current time display
 */
void TogglePauseResume(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        if (!CLOCK_IS_PAUSED) {
            /** About to pause: save current milliseconds first */
            PauseTimerMilliseconds();
        }
        
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {
            /** Record pause timestamp and stop updates */
            CLOCK_LAST_TIME_UPDATE = time(NULL);
            KillTimer(hwnd, 1);
            
            extern BOOL PauseNotificationSound(void);
            PauseNotificationSound();
        } else {
            /** Resume timer updates and notification sounds */
            ResetMillisecondAccumulator();  /** Reset millisecond timing on resume */
            SetTimer(hwnd, 1, GetTimerInterval(), NULL);
            
            extern BOOL ResumeNotificationSound(void);
            ResumeNotificationSound();
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Restart current timer from beginning
 * @param hwnd Main window handle
 *
 * Resets elapsed time and notification state for active timer
 */
void RestartCurrentTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        extern int elapsed_time;
        extern BOOL message_shown;
        
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        if (CLOCK_COUNT_UP) {
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            countdown_elapsed_time = 0;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on restart */
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Start first configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses first time option or falls back to default countdown
 */
void StartQuickCountdown1(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (time_options_count > 0) {
        CLOCK_TOTAL_TIME = time_options[0];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start second configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses second time option or falls back to default countdown
 */
void StartQuickCountdown2(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (time_options_count > 1) {
        CLOCK_TOTAL_TIME = time_options[1];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start third configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses third time option or falls back to default countdown
 */
void StartQuickCountdown3(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (time_options_count > 2) {
        CLOCK_TOTAL_TIME = time_options[2];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start quick countdown timer by index
 * @param hwnd Main window handle
 * @param index 1-based index of time option to use
 *
 * Generic function to start any configured quick countdown option
 */
void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    extern void StopNotificationSound(void);
    StopNotificationSound();

    CloseAllNotifications();

    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;

    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();

    extern int time_options[];
    extern int time_options_count;

    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;

    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;

    /** Convert to zero-based index for array access */
    int zeroBased = index - 1;
    if (zeroBased >= 0 && zeroBased < time_options_count) {
        CLOCK_TOTAL_TIME = time_options[zeroBased];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */

        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }

        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}
