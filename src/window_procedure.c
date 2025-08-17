/**
 * @file window_procedure.c
 * @brief Window message processing implementation
 * 
 * This file implements the message processing callback function for the application's main window,
 * handling all message events for the window.
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

// Variables imported from main.c
extern wchar_t inputText[256];
extern int elapsed_time;
extern int message_shown;

// Function declarations imported from main.c
extern void ShowNotification(HWND hwnd, const char* message);
extern void PauseMediaPlayback(void);

// Add these external variable declarations at the beginning of the file
extern int POMODORO_TIMES[10]; // Pomodoro time array
extern int POMODORO_TIMES_COUNT; // Number of pomodoro time options
extern int current_pomodoro_time_index; // Current pomodoro time index
extern int complete_pomodoro_cycles; // Completed pomodoro cycles

// If ShowInputDialog function needs to be declared, add at the beginning
extern BOOL ShowInputDialog(HWND hwnd, wchar_t* text);

// Modify to match the correct function declaration in config.h
extern void WriteConfigPomodoroTimeOptions(int* times, int count);

// If the function doesn't exist, use an existing similar function
// For example, modify calls to WriteConfigPomodoroTimeOptions to WriteConfigPomodoroTimes

// Add at the beginning of the file
typedef struct {
    const wchar_t* title;
    const wchar_t* prompt;
    const wchar_t* defaultText;
    wchar_t* result;
    size_t maxLen;
} INPUTBOX_PARAMS;

// Add declaration for ShowPomodoroLoopDialog function at the beginning of the file
extern void ShowPomodoroLoopDialog(HWND hwndParent);

// Add declaration for OpenUserGuide function
extern void OpenUserGuide(void);

// Add declaration for OpenSupportPage function
extern void OpenSupportPage(void);

// Add declaration for OpenFeedbackPage function
extern void OpenFeedbackPage(void);

// Helper function: Check if a string contains only spaces
static BOOL isAllSpacesOnly(const wchar_t* str) {
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Input dialog callback function
 * @param hwndDlg Dialog handle
 * @param uMsg Message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Processing result
 */
INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            // Get passed parameters
            INPUTBOX_PARAMS* params = (INPUTBOX_PARAMS*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            // Set dialog title
            SetWindowTextW(hwndDlg, params->title);
            
            // Set prompt message
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            
            // Set default text
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            
            // Select text
            SendDlgItemMessageW(hwndDlg, IDC_EDIT_INPUT, EM_SETSEL, 0, -1);
            
            // Set focus
            SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_INPUT));
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    // Get input text
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                }
                
                case IDCANCEL:
                    // Cancel operation
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display input dialog
 * @param hwndParent Parent window handle
 * @param title Dialog title
 * @param prompt Prompt message
 * @param defaultText Default text
 * @param result Result buffer
 * @param maxLen Maximum buffer length
 * @return TRUE if successful, FALSE if canceled
 */
BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    // Prepare parameters to pass to dialog
    INPUTBOX_PARAMS params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
    // Display modal dialog
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}

void ExitProgram(HWND hwnd) {
    RemoveTrayIcon();

    PostQuitMessage(0);
}

#define HOTKEY_ID_SHOW_TIME       100  // Hotkey ID to show current time
#define HOTKEY_ID_COUNT_UP        101  // Hotkey ID for count up timer
#define HOTKEY_ID_COUNTDOWN       102  // Hotkey ID for countdown timer
#define HOTKEY_ID_QUICK_COUNTDOWN1 103 // Hotkey ID for quick countdown 1
#define HOTKEY_ID_QUICK_COUNTDOWN2 104 // Hotkey ID for quick countdown 2
#define HOTKEY_ID_QUICK_COUNTDOWN3 105 // Hotkey ID for quick countdown 3
#define HOTKEY_ID_POMODORO        106  // Hotkey ID for pomodoro timer
#define HOTKEY_ID_TOGGLE_VISIBILITY 107 // Hotkey ID for hide/show
#define HOTKEY_ID_EDIT_MODE       108  // Hotkey ID for edit mode
#define HOTKEY_ID_PAUSE_RESUME    109  // Hotkey ID for pause/resume
#define HOTKEY_ID_RESTART_TIMER   110  // Hotkey ID for restart timer
#define HOTKEY_ID_CUSTOM_COUNTDOWN 111 // Hotkey ID for custom countdown

/**
 * @brief Register global hotkeys
 * @param hwnd Window handle
 * 
 * Reads and registers global hotkey settings from the configuration file for quickly switching between
 * displaying current time, count up timer, and default countdown.
 * If a hotkey is already registered, it will be unregistered before re-registering.
 * If a hotkey cannot be registered (possibly because it's being used by another program),
 * that hotkey setting will be set to none and the configuration file will be updated.
 * 
 * @return BOOL Whether at least one hotkey was successfully registered
 */
BOOL RegisterGlobalHotkeys(HWND hwnd) {
    // First unregister all previously registered hotkeys
    UnregisterGlobalHotkeys(hwnd);
    
    // Use new function to read hotkey configuration
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
    
    ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                     &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                     &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                     &pauseResumeHotkey, &restartTimerHotkey);
    
    BOOL success = FALSE;
    BOOL configChanged = FALSE;
    
    // Register show current time hotkey
    if (showTimeHotkey != 0) {
        BYTE vk = LOBYTE(showTimeHotkey);  // Virtual key code
        BYTE mod = HIBYTE(showTimeHotkey); // Modifier keys
        
        UINT fsModifiers = 0;
        if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
        if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
        if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
        
        if (RegisterHotKey(hwnd, HOTKEY_ID_SHOW_TIME, fsModifiers, vk)) {
            success = TRUE;
        } else {
            // Hotkey registration failed, clear configuration
            showTimeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register count up hotkey
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
            // Hotkey registration failed, clear configuration
            countUpHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register countdown hotkey
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
            // Hotkey registration failed, clear configuration
            countdownHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register quick countdown 1 hotkey
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
            // Hotkey registration failed, clear configuration
            quickCountdown1Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register quick countdown 2 hotkey
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
            // Hotkey registration failed, clear configuration
            quickCountdown2Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register quick countdown 3 hotkey
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
            // Hotkey registration failed, clear configuration
            quickCountdown3Hotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register pomodoro hotkey
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
            // Hotkey registration failed, clear configuration
            pomodoroHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register hide/show window hotkey
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
            // Hotkey registration failed, clear configuration
            toggleVisibilityHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register edit mode hotkey
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
            // Hotkey registration failed, clear configuration
            editModeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register pause/resume hotkey
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
            // Hotkey registration failed, clear configuration
            pauseResumeHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // Register restart timer hotkey
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
            // Hotkey registration failed, clear configuration
            restartTimerHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    // If any hotkey registration failed, update config file
    if (configChanged) {
        WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                           quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                           pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                           pauseResumeHotkey, restartTimerHotkey);
        
        // Check if custom countdown hotkey was cleared, if so, update config
        if (customCountdownHotkey == 0) {
            WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", "None");
        }
    }
    
    // Added after reading hotkey configuration
    ReadCustomCountdownHotkey(&customCountdownHotkey);
    
    // Added after registering countdown hotkey
    // Register custom countdown hotkey
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
            // Hotkey registration failed, clear configuration
            customCountdownHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    return success;
}

/**
 * @brief Unregister global hotkeys
 * @param hwnd Window handle
 * 
 * Unregister all previously registered global hotkeys.
 */
void UnregisterGlobalHotkeys(HWND hwnd) {
    // Unregister all previously registered hotkeys
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
 * @brief Main window message processing callback function
 * @param hwnd Window handle
 * @param msg Message type
 * @param wp Message parameter (specific meaning depends on message type)
 * @param lp Message parameter (specific meaning depends on message type)
 * @return LRESULT Message processing result
 * 
 * Handles all message events for the main window, including:
 * - Window creation/destruction
 * - Mouse events (dragging, wheel scaling)
 * - Timer events
 * - System tray interaction
 * - Drawing events
 * - Menu command processing
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static wchar_t time_text[50];
    UINT uID;
    UINT uMouseMsg;

    // Check if this is a TaskbarCreated message
    // TaskbarCreated is a system message broadcasted when Windows Explorer restarts
    // At this time all tray icons are destroyed and need to be recreated by applications
    // Handling this message solves the issue of the tray icon disappearing while the program is still running
    if (msg == WM_TASKBARCREATED) {
        // Explorer has restarted, need to recreate the tray icon
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        return 0;
    }

    switch(msg)
    {
        case WM_APP_SHOW_CLI_HELP: {
            ShowCliHelpDialog(hwnd);
            return 0;
        }
        case WM_COPYDATA: {
            PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
            if (pcds && pcds->dwData == COPYDATA_ID_CLI_TEXT && pcds->lpData && pcds->cbData > 0) {
                // Ensure null-terminated
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
        case WM_APP_QUICK_COUNTDOWN_INDEX: {
            // lParam carries the 1-based index
            int idx = (int)lp;
            if (idx >= 1) {
                StartQuickCountdownByIndex(hwnd, idx);
            } else {
                StartDefaultCountDown(hwnd);
            }
            return 0;
        }
        case WM_CREATE: {
            // Register global hotkeys when window is created
            RegisterGlobalHotkeys(hwnd);
            HandleWindowCreate(hwnd);
            break;
        }

        case WM_SETCURSOR: {
            // When in edit mode, always use default arrow cursor
            if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE; // Indicates we've handled this message
            }
            
            // Also use default arrow cursor when handling tray icon operations
            if (LOWORD(lp) == HTCLIENT || msg == CLOCK_WM_TRAYICON) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            StartDragWindow(hwnd);
            break;
        }

        case WM_LBUTTONUP: {
            EndDragWindow(hwnd);
            break;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            HandleScaleWindow(hwnd, delta);
            break;
        }

        case WM_MOUSEMOVE: {
            if (HandleDragWindow(hwnd)) {
                return 0;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            HandleWindowPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (HandleTimerEvent(hwnd, wp)) {
                break;
            }
            break;
        }
        case WM_DESTROY: {
            // Unregister global hotkeys when window is destroyed
            UnregisterGlobalHotkeys(hwnd);
            HandleWindowDestroy(hwnd);
            return 0;
        }
        case CLOCK_WM_TRAYICON: {
            HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
            break;
        }
        case WM_COMMAND: {
            // Handle preset color options (ID: 201 ~ 201+COLOR_OPTIONS_COUNT-1)
            if (LOWORD(wp) >= 201 && LOWORD(wp) < 201 + COLOR_OPTIONS_COUNT) {
                int colorIndex = LOWORD(wp) - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    // Update current color
                    strncpy(CLOCK_TEXT_COLOR, COLOR_OPTIONS[colorIndex].hexColor, 
                            sizeof(CLOCK_TEXT_COLOR) - 1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
                    
                    // Write to config file
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    WriteConfig(config_path);
                    
                    // Redraw window to show new color
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }
            WORD cmd = LOWORD(wp);
            switch (cmd) {
                case 101: {   
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        KillTimer(hwnd, 1);
                    }
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);

                        if (inputText[0] == L'\0') {
                            break;
                        }

                        // Check if it's only spaces
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
                        // Convert Unicode to ANSI for ParseInput
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            // Stop any notification sound that may be playing
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            // Close all notification windows
                            CloseAllNotifications();
                            
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = total_seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            // If currently in Pomodoro mode, reset the Pomodoro state when switching to normal countdown
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                            break;
                        } else {
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }
                // Handle quick time options (legacy IDs 102..)
                case 102: case 103: case 104: case 105: case 106:
                case 107: case 108: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();
                    
                    int index = cmd - 102;
                    if (index >= 0 && index < time_options_count) {
                        int seconds = time_options[index];
                        if (seconds > 0) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = seconds; // Already in seconds
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            // If currently in Pomodoro mode, reset the Pomodoro state when switching to normal countdown
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                        }
                    }
                    break;
                }
                // Handle quick time options (new ID base CLOCK_IDM_QUICK_TIME_BASE, support up to MAX_TIME_OPTIONS)
                default: if (cmd >= CLOCK_IDM_QUICK_TIME_BASE && cmd < CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS) {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();

                    // Close all notification windows
                    CloseAllNotifications();

                    int index = cmd - CLOCK_IDM_QUICK_TIME_BASE;
                    if (index >= 0 && index < time_options_count) {
                        int seconds = time_options[index];
                        if (seconds > 0) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = seconds; // Already in seconds
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;

                            CLOCK_IS_PAUSED = FALSE;
                            elapsed_time = 0;
                            message_shown = FALSE;
                            countup_message_shown = FALSE;

                            // If currently in Pomodoro mode, reset the Pomodoro state when switching to normal countdown
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }

                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            SetTimer(hwnd, 1, 1000, NULL);
                        }
                    }
                    return 0;
                } // end quick time base range
                // Handle exit option
                case 109: {
                    ExitProgram(hwnd);
                    break;
                }
                case CLOCK_IDC_MODIFY_TIME_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);

                        // Check if input is empty or contains only spaces
                        BOOL isAllSpaces = TRUE;
                        for (int i = 0; inputText[i]; i++) {
                            if (!iswspace(inputText[i])) {
                                isAllSpaces = FALSE;
                                break;
                            }
                        }
                        
                        // If input is empty or contains only spaces, exit the loop
                        if (inputText[0] == L'\0' || isAllSpaces) {
                            break;
                        }

                        // Convert inputText to char for parsing
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        
                        char* token = strtok(inputTextA, " ");
                        char options[256] = {0};
                        int valid = 1;
                        int count = 0;
                        
                        while (token && count < MAX_TIME_OPTIONS) {
                            int seconds = 0;
                            // Use ParseTimeInput to support flexible time formats like 30s, 25m, 1h30m
                            extern BOOL ParseTimeInput(const char* input, int* seconds);
                            if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                                valid = 0;
                                break;
                            }
                            
                            if (count > 0) {
                                strcat(options, ",");
                            }
                            
                            // Store seconds directly (no conversion needed)
                            char secondsStr[32];
                            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                            strcat(options, secondsStr);
                            count++;
                            token = strtok(NULL, " ");
                        }

                        if (valid && count > 0) {
                            // Stop any notification sound that may be playing
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
                case CLOCK_IDC_MODIFY_DEFAULT_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_STARTUP_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_STARTUP_DIALOG);

                        if (inputText[0] == L'\0') {
                            break;
                        }

                        // Check if it's only spaces
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
                        // Convert Unicode to ANSI for ParseInput
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            // Stop any notification sound that may be playing
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
                case 200: {   
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // First, stop all timers to ensure no timer events will be processed
                    KillTimer(hwnd, 1);
                    
                    // Unregister all global hotkeys to ensure they don't remain active after reset
                    UnregisterGlobalHotkeys(hwnd);
                    
                    // Fully reset timer state - critical part
                    // Import all timer state variables that need to be reset
                    extern int elapsed_time;            // from main.c
                    extern int countdown_elapsed_time;  // from timer.c
                    extern int countup_elapsed_time;    // from timer.c
                    extern BOOL message_shown;          // from main.c 
                    extern BOOL countdown_message_shown;// from timer.c
                    extern BOOL countup_message_shown;  // from timer.c
                    
                    // Import high-precision timer initialization function from timer.c
                    extern BOOL InitializeHighPrecisionTimer(void);
                    extern void ResetTimer(void);    // Use a dedicated reset function
                    extern void ReadNotificationMessagesConfig(void); // Read notification message configuration
                    
                    // Reset all timer state variables - order matters!
                    CLOCK_TOTAL_TIME = 25 * 60;      // 1. First set total time to 25 minutes
                    elapsed_time = 0;                // 2. Reset elapsed_time in main.c
                    countdown_elapsed_time = 0;      // 3. Reset countdown_elapsed_time in timer.c
                    countup_elapsed_time = 0;        // 4. Reset countup_elapsed_time in timer.c
                    message_shown = FALSE;           // 5. Reset message status
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    // Set timer state to countdown mode
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    // Reset Pomodoro state
                    current_pomodoro_phase = POMODORO_PHASE_IDLE;
                    current_pomodoro_time_index = 0;
                    complete_pomodoro_cycles = 0;
                    
                    // Call the dedicated timer reset function - this is crucial!
                    // This function reinitializes the high-precision timer and resets all timing states
                    ResetTimer();
                    
                    // Reset UI state
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    // Reset file path
                    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                    
                    // Default language initialization
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
                    
                    // Delete and recreate the configuration file
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    
                    // Ensure the file is closed and deleted
                    FILE* test = fopen(config_path, "r");
                    if (test) {
                        fclose(test);
                        remove(config_path);
                    }
                    
                    // Recreate default configuration
                    CreateDefaultConfig(config_path);
                    
                    // Reread the configuration
                    ReadConfig();
                    
                    // Ensure notification messages are reread
                    ReadNotificationMessagesConfig();
                    
                    // Restore default font
                    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                    for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
                        if (strcmp(fontResources[i].fontName, "Wallpoet Essence.ttf") == 0) {  
                            LoadFontFromResource(hInstance, fontResources[i].resourceId);
                            break;
                        }
                    }
                    
                    // Reset window scale
                    CLOCK_WINDOW_SCALE = 1.0f;
                    CLOCK_FONT_SCALE_FACTOR = 1.0f;
                    
                    // Recalculate window size
                    HDC hdc = GetDC(hwnd);
                    HFONT hFont = CreateFont(
                        -CLOCK_BASE_FONT_SIZE,   
                        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, FONT_INTERNAL_NAME
                    );
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    char time_text[50];
                    FormatTime(CLOCK_TOTAL_TIME, time_text);
                    SIZE textSize;
                    GetTextExtentPoint32(hdc, time_text, strlen(time_text), &textSize);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    // Set default scale based on screen height
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    float defaultScale = (screenHeight * 0.03f) / 20.0f;
                    CLOCK_WINDOW_SCALE = defaultScale;
                    CLOCK_FONT_SCALE_FACTOR = defaultScale;
                    
                    // Reset window position
                    SetWindowPos(hwnd, NULL, 
                        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        textSize.cx * defaultScale, textSize.cy * defaultScale,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    );
                    
                    // Ensure window is visible
                    ShowWindow(hwnd, SW_SHOW);
                    
                    // Restart the timer - ensure it's started after all states are reset
                    SetTimer(hwnd, 1, 1000, NULL);
                    
                    // Refresh window display
                    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
                    RedrawWindow(hwnd, NULL, NULL, 
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    
                    // Reregister default hotkeys
                    RegisterGlobalHotkeys(hwnd);
                    
                    break;
                }
                case CLOCK_IDM_TIMER_PAUSE_RESUME: {
                    PauseResumeTimer(hwnd);
                    break;
                }
                case CLOCK_IDM_TIMER_RESTART: {
                    // Close all notification windows
                    CloseAllNotifications();
                    RestartTimer(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE: {
                    SetLanguage(APP_LANG_CHINESE_SIMP);
                    WriteConfigLanguage(APP_LANG_CHINESE_SIMP);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_CHINESE_TRAD: {
                    SetLanguage(APP_LANG_CHINESE_TRAD);
                    WriteConfigLanguage(APP_LANG_CHINESE_TRAD);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_ENGLISH: {
                    SetLanguage(APP_LANG_ENGLISH);
                    WriteConfigLanguage(APP_LANG_ENGLISH);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_SPANISH: {
                    SetLanguage(APP_LANG_SPANISH);
                    WriteConfigLanguage(APP_LANG_SPANISH);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_FRENCH: {
                    SetLanguage(APP_LANG_FRENCH);
                    WriteConfigLanguage(APP_LANG_FRENCH);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_GERMAN: {
                    SetLanguage(APP_LANG_GERMAN);
                    WriteConfigLanguage(APP_LANG_GERMAN);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_RUSSIAN: {
                    SetLanguage(APP_LANG_RUSSIAN);
                    WriteConfigLanguage(APP_LANG_RUSSIAN);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_PORTUGUESE: {
                    SetLanguage(APP_LANG_PORTUGUESE);
                    WriteConfigLanguage(APP_LANG_PORTUGUESE);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_JAPANESE: {
                    SetLanguage(APP_LANG_JAPANESE);
                    WriteConfigLanguage(APP_LANG_JAPANESE);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_LANG_KOREAN: {
                    SetLanguage(APP_LANG_KOREAN);
                    WriteConfigLanguage(APP_LANG_KOREAN);
                    // Refresh window
                    InvalidateRect(hwnd, NULL, TRUE);
                    // Refresh tray menu
                    extern void UpdateTrayIcon(HWND hwnd);
                    UpdateTrayIcon(hwnd);
                    break;
                }
                case CLOCK_IDM_ABOUT:
                    ShowAboutDialog(hwnd);
                    return 0;
                case CLOCK_IDM_TOPMOST: {
                    // Toggle the topmost state in configuration
                    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
                    
                    // If in edit mode, just update the stored state but don't apply it yet
                    if (CLOCK_EDIT_MODE) {
                        // Update the configuration and saved state only
                        PREVIOUS_TOPMOST_STATE = newTopmost;
                        CLOCK_WINDOW_TOPMOST = newTopmost;
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                    } else {
                        // Not in edit mode, apply it immediately
                        SetWindowTopmost(hwnd, newTopmost);
                        WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                        // Force ensure visibility after toggling
                        InvalidateRect(hwnd, NULL, TRUE);
                        if (newTopmost) {
                            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        } else {
                            // Reattach to desktop for non-topmost mode and ensure visible
                            extern void ReattachToDesktop(HWND);
                            ReattachToDesktop(hwnd);
                            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                            // Force redraw and schedule a short delayed re-assertion for stability
                            InvalidateRect(hwnd, NULL, TRUE);
                            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
                            KillTimer(hwnd, 1002);
                            SetTimer(hwnd, 1002, 150, NULL);
                        }
                    }
                    break;
                }
                case CLOCK_IDM_COUNTDOWN_RESET: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();

                    if (CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = FALSE;  // Switch to countdown mode
                    }
                    
                    // Reset the countdown timer
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    // Restart the timer
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 1000, NULL);
                    
                    // Force redraw of the window
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    // Ensure the window is on top and visible after reset
                    HandleWindowReset(hwnd);
                    break;
                }
                case CLOCK_IDC_EDIT_MODE: {
                    if (CLOCK_EDIT_MODE) {
                        EndEditMode(hwnd);
                    } else {
                        StartEditMode(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
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
                case CLOCK_IDC_FONT_RECMONO: {
                    WriteConfigFont("RecMonoCasual Nerd Font Mono Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DEPARTURE: {
                    WriteConfigFont("DepartureMono Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_TERMINESS: {
                    WriteConfigFont("Terminess Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PINYON_SCRIPT: {
                    WriteConfigFont("Pinyon Script Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_ARBUTUS: {
                    WriteConfigFont("Arbutus Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_BERKSHIRE: {
                    WriteConfigFont("Berkshire Swash Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_CAVEAT: {
                    WriteConfigFont("Caveat Brush Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_CREEPSTER: {
                    WriteConfigFont("Creepster Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DOTO: {  
                    WriteConfigFont("Doto ExtraBold Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FOLDIT: {
                    WriteConfigFont("Foldit SemiBold Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FREDERICKA: {
                    WriteConfigFont("Fredericka the Great Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_FRIJOLE: {
                    WriteConfigFont("Frijole Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_GWENDOLYN: {
                    WriteConfigFont("Gwendolyn Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_HANDJET: {
                    WriteConfigFont("Handjet Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_INKNUT: {
                    WriteConfigFont("Inknut Antiqua Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_JACQUARD: {
                    WriteConfigFont("Jacquard 12 Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_JACQUARDA: {
                    WriteConfigFont("Jacquarda Bastarda 9 Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KAVOON: {
                    WriteConfigFont("Kavoon Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KUMAR_ONE_OUTLINE: {
                    WriteConfigFont("Kumar One Outline Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_KUMAR_ONE: {
                    WriteConfigFont("Kumar One Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_LAKKI_REDDY: {
                    WriteConfigFont("Lakki Reddy Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_LICORICE: {
                    WriteConfigFont("Licorice Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MA_SHAN_ZHENG: {
                    WriteConfigFont("Ma Shan Zheng Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MOIRAI_ONE: {
                    WriteConfigFont("Moirai One Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_MYSTERY_QUEST: {
                    WriteConfigFont("Mystery Quest Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_NOTO_NASTALIQ: {
                    WriteConfigFont("Noto Nastaliq Urdu Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PIEDRA: {
                    WriteConfigFont("Piedra Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PIXELIFY: {
                    WriteConfigFont("Pixelify Sans Medium Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PRESS_START: {
                    WriteConfigFont("Press Start 2P Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_BUBBLES: {
                    WriteConfigFont("Rubik Bubbles Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_BURNED: {
                    WriteConfigFont("Rubik Burned Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_GLITCH: {
                    WriteConfigFont("Rubik Glitch Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_MARKER_HATCH: {
                    WriteConfigFont("Rubik Marker Hatch Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_PUDDLES: {
                    WriteConfigFont("Rubik Puddles Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_VINYL: {
                    WriteConfigFont("Rubik Vinyl Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUBIK_WET_PAINT: {
                    WriteConfigFont("Rubik Wet Paint Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_RUGE_BOOGIE: {
                    WriteConfigFont("Ruge Boogie Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_SEVILLANA: {
                    WriteConfigFont("Sevillana Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_SILKSCREEN: {
                    WriteConfigFont("Silkscreen Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_STICK: {
                    WriteConfigFont("Stick Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_UNDERDOG: {
                    WriteConfigFont("Underdog Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_WALLPOET: {
                    WriteConfigFont("Wallpoet Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_YESTERYEAR: {
                    WriteConfigFont("Yesteryear Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_ZCOOL_KUAILE: {
                    WriteConfigFont("ZCOOL KuaiLe Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_PROFONT: {
                    WriteConfigFont("ProFont IIx Nerd Font Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDC_FONT_DADDYTIME: {
                    WriteConfigFont("DaddyTimeMono Nerd Font Propo Essence.ttf");
                    goto refresh_window;
                }
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();

                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        ShowWindow(hwnd, SW_SHOW);  
                        
                        CLOCK_COUNT_UP = FALSE;
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0; // Ensure total time is reset
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, 100, NULL); // Reduce interval to 100ms for higher refresh rate
                    } else {
                        KillTimer(hwnd, 1);   
                        // When canceling showing current time, fully reset state instead of restoring previous state
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0;
                        message_shown = 0;   // Reset message shown state
                        // Set timer with longer interval because second-level updates are no longer needed
                        SetTimer(hwnd, 1, 1000, NULL); 
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_24HOUR_FORMAT: {  
                    CLOCK_USE_24HOUR = !CLOCK_USE_24HOUR;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        FILE *fp = fopen(config_path, "r");
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
                case CLOCK_IDM_SHOW_SECONDS: {  
                    CLOCK_SHOW_SECONDS = !CLOCK_SHOW_SECONDS;
                    {
                        char config_path[MAX_PATH];
                        GetConfigPath(config_path, MAX_PATH);
                        
                        char currentStartupMode[20];
                        FILE *fp = fopen(config_path, "r");
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
                case CLOCK_IDM_RECENT_FILE_1:
                case CLOCK_IDM_RECENT_FILE_2:
                case CLOCK_IDM_RECENT_FILE_3:
                case CLOCK_IDM_RECENT_FILE_4:
                case CLOCK_IDM_RECENT_FILE_5: {
                    int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                    if (index < CLOCK_RECENT_FILES_COUNT) {
                        wchar_t wPath[MAX_PATH] = {0};
                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                        
                        if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                            // Step 1: Set as the current file to open on timeout
                            WriteConfigTimeoutFile(CLOCK_RECENT_FILES[index].path);
                            
                            // Step 2: Update the recent files list (move this file to the top)
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                            
                            // Clear invalid file path
                            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                            CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                            WriteConfigTimeoutAction("MESSAGE");
                            
                            // Remove this file from the recent files list
                            for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                            }
                            CLOCK_RECENT_FILES_COUNT--;
                            
                            // Update recent files list in the configuration file
                            char config_path[MAX_PATH];
                            GetConfigPath(config_path, MAX_PATH);
                            WriteConfig(config_path);
                        }
                    }
                    break;
                }
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
                        // Convert wide character path to UTF-8 to save in the configuration file
                        char utf8Path[MAX_PATH * 3] = {0}; // Larger buffer to accommodate UTF-8 encoding
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
                        
                        if (GetFileAttributesW(szFile) != INVALID_FILE_ATTRIBUTES) {
                            // Step 1: Set as the current file to open on timeout
                            WriteConfigTimeoutFile(utf8Path);
                            
                            // Step 2: Update the recent files list
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
                        
                        // Step 1: Set as the current file to open on timeout
                        WriteConfigTimeoutFile(utf8Path);
                        
                        // Step 2: Update the recent files list
                        SaveRecentFile(utf8Path);
                    }
                    break;
                }
                case CLOCK_IDM_COUNT_UP: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();

                    CLOCK_COUNT_UP = !CLOCK_COUNT_UP;
                    if (CLOCK_COUNT_UP) {
                        ShowWindow(hwnd, SW_SHOW);
                        
                        elapsed_time = 0;
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_START: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();

                    if (!CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = TRUE;
                        
                        // Ensure the timer starts from 0 every time it switches to count-up mode
                        countup_elapsed_time = 0;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                    } else {
                        // Already in count-up mode, so toggle pause/run state
                        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_RESET: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();

                    // Reset the count-up counter
                    extern void ResetTimer(void);
                    ResetTimer();
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDC_SET_COUNTDOWN_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        // Use a special parameter to indicate this is startup settings dialog
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
                        // Convert Unicode to ANSI for ParseInput
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
                case CLOCK_IDM_POMODORO_START: {
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Close all notification windows
                    CloseAllNotifications();
                    
                    if (!IsWindowVisible(hwnd)) {
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    
                    // Reset timer state
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    countdown_elapsed_time = 0;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    // Set work time
                    CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                    
                    // Initialize Pomodoro phase
                    extern void InitializePomodoro(void);
                    InitializePomodoro();
                    
                    // Save original timeout action
                    TimeoutActionType originalAction = CLOCK_TIMEOUT_ACTION;
                    
                    // Force set to show message
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                    
                    // Start the timer
                    KillTimer(hwnd, 1);
                    SetTimer(hwnd, 1, 1000, NULL);
                    
                    // Reset message state
                    elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_POMODORO_WORK:
                case CLOCK_IDM_POMODORO_BREAK:
                case CLOCK_IDM_POMODORO_LBREAK:
                    // Keep original menu item ID handling
                    {
                        int selectedIndex = 0;
                        if (LOWORD(wp) == CLOCK_IDM_POMODORO_WORK) {
                            selectedIndex = 0;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_BREAK) {
                            selectedIndex = 1;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_LBREAK) {
                            selectedIndex = 2;
                        }
                        
                        // Use a common dialog to modify Pomodoro time
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), 
                                 MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
                                 hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
                        
                        // Process input result
                        if (inputText[0] && !isAllSpacesOnly(inputText)) {
                            int total_seconds = 0;
                            // Convert Unicode to ANSI for ParseInput
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                                // Update selected time
                                POMODORO_TIMES[selectedIndex] = total_seconds;
                                
                                // Use existing function to update configuration
                                // IMPORTANT: Add config write for dynamic IDs
                                WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
                                
                                // Update core variables
                                if (selectedIndex == 0) POMODORO_WORK_TIME = total_seconds;
                                else if (selectedIndex == 1) POMODORO_SHORT_BREAK = total_seconds;
                                else if (selectedIndex == 2) POMODORO_LONG_BREAK = total_seconds;
                            }
                        }
                    }
                    break;

                // Also handle new dynamic ID range
                case 600: case 601: case 602: case 603: case 604:
                case 605: case 606: case 607: case 608: case 609:
                    // Handle Pomodoro time setting options (dynamic ID range)
                    {
                        // Calculate the selected option index
                        int selectedIndex = LOWORD(wp) - CLOCK_IDM_POMODORO_TIME_BASE;
                        
                        if (selectedIndex >= 0 && selectedIndex < POMODORO_TIMES_COUNT) {
                            // Use a common dialog to modify Pomodoro time
                            memset(inputText, 0, sizeof(inputText));
                            DialogBoxParamW(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
                                     hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
                            
                            // Process input result
                            if (inputText[0] && !isAllSpacesOnly(inputText)) {
                                int total_seconds = 0;
                                // Convert Unicode to ANSI for ParseInput
                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                                    // Update selected time
                                    POMODORO_TIMES[selectedIndex] = total_seconds;
                                    
                                    // Use existing function to update configuration
                                    // IMPORTANT: Add config write for dynamic IDs
                                    WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
                                    
                                    // Update core variables
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
                    // Stop any notification sound that may be playing
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    // Call ResetTimer to reset the timer state
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    // If currently in Pomodoro mode, reset related states
                    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME || 
                        CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK || 
                        CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK) {
                        // Restart the timer
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                    }
                    
                    // Force redraw of the window
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    // Ensure the window is on top and visible after reset
                    HandleWindowReset(hwnd);
                    break;
                }
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
                case CLOCK_IDM_RUN_COMMAND: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_RUN_COMMAND;
                    WriteConfigTimeoutAction("RUN_COMMAND");
                    break;
                }
                case CLOCK_IDM_HTTP_REQUEST: {
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_HTTP_REQUEST;
                    WriteConfigTimeoutAction("HTTP_REQUEST");
                    break;
                }
                case CLOCK_IDM_CHECK_UPDATE: {
                    // Call async update check function - non-silent mode
                    CheckForUpdateAsync(hwnd, FALSE);
                    break;
                }
                case CLOCK_IDM_OPEN_WEBSITE:
                    // Don't set action type immediately, wait for dialog result
                    ShowWebsiteDialog(hwnd);
                    break;
                
                case CLOCK_IDM_CURRENT_WEBSITE:
                    ShowWebsiteDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_COMBINATION:
                    ShowPomodoroComboDialog(hwnd);
                    break;
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
                case CLOCK_IDM_HOTKEY_SETTINGS: {
                    ShowHotkeySettingsDialog(hwnd);
                    // Register/reregister global hotkeys
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

refresh_window:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        case WM_WINDOWPOSCHANGED: {
            if (CLOCK_EDIT_MODE) {
                SaveWindowSettings(hwnd);
            }
            break;
        }
        case WM_RBUTTONUP: {
            if (CLOCK_EDIT_MODE) {
                EndEditMode(hwnd);
                return 0;
            }
            break;
        }
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
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
            if (lpdis->CtlType == ODT_MENU) {
                int colorIndex = lpdis->itemID - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
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
        case WM_MENUSELECT: {
            UINT menuItem = LOWORD(wp);
            UINT flags = HIWORD(wp);
            HMENU hMenu = (HMENU)lp;

            if (!(flags & MF_POPUP) && hMenu != NULL) {
                int colorIndex = menuItem - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(PREVIEW_COLOR, COLOR_OPTIONS[colorIndex].hexColor, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }

                for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
                    if (fontResources[i].menuId == menuItem) {
                        strncpy(PREVIEW_FONT_NAME, fontResources[i].fontName, 99);
                        PREVIEW_FONT_NAME[99] = '\0';
                        
                        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, 99);
                        PREVIEW_INTERNAL_NAME[99] = '\0';
                        char* dot = strrchr(PREVIEW_INTERNAL_NAME, '.');
                        if (dot) *dot = '\0';
                        
                        LoadFontByName((HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
                                     fontResources[i].fontName);
                        
                        IS_PREVIEWING = TRUE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
                
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (flags & MF_POPUP) {
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                    IS_PREVIEWING = FALSE;
                    IS_COLOR_PREVIEWING = FALSE;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        case WM_EXITMENULOOP: {
            if (IS_PREVIEWING || IS_COLOR_PREVIEWING) {
                IS_PREVIEWING = FALSE;
                IS_COLOR_PREVIEWING = FALSE;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }
        case WM_RBUTTONDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                // Toggle edit mode
                CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                
                if (CLOCK_EDIT_MODE) {
                    // Entering edit mode
                    SetClickThrough(hwnd, FALSE);
                } else {
                    // Exiting edit mode
                    SetClickThrough(hwnd, TRUE);
                    SaveWindowSettings(hwnd);  // Save settings when exiting edit mode
                    WriteConfigColor(CLOCK_TEXT_COLOR);  // Save the current color to config
                }
                
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            SaveWindowSettings(hwnd);  // Save window settings before closing
            DestroyWindow(hwnd);  // Close the window
            break;
        }
        case WM_LBUTTONDBLCLK: {
            if (!CLOCK_EDIT_MODE) {
                // Enter edit mode
                StartEditMode(hwnd);
                return 0;
            }
            break;
        }
        case WM_HOTKEY: {
            // WM_HOTKEY message's lp contains key information
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
                // Check if the input dialog already exists
                if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
                    // The dialog already exists, close it
                    SendMessage(g_hwndInputDialog, WM_CLOSE, 0, 0);
                    return 0;
                }
                
                // Reset notification flag to ensure notification can be shown when countdown ends
                extern BOOL countdown_message_shown;
                countdown_message_shown = FALSE;
                
                // Ensure latest notification configuration is read
                extern void ReadNotificationTypeConfig(void);
                ReadNotificationTypeConfig();
                
                // Show input dialog to set countdown
                extern int elapsed_time;
                extern BOOL message_shown;
                
                // Clear input text
                memset(inputText, 0, sizeof(inputText));
                
                // Show input dialog
                INT_PTR result = DialogBoxParamW(GetModuleHandle(NULL), 
                                         MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), 
                                         hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);
                
                // If the dialog has input and was confirmed
                if (inputText[0] != L'\0') {
                                        // Check if input is valid
                    int total_seconds = 0;
                    // Convert Unicode to ANSI for ParseInput
                    char inputTextA[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                    if (ParseInput(inputTextA, &total_seconds)) {
                        // Stop any notification sound that may be playing
                        extern void StopNotificationSound(void);
                        StopNotificationSound();
                        
                        // Close all notification windows
                        CloseAllNotifications();
                        
                        // Set countdown state
                        CLOCK_TOTAL_TIME = total_seconds;
                        countdown_elapsed_time = 0;
                        elapsed_time = 0;
                        message_shown = FALSE;
                        countdown_message_shown = FALSE;
                        
                        // Switch to countdown mode
                        CLOCK_COUNT_UP = FALSE;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_IS_PAUSED = FALSE;
                        
                        // Stop and restart the timer
                        KillTimer(hwnd, 1);
                        SetTimer(hwnd, 1, 1000, NULL);
                        
                        // Refresh window display
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN1) {
                // Start quick countdown 1
                StartQuickCountdown1(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN2) {
                // Start quick countdown 2
                StartQuickCountdown2(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN3) {
                // Start quick countdown 3
                StartQuickCountdown3(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_POMODORO) {
                // Start Pomodoro timer
                StartPomodoroTimer(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_TOGGLE_VISIBILITY) {
                // Hide/show window
                if (IsWindowVisible(hwnd)) {
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                }
                return 0;
            } else if (wp == HOTKEY_ID_EDIT_MODE) {
                // Enter edit mode
                ToggleEditMode(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_PAUSE_RESUME) {
                // Pause/resume timer
                TogglePauseResume(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_RESTART_TIMER) {
                // Close all notification windows
                CloseAllNotifications();
                // Restart current timer
                RestartCurrentTimer(hwnd);
                return 0;
            }
            break;
        }
        // Handle reregistration message after hotkey settings change
        case WM_APP+1: {
            // Only reregister hotkeys, do not open dialog
            RegisterGlobalHotkeys(hwnd);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// External variable declarations
extern int CLOCK_DEFAULT_START_TIME;
extern int countdown_elapsed_time;
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern int CLOCK_TOTAL_TIME;

// Remove menu items
void RemoveMenuItems(HMENU hMenu, int count);

// Add menu item
void AddMenuItem(HMENU hMenu, UINT id, const wchar_t* text, BOOL isEnabled);

// Modify menu item text
void ModifyMenuItemText(HMENU hMenu, UINT id, const wchar_t* text);

/**
 * @brief Toggle show current time mode
 * @param hwnd Window handle
 */
void ToggleShowTimeMode(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // If not currently in show current time mode, then enable it
    // If already in show current time mode, do nothing (don't turn it off)
    if (!CLOCK_SHOW_CURRENT_TIME) {
        // Switch to show current time mode
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        
        // Reset the timer to ensure the update frequency is correct
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, 100, NULL);  // Use 100ms update frequency to keep the time display smooth
        
        // Refresh the window
        InvalidateRect(hwnd, NULL, TRUE);
    }
    // Already in show current time mode, do nothing
}

/**
 * @brief Start count up timer
 * @param hwnd Window handle
 */
void StartCountUp(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Declare external variables
    extern int countup_elapsed_time;
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    // Reset count up counter
    countup_elapsed_time = 0;
    
    // Set to count up mode
    CLOCK_COUNT_UP = TRUE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_IS_PAUSED = FALSE;
    
    // If it was previously in show current time mode, stop the old timer and start a new one
    if (wasShowingTime) {
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, 1000, NULL); // Set to update once per second
    }
    
    // Refresh the window
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start default countdown
 * @param hwnd Window handle
 */
void StartDefaultCountDown(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Reset notification flag to ensure notification can be shown when countdown ends
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    // Ensure latest notification configuration is read
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    // Set mode
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (CLOCK_DEFAULT_START_TIME > 0) {
        CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        
        // If it was previously in show current time mode, stop the old timer and start a new one
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL); // Set to update once per second
        }
    } else {
        // If no default countdown is set, show the settings dialog
        PostMessage(hwnd, WM_COMMAND, 101, 0);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start Pomodoro timer
 * @param hwnd Window handle
 */
void StartPomodoroTimer(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    // If it was previously in show current time mode, stop the old timer
    if (wasShowingTime) {
        KillTimer(hwnd, 1);
    }
    
    // Use the Pomodoro menu item command to start the Pomodoro timer
    PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
}

/**
 * @brief Toggle edit mode
 * @param hwnd Window handle
 */
void ToggleEditMode(HWND hwnd) {
    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
    
    if (CLOCK_EDIT_MODE) {
        // Record the current topmost state
        PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
        
        // If not currently topmost, set it to topmost
        if (!CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
        }
        
        // Apply blur effect
        SetBlurBehind(hwnd, TRUE);
        
        // Disable click-through
        SetClickThrough(hwnd, FALSE);
        
        // Ensure the window is visible and in the foreground
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else {
        // Remove blur effect
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        // Restore click-through
        SetClickThrough(hwnd, TRUE);
        
        // If it was not topmost before, restore to non-topmost
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            // Force redraw and schedule a delayed re-assertion to ensure visibility
            // This follows the same pattern used for topmost toggle
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, 1002);
            SetTimer(hwnd, 1002, 150, NULL);
            return; // Early return to avoid duplicate InvalidateRect call
        }
        
        // Save window settings and color settings
        SaveWindowSettings(hwnd);
        WriteConfigColor(CLOCK_TEXT_COLOR);
    }
    
    // Refresh the window
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Pause/resume timer
 * @param hwnd Window handle
 */
void TogglePauseResume(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Only effective when not in show time mode
    if (!CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Restart the current timer
 * @param hwnd Window handle
 */
void RestartCurrentTimer(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Only effective when not in show time mode
    if (!CLOCK_SHOW_CURRENT_TIME) {
        // Variables imported from main.c
        extern int elapsed_time;
        extern BOOL message_shown;
        
        // Reset message shown state to allow notification and sound to be played again when timer ends
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        if (CLOCK_COUNT_UP) {
            // Reset count up timer
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            // Reset countdown timer
            countdown_elapsed_time = 0;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = FALSE;
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Start quick countdown 1
 * @param hwnd Window handle
 */
void StartQuickCountdown1(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Reset notification flag to ensure notification can be shown when countdown ends
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    // Ensure latest notification configuration is read
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    // Check if there is at least one preset time option
    if (time_options_count > 0) {
        CLOCK_TOTAL_TIME = time_options[0]; // Already in seconds
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        
        // If it was previously in show current time mode, stop the old timer and start a new one
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL); // Set to update once per second
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        // No preset -> fallback to default countdown
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start quick countdown 2
 * @param hwnd Window handle
 */
void StartQuickCountdown2(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Reset notification flag to ensure notification can be shown when countdown ends
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    // Ensure latest notification configuration is read
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    // Check if there are at least two preset time options
    if (time_options_count > 1) {
        CLOCK_TOTAL_TIME = time_options[1]; // Already in seconds
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        
        // If it was previously in show current time mode, stop the old timer and start a new one
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL); // Set to update once per second
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        // Not enough preset -> fallback to default countdown
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start quick countdown 3
 * @param hwnd Window handle
 */
void StartQuickCountdown3(HWND hwnd) {
    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    // Close all notification windows
    CloseAllNotifications();
    
    // Reset notification flag to ensure notification can be shown when countdown ends
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    // Ensure latest notification configuration is read
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    // Save previous state to determine if timer needs to be reset
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    // Check if there are at least three preset time options
    if (time_options_count > 2) {
        CLOCK_TOTAL_TIME = time_options[2]; // Already in seconds
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        
        // If it was previously in show current time mode, stop the old timer and start a new one
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL); // Set to update once per second
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        // Not enough preset -> fallback to default countdown
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start quick countdown by 1-based preset index
 */
void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    // Stop any notification sound that may be playing
    extern void StopNotificationSound(void);
    StopNotificationSound();

    // Close all notification windows
    CloseAllNotifications();

    // Reset notification flag to ensure notification can be shown when countdown ends
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;

    // Ensure latest notification configuration is read
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();

    extern int time_options[];
    extern int time_options_count;

    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;

    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;

    int zeroBased = index - 1;
    if (zeroBased >= 0 && zeroBased < time_options_count) {
        CLOCK_TOTAL_TIME = time_options[zeroBased]; // in seconds
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;

        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
        }

        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        // Out of range -> fallback to default countdown
        StartDefaultCountDown(hwnd);
    }
}
