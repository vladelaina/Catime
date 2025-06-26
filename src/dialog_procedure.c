/**
 * @file dialog_procedure.c
 * @brief Implementation of dialog message handling procedures
 * 
 * This file implements the dialog message handling callback functions for the application,
 * processing all dialog message events including initialization, color management, button clicks, and keyboard events.
 */

#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
#include <strsafe.h>
#include <shlobj.h>
#include <uxtheme.h>
#include "../resource/resource.h"
#include "../include/dialog_procedure.h"
#include "../include/language.h"
#include "../include/config.h"
#include "../include/audio_player.h" 
#include "../include/window_procedure.h"  // Add window handling header to use RegisterGlobalHotkeys and UnregisterGlobalHotkeys functions
#include "../include/hotkey.h"  // Include hotkey management header
#include "../include/dialog_language.h"  // Add dialog language support header

// Function declaration
static void DrawColorSelectButton(HDC hdc, HWND hwnd);

// Variables imported from main.c
extern char inputText[256];

// External declarations for pomodoro related variables
#define MAX_POMODORO_TIMES 10  // Keep the maximum number of pomodoro time entries unchanged
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // Store all pomodoro times
extern int POMODORO_TIMES_COUNT;               // Actual number of pomodoro times
extern int POMODORO_WORK_TIME;                 // Pomodoro work time (seconds)
extern int POMODORO_SHORT_BREAK;               // Pomodoro short break time (seconds)
extern int POMODORO_LONG_BREAK;                // Pomodoro long break time (seconds)
extern int POMODORO_LOOP_COUNT;                // Pomodoro loop count

// Store old edit control procedure
WNDPROC wpOrigEditProc;

// Add global variable to track about dialog handle
static HWND g_hwndAboutDlg = NULL;

// Add global variable to track error dialog handle
static HWND g_hwndErrorDlg = NULL;

// Add global variable to track countdown input dialog handle
HWND g_hwndInputDialog = NULL;

// Add subclassing procedure for loop count edit box
static WNDPROC wpOrigLoopEditProc;  // Store original edit control procedure

// Add constant strings
#define URL_GITHUB_REPO L"https://github.com/vladelaina/Catime"

// Subclassing procedure for edit controls
LRESULT APIENTRY EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static BOOL firstKeyProcessed = FALSE;
    
    switch (msg) {
    case WM_SETFOCUS:
        // When getting focus, ensure all text is selected
        PostMessage(hwnd, EM_SETSEL, 0, -1);
        // Reset first key flag
        firstKeyProcessed = FALSE;
        break;
        
    case WM_KEYDOWN:
        // Handle first key press issue
        if (!firstKeyProcessed) {
            // Force clear all modifier key states
            // This helps resolve hotkey residual state issues
            firstKeyProcessed = TRUE;
            
            // Mark that we've processed the first key, but don't do special handling
            // Let the system handle this key normally to avoid duplicate input
        }
        
        // Enter key handling
        if (wParam == VK_RETURN) {
            // Send BM_CLICK message to the parent window's OK button
            HWND hwndOkButton = GetDlgItem(GetParent(hwnd), CLOCK_IDC_BUTTON_OK);
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwndOkButton);
            return 0;
        }
        // Ctrl+A select all handling
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    
    case WM_CHAR:
        // Prevent Ctrl+A from generating a character to avoid alert sound
        if (wParam == 1 || (wParam == 'a' || wParam == 'A') && GetKeyState(VK_CONTROL) < 0) {
            return 0;
        }
        // Prevent Enter key from generating character messages for further processing to avoid alert sound
        if (wParam == VK_RETURN) { // VK_RETURN (0x0D) is the char code for Enter
            return 0;
        }
        break;
    }
    
    return CallWindowProc(wpOrigEditProc, hwnd, msg, wParam, lParam);
}

// Add error dialog handling function declaration at the beginning of the file
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// Add function to display error dialog
void ShowErrorDialog(HWND hwndParent) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(IDD_ERROR_DIALOG), 
             hwndParent, 
             ErrorDlgProc);
}

// Add error dialog handling function
INT_PTR CALLBACK ErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            // Set localized error prompt text
            SetDlgItemTextW(hwndDlg, IDC_ERROR_TEXT, 
                GetLocalizedString(L"输入格式无效，请重新输入。", L"Invalid input format, please try again."));
            
            // Set dialog title
            SetWindowTextW(hwndDlg, GetLocalizedString(L"错误", L"Error"));
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief Input dialog procedure
 * @param hwndDlg Dialog handle
 * @param msg Message type
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return INT_PTR Message processing result
 * 
 * Handles the countdown input dialog's:
 * 1. Control initialization and focus setting
 * 2. Background/control color management
 * 3. OK button click processing
 * 4. Enter key response
 * 5. Resource cleanup
 */
INT_PTR CALLBACK DlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // Save dialog ID to GWLP_USERDATA
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            // Save dialog handle
            g_hwndInputDialog = hwndDlg;
            
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));

            DWORD dlgId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
            
            // Apply multi-language support - generalized handling
            ApplyDialogLanguage(hwndDlg, (int)dlgId);

            // Check if dialog ID is for quick countdown options dialog, and if so set title (this part may be overridden by ApplyDialogLanguage, but keep it just in case)
            if (dlgId == CLOCK_IDD_SHORTCUT_DIALOG) { 
                // SetWindowTextW(hwndDlg, GetLocalizedString(L"Countdown Presets", L"Countdown Presets"));
                // The line above is handled by ApplyDialogLanguage if g_dialogTitles contains CLOCK_IDD_SHORTCUT_DIALOG
            }
            
            // Get handle of the edit control
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);

            // Subclass the edit control
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);

            // Ensure input box gets focus - use multiple methods to guarantee focus setting
            SetFocus(hwndEdit);
            
            // Use multiple delayed messages with different delay times to ensure focus and text selection take effect correctly
            PostMessage(hwndDlg, WM_APP+100, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+101, 0, (LPARAM)hwndEdit);
            PostMessage(hwndDlg, WM_APP+102, 0, (LPARAM)hwndEdit);
            
            // Select all text in edit box
            SendDlgItemMessage(hwndDlg, CLOCK_IDC_EDIT, EM_SETSEL, 0, -1);
            
            // Set default button ID
            SendMessage(hwndDlg, DM_SETDEFID, CLOCK_IDC_BUTTON_OK, 0);

            // Set a special timer to set focus after the dialog is fully displayed
            SetTimer(hwndDlg, 9999, 50, NULL);
            
            // Force reset all modifier key states (prevent hotkey residual states)
            // This resolves the issue of the first key press being ignored after opening the dialog with a hotkey
            PostMessage(hwndDlg, WM_APP+103, 0, 0);
            
            // Set build time (optimized wide character handling)
            char month[4];
            int day, year, hour, min, sec;
            
            // Parse compiler-generated date and time
            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            // Convert month abbreviation to number
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            // Format date and time as YYYY/MM/DD HH:MM:SS and add UTC+8 identifier
            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                    year, month_num, day, hour, min, sec);

            // Set control text
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return FALSE;  
        }

        // Add code to handle WM_CLOSE message, when closing dialog via shortcut key, don't check input validity
        case WM_CLOSE: {
            // Directly close the dialog without input validation
            g_hwndInputDialog = NULL;
            EndDialog(hwndDlg, 0);
            return TRUE;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkColor(hdcStatic, RGB(0xF3, 0xF3, 0xF3));
            if (!hBackgroundBrush) {
                hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            }
            return (INT_PTR)hBackgroundBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetBkColor(hdcEdit, RGB(0xFF, 0xFF, 0xFF));
            if (!hEditBrush) {
                hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            }
            return (INT_PTR)hEditBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdcBtn = (HDC)wParam;
            SetBkColor(hdcBtn, RGB(0xFD, 0xFD, 0xFD));
            if (!hButtonBrush) {
                hButtonBrush = CreateSolidBrush(RGB(0xFD, 0xFD, 0xFD));
            }
            return (INT_PTR)hButtonBrush;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, inputText, sizeof(inputText));
                
                // Check if the input is empty or contains only spaces
                BOOL isAllSpaces = TRUE;
                for (int i = 0; inputText[i]; i++) {
                    if (!isspace((unsigned char)inputText[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                if (inputText[0] == '\0' || isAllSpaces) {
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, 0);
                    return TRUE;
                }
                
                int total_seconds;
                if (ParseInput(inputText, &total_seconds)) {
                                    // Call different configuration update functions based on dialog ID
                int dialogId = GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
                if (dialogId == CLOCK_IDD_POMODORO_TIME_DIALOG) {
                    // General pomodoro time settings, specific update logic handled by caller
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK); // Return IDOK to indicate success
                } else if (dialogId == CLOCK_IDD_POMODORO_LOOP_DIALOG) {
                    // Pomodoro loop count
                    WriteConfigPomodoroLoopCount(total_seconds);
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK);
                } else if (dialogId == CLOCK_IDD_STARTUP_DIALOG) {
                    // Only CLOCK_IDD_STARTUP_DIALOG (i.e., "Preset Management"->"Startup Settings"->"Countdown") will modify the default start time
                    WriteConfigDefaultStartTime(total_seconds);
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK);
                } else if (dialogId == CLOCK_IDD_SHORTCUT_DIALOG) {
                    // Countdown preset management
                    WriteConfigDefaultStartTime(total_seconds);
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK);
                } else {
                    // For other dialog IDs (including CLOCK_IDD_DIALOG1, i.e., regular countdown), don't modify default start time configuration
                    g_hwndInputDialog = NULL;
                    EndDialog(hwndDlg, IDOK); // Just assume successful input acquisition
                }
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetWindowTextA(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT), "");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                return TRUE;
            }
            break;

        case WM_TIMER:
            if (wParam == 9999) {
                // Timer used to set focus after the dialog is fully displayed
                KillTimer(hwndDlg, 9999);
                
                HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
                if (hwndEdit && IsWindow(hwndEdit)) {
                    // Ensure window is in the foreground
                    SetForegroundWindow(hwndDlg);
                    // Ensure edit box gets focus
                    SetFocus(hwndEdit);
                    // Select all text
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
                return TRUE;
            }
            break;

        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                int dlgId = GetDlgCtrlID((HWND)lParam);
                if (dlgId == CLOCK_IDD_COLOR_DIALOG) {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                } else {
                    SendMessage(hwndDlg, WM_COMMAND, CLOCK_IDC_BUTTON_OK, 0);
                }
                return TRUE;
            }
            break;

        case WM_APP+100:
        case WM_APP+101:
        case WM_APP+102:
            // Delayed execution of focus and selection settings
            if (lParam) {
                HWND hwndEdit = (HWND)lParam;
                // Ensure window is valid and visible
                if (IsWindow(hwndEdit) && IsWindowVisible(hwndEdit)) {
                    // Ensure window is in foreground
                    SetForegroundWindow(hwndDlg);
                    // Set focus to input box
                    SetFocus(hwndEdit);
                    // Select all text
                    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
                }
            }
            return TRUE;
            
        case WM_APP+103:
            // Force reset all modifier key states
            // Simulate release of all possible modifier keys
            // This helps clear any potentially lingering hotkey states
            {
                INPUT inputs[8] = {0};
                int inputCount = 0;
                
                // Left Shift key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Right Shift key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RSHIFT;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Left Ctrl key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Right Ctrl key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RCONTROL;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Left Alt key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Right Alt key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RMENU;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Left Win key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_LWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Right Win key release
                inputs[inputCount].type = INPUT_KEYBOARD;
                inputs[inputCount].ki.wVk = VK_RWIN;
                inputs[inputCount].ki.dwFlags = KEYEVENTF_KEYUP;
                inputCount++;
                
                // Send all key release events
                SendInput(inputCount, inputs, sizeof(INPUT));
            }
            return TRUE;

        case WM_DESTROY:
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // Release resources
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            
            // Clear dialog handle
            g_hwndInputDialog = NULL;
            }
            break;
    }
    return FALSE;
}

// About Dialog Procedure
INT_PTR CALLBACK AboutDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hLargeIcon = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // Load large icon, using the defined size
            hLargeIcon = (HICON)LoadImage(GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDI_CATIME),
                IMAGE_ICON,
                ABOUT_ICON_SIZE,    // Use the defined size
                ABOUT_ICON_SIZE,    // Use the defined size
                LR_DEFAULTCOLOR);
            
            if (hLargeIcon) {
                // Set the icon for the static control
                SendDlgItemMessage(hwndDlg, IDC_ABOUT_ICON, STM_SETICON, (WPARAM)hLargeIcon, 0);
            }
            
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, IDD_ABOUT_DIALOG);
            
            // Set version information (will override the version setting from ApplyDialogLanguage)
            const wchar_t* versionFormat = GetDialogLocalizedString(IDD_ABOUT_DIALOG, IDC_VERSION_TEXT);
            if (versionFormat) {
                wchar_t versionText[256];
                StringCbPrintfW(versionText, sizeof(versionText), versionFormat, CATIME_VERSION);
                SetDlgItemTextW(hwndDlg, IDC_VERSION_TEXT, versionText);
            }

            // Set link texts
            SetDlgItemTextW(hwndDlg, IDC_CREDIT_LINK, GetLocalizedString(L"特别感谢猫屋敷梨梨Official提供的图标", L"Special thanks to Neko House Lili Official for the icon"));
            SetDlgItemTextW(hwndDlg, IDC_CREDITS, GetLocalizedString(L"鸣谢", L"Credits"));
            SetDlgItemTextW(hwndDlg, IDC_BILIBILI_LINK, GetLocalizedString(L"BiliBili", L"BiliBili"));
            SetDlgItemTextW(hwndDlg, IDC_GITHUB_LINK, GetLocalizedString(L"GitHub", L"GitHub"));
            SetDlgItemTextW(hwndDlg, IDC_COPYRIGHT_LINK, GetLocalizedString(L"版权声明", L"Copyright Notice"));
            SetDlgItemTextW(hwndDlg, IDC_SUPPORT, GetLocalizedString(L"支持", L"Support"));

            // Set build time (optimized wide character handling)
            char month[4];
            int day, year, hour, min, sec;
            
            // Parse date and time generated by compiler
            sscanf(__DATE__, "%3s %d %d", month, &day, &year);
            sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

            // Convert month abbreviation to number
            const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
            int month_num = 0;
            while (++month_num <= 12 && strcmp(month, months[month_num-1]));

            // Get localized date format string
            const wchar_t* dateFormat = GetLocalizedString(L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)",
                                                         L"Build Date: %04d/%02d/%02d %02d:%02d:%02d (UTC+8)");
            
            // Format date and time
            wchar_t timeStr[60];
            StringCbPrintfW(timeStr, sizeof(timeStr), dateFormat,
                    year, month_num, day, hour, min, sec);

            // Set control text
            SetDlgItemTextW(hwndDlg, IDC_BUILD_DATE, timeStr);

            return TRUE;
        }

        case WM_DESTROY:
            if (hLargeIcon) {
                DestroyIcon(hLargeIcon);
                hLargeIcon = NULL;
            }
            g_hwndAboutDlg = NULL;  // Clear dialog handle
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, LOWORD(wParam));
                g_hwndAboutDlg = NULL;
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDIT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://space.bilibili.com/26087398", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_BILIBILI_LINK) {
                // Directly open BiliBili personal homepage
                ShellExecuteW(NULL, L"open", URL_BILIBILI_SPACE, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_GITHUB_LINK) {
                ShellExecuteW(NULL, L"open", URL_GITHUB_REPO, NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_CREDITS) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/#thanks", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_SUPPORT) {
                ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support.html", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            if (LOWORD(wParam) == IDC_COPYRIGHT_LINK) {
                ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime#️copyright-notice", NULL, NULL, SW_SHOWNORMAL);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            // Close all child dialogs
            EndDialog(hwndDlg, 0);
            g_hwndAboutDlg = NULL;  // Clear dialog handle
            return TRUE;

        case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wParam;
            HWND hwndCtl = (HWND)lParam;
            
            if (GetDlgCtrlID(hwndCtl) == IDC_CREDIT_LINK || 
                GetDlgCtrlID(hwndCtl) == IDC_BILIBILI_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_GITHUB_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_CREDITS ||
                GetDlgCtrlID(hwndCtl) == IDC_COPYRIGHT_LINK ||
                GetDlgCtrlID(hwndCtl) == IDC_SUPPORT) {
                SetTextColor(hdc, 0x00D26919); // Keep the same orange color (BGR format)
                SetBkMode(hdc, TRANSPARENT);
                return (INT_PTR)GetStockObject(NULL_BRUSH);
            }
            break;
        }
    }
    return FALSE;
}

// Add DPI awareness related type definitions (if not provided by the compiler)
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
// DPI_AWARENESS_CONTEXT is a HANDLE
typedef HANDLE DPI_AWARENESS_CONTEXT;
// Related DPI context constants definition
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

// Show the About dialog
void ShowAboutDialog(HWND hwndParent) {
    // If an About dialog already exists, close it first
    if (g_hwndAboutDlg != NULL && IsWindow(g_hwndAboutDlg)) {
        EndDialog(g_hwndAboutDlg, 0);
        g_hwndAboutDlg = NULL;
    }
    
    // Save current DPI awareness context
    HANDLE hOldDpiContext = NULL;
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        // Function pointer type definitions
        typedef HANDLE (WINAPI* GetThreadDpiAwarenessContextFunc)(void);
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        
        GetThreadDpiAwarenessContextFunc getThreadDpiAwarenessContextFunc = 
            (GetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "GetThreadDpiAwarenessContext");
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (getThreadDpiAwarenessContextFunc && setThreadDpiAwarenessContextFunc) {
            // Save current DPI context
            hOldDpiContext = getThreadDpiAwarenessContextFunc();
            // Set to per-monitor DPI awareness V2 mode
            setThreadDpiAwarenessContextFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    
    // Create new About dialog
    g_hwndAboutDlg = CreateDialog(GetModuleHandle(NULL), 
                                 MAKEINTRESOURCE(IDD_ABOUT_DIALOG), 
                                 hwndParent, 
                                 AboutDlgProc);
    
    // Restore original DPI awareness context
    if (hUser32 && hOldDpiContext) {
        typedef HANDLE (WINAPI* SetThreadDpiAwarenessContextFunc)(HANDLE);
        SetThreadDpiAwarenessContextFunc setThreadDpiAwarenessContextFunc = 
            (SetThreadDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
        
        if (setThreadDpiAwarenessContextFunc) {
            setThreadDpiAwarenessContextFunc(hOldDpiContext);
        }
    }
    
    ShowWindow(g_hwndAboutDlg, SW_SHOW);
}

// Add global variable to track pomodoro loop count setting dialog handle
static HWND g_hwndPomodoroLoopDialog = NULL;

void ShowPomodoroLoopDialog(HWND hwndParent) {
    if (!g_hwndPomodoroLoopDialog) {
        g_hwndPomodoroLoopDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_LOOP_DIALOG),
            hwndParent,
            PomodoroLoopDlgProc
        );
        if (g_hwndPomodoroLoopDialog) {
            ShowWindow(g_hwndPomodoroLoopDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroLoopDialog);
    }
}

// Add subclassing procedure for loop count edit box
LRESULT APIENTRY LoopEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {
            // 发送BM_CLICK消息给父窗口（对话框）
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(CLOCK_IDC_BUTTON_OK, BN_CLICKED), (LPARAM)hwnd);
            return 0;
        }
        // 处理Ctrl+A全选
        if (wParam == 'A' && GetKeyState(VK_CONTROL) < 0) {
            SendMessage(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_CHAR: {
        // 处理Ctrl+A的字符消息，防止发出提示音
        if (GetKeyState(VK_CONTROL) < 0 && (wParam == 1 || wParam == 'a' || wParam == 'A')) {
            return 0;
        }
        // 阻止回车键产生的字符消息进一步处理，防止发出提示音
        if (wParam == VK_RETURN) { // VK_RETURN (0x0D) is the char code for Enter
            return 0;
        }
        break;
    }
    }
    return CallWindowProc(wpOrigLoopEditProc, hwnd, uMsg, wParam, lParam);
}

// Modify helper function to handle numeric input with spaces
BOOL IsValidNumberInput(const wchar_t* str) {
    // Check if empty
    if (!str || !*str) {
        return FALSE;
    }
    
    BOOL hasDigit = FALSE;  // Used to track if at least one digit is found
    wchar_t cleanStr[16] = {0};  // Used to store cleaned string
    int cleanIndex = 0;
    
    // Traverse string, ignore spaces, only keep digits
    for (int i = 0; str[i]; i++) {
        if (iswdigit(str[i])) {
            cleanStr[cleanIndex++] = str[i];
            hasDigit = TRUE;
        } else if (!iswspace(str[i])) {  // If not a space and not a digit, then invalid
            return FALSE;
        }
    }
    
    return hasDigit;  // Return TRUE as long as there is at least one digit
}

// Modify PomodoroLoopDlgProc function
INT_PTR CALLBACK PomodoroLoopDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_LOOP_DIALOG);
            
            // Set static text
            SetDlgItemTextW(hwndDlg, CLOCK_IDC_STATIC, GetLocalizedString(L"请输入循环次数（1-100）：", L"Please enter loop count (1-100):"));
            
            // Set focus to edit box
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetFocus(hwndEdit);
            
            // Subclass edit control
            wpOrigLoopEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, 
                                                          (LONG_PTR)LoopEditSubclassProc);
            
            return FALSE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK) {
                wchar_t input_str[16];
                GetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, input_str, sizeof(input_str)/sizeof(wchar_t));
                
                // Check if input is empty or contains only spaces
                BOOL isAllSpaces = TRUE;
                for (int i = 0; input_str[i]; i++) {
                    if (!iswspace(input_str[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (input_str[0] == L'\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndPomodoroLoopDialog = NULL;
                    return TRUE;
                }
                
                // Validate input and handle spaces
                if (!IsValidNumberInput(input_str)) {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                    return TRUE;
                }
                
                // Extract digits (ignoring spaces)
                wchar_t cleanStr[16] = {0};
                int cleanIndex = 0;
                for (int i = 0; input_str[i]; i++) {
                    if (iswdigit(input_str[i])) {
                        cleanStr[cleanIndex++] = input_str[i];
                    }
                }
                
                int new_loop_count = _wtoi(cleanStr);
                if (new_loop_count >= 1 && new_loop_count <= 100) {
                    // Update configuration file and global variables
                    WriteConfigPomodoroLoopCount(new_loop_count);
                    EndDialog(hwndDlg, IDOK);
                    g_hwndPomodoroLoopDialog = NULL;
                } else {
                    ShowErrorDialog(hwndDlg);
                    SetDlgItemTextW(hwndDlg, CLOCK_IDC_EDIT, L"");
                    SetFocus(GetDlgItem(hwndDlg, CLOCK_IDC_EDIT));
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroLoopDialog = NULL;
                return TRUE;
            }
            break;

        case WM_DESTROY:
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigLoopEditProc);
            }
            break;

        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndPomodoroLoopDialog = NULL;
            return TRUE;
    }
    return FALSE;
}

// Add global variable to track website URL dialog handle
static HWND g_hwndWebsiteDialog = NULL;

// Website URL input dialog procedure
INT_PTR CALLBACK WebsiteDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;

    switch (msg) {
        case WM_INITDIALOG: {
            // Set dialog as modal
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
            
            // Set background and control colors
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // Subclass the edit control to support Enter key submission
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // If URL already exists, prefill the edit box
            if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, CLOCK_TIMEOUT_WEBSITE_URL);
            }
            
            // Apply multilingual support
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_WEBSITE_DIALOG);
            
            // Set focus to edit box and select all text
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // Because we manually set the focus
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || HIWORD(wParam) == BN_CLICKED) {
                char url[MAX_PATH] = {0};
                GetDlgItemText(hwndDlg, CLOCK_IDC_EDIT, url, sizeof(url));
                
                // Check if the input is empty or contains only spaces
                BOOL isAllSpaces = TRUE;
                for (int i = 0; url[i]; i++) {
                    if (!isspace((unsigned char)url[i])) {
                        isAllSpaces = FALSE;
                        break;
                    }
                }
                
                if (url[0] == '\0' || isAllSpaces) {
                    EndDialog(hwndDlg, IDCANCEL);
                    g_hwndWebsiteDialog = NULL;
                    return TRUE;
                }
                
                // Validate URL format - simple check, should at least contain http:// or https://
                if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
                    // Add https:// prefix
                    char tempUrl[MAX_PATH] = "https://";
                    StringCbCatA(tempUrl, sizeof(tempUrl), url);
                    StringCbCopyA(url, sizeof(url), tempUrl);
                }
                
                // Update configuration
                WriteConfigTimeoutWebsite(url);
                EndDialog(hwndDlg, IDOK);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // User cancelled, don't change timeout action
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndWebsiteDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // Release resources
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndWebsiteDialog = NULL;
            return TRUE;
    }
    
    return FALSE;
}

// Show website URL input dialog
void ShowWebsiteDialog(HWND hwndParent) {
    // Use modal dialog instead of modeless dialog, so we can know whether the user confirmed or cancelled
    INT_PTR result = DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(CLOCK_IDD_WEBSITE_DIALOG),
        hwndParent,
        WebsiteDialogProc
    );
    
    // Only when the user clicks OK and inputs a valid URL will IDOK be returned, at which point WebsiteDialogProc has already set CLOCK_TIMEOUT_ACTION
    // If the user cancels or closes the dialog, the timeout action won't be changed
}

// Set global variable to track the Pomodoro combination dialog handle
static HWND g_hwndPomodoroComboDialog = NULL;

// Add Pomodoro combination dialog procedure
INT_PTR CALLBACK PomodoroComboDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    static HBRUSH hButtonBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set background and control colors
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
            hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
            hButtonBrush = CreateSolidBrush(RGB(240, 240, 240));
            
            // Subclass the edit control to support Enter key submission
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Read current pomodoro time options from configuration and format for display
            char currentOptions[256] = {0};
            for (int i = 0; i < POMODORO_TIMES_COUNT; i++) {
                char timeStr[32];
                int seconds = POMODORO_TIMES[i];
                
                // Format time into human-readable format
                if (seconds >= 3600) {
                    int hours = seconds / 3600;
                    int mins = (seconds % 3600) / 60;
                    int secs = seconds % 60;
                    if (mins == 0 && secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh ", hours);
                    else if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm ", hours, mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dh%dm%ds ", hours, mins, secs);
                } else if (seconds >= 60) {
                    int mins = seconds / 60;
                    int secs = seconds % 60;
                    if (secs == 0)
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm ", mins);
                    else
                        StringCbPrintfA(timeStr, sizeof(timeStr), "%dm%ds ", mins, secs);
                } else {
                    StringCbPrintfA(timeStr, sizeof(timeStr), "%ds ", seconds);
                }
                
                StringCbCatA(currentOptions, sizeof(currentOptions), timeStr);
            }
            
            // Remove trailing space
            if (strlen(currentOptions) > 0 && currentOptions[strlen(currentOptions) - 1] == ' ') {
                currentOptions[strlen(currentOptions) - 1] = '\0';
            }
            
            // Set edit box text
            SetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, currentOptions);
            
            // Apply multilingual support - moved here to ensure all default text is covered
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_POMODORO_COMBO_DIALOG);
            
            // Set focus to edit box and select all text
            SetFocus(hwndEdit);
            SendMessage(hwndEdit, EM_SETSEL, 0, -1);
            
            return FALSE;  // Because we manually set the focus
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(240, 240, 240));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(255, 255, 255));
            return (INT_PTR)hEditBrush;
            
        case WM_CTLCOLORBTN:
            return (INT_PTR)hButtonBrush;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == CLOCK_IDC_BUTTON_OK || LOWORD(wParam) == IDOK) {
                char input[256] = {0};
                GetDlgItemTextA(hwndDlg, CLOCK_IDC_EDIT, input, sizeof(input));
                
                // Parse input time format and convert to seconds array
                char *token, *saveptr;
                char input_copy[256];
                StringCbCopyA(input_copy, sizeof(input_copy), input);
                
                int times[MAX_POMODORO_TIMES] = {0};
                int times_count = 0;
                
                token = strtok_r(input_copy, " ", &saveptr);
                while (token && times_count < MAX_POMODORO_TIMES) {
                    int seconds = 0;
                    if (ParseTimeInput(token, &seconds)) {
                        times[times_count++] = seconds;
                    }
                    token = strtok_r(NULL, " ", &saveptr);
                }
                
                if (times_count > 0) {
                    // Update global variables
                    POMODORO_TIMES_COUNT = times_count;
                    for (int i = 0; i < times_count; i++) {
                        POMODORO_TIMES[i] = times[i];
                    }
                    
                    // Update basic pomodoro times
                    if (times_count > 0) POMODORO_WORK_TIME = times[0];
                    if (times_count > 1) POMODORO_SHORT_BREAK = times[1];
                    if (times_count > 2) POMODORO_LONG_BREAK = times[2];
                    
                    // Write to configuration file
                    WriteConfigPomodoroTimeOptions(times, times_count);
                }
                
                EndDialog(hwndDlg, IDOK);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndPomodoroComboDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Restore original edit control procedure
            {
            HWND hwndEdit = GetDlgItem(hwndDlg, CLOCK_IDC_EDIT);
            SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            
            // Release resources
            if (hBackgroundBrush) {
                DeleteObject(hBackgroundBrush);
                hBackgroundBrush = NULL;
            }
            if (hEditBrush) {
                DeleteObject(hEditBrush);
                hEditBrush = NULL;
            }
            if (hButtonBrush) {
                DeleteObject(hButtonBrush);
                hButtonBrush = NULL;
            }
            }
            break;
    }
    
    return FALSE;
}

// Display Pomodoro combination dialog
void ShowPomodoroComboDialog(HWND hwndParent) {
    if (!g_hwndPomodoroComboDialog) {
        g_hwndPomodoroComboDialog = CreateDialog(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(CLOCK_IDD_POMODORO_COMBO_DIALOG), // Use new dialog resource
            hwndParent,
            PomodoroComboDialogProc
        );
        if (g_hwndPomodoroComboDialog) {
            ShowWindow(g_hwndPomodoroComboDialog, SW_SHOW);
        }
    } else {
        SetForegroundWindow(g_hwndPomodoroComboDialog);
    }
}

// Parse time input (such as "25m", "30s", "1h30m" etc.)
BOOL ParseTimeInput(const char* input, int* seconds) {
    if (!input || !seconds) return FALSE;
    
    *seconds = 0;
    char* buffer = _strdup(input);
    if (!buffer) return FALSE;
    
    int len = strlen(buffer);
    char* pos = buffer;
    int value = 0;
    int tempSeconds = 0;
    
    while (*pos) {
        // Read digits
        if (isdigit((unsigned char)*pos)) {
            value = 0;
            while (isdigit((unsigned char)*pos)) {
                value = value * 10 + (*pos - '0');
                pos++;
            }
            
            // Read units
            if (*pos == 'h' || *pos == 'H') {
                tempSeconds += value * 3600; // Hours to seconds
                pos++;
            } else if (*pos == 'm' || *pos == 'M') {
                tempSeconds += value * 60;   // Minutes to seconds
                pos++;
            } else if (*pos == 's' || *pos == 'S') {
                tempSeconds += value;        // Seconds
                pos++;
            } else if (*pos == '\0') {
                // No unit, default to minutes
                tempSeconds += value * 60;
            } else {
                // Invalid character
                free(buffer);
                return FALSE;
            }
        } else {
            // Non-digit starting character
            pos++;
        }
    }
    
    free(buffer);
    *seconds = tempSeconds;
    return TRUE;
}

// Add global variable to track notification message dialog handle
static HWND g_hwndNotificationMessagesDialog = NULL;

// Add notification messages dialog procedure
INT_PTR CALLBACK NotificationMessagesDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set window topmost
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // Create brushes
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            // Read latest configuration to global variables
            ReadNotificationMessagesConfig();
            
            // For handling UTF-8 Chinese characters, we need to convert to Unicode
            wchar_t wideText[sizeof(CLOCK_TIMEOUT_MESSAGE_TEXT)];
            
            // First edit box - Countdown timeout message
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // Second edit box - Pomodoro timeout message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // Third edit box - Pomodoro cycle completion message
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // Localize label text
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL1, 
                           GetLocalizedString(L"Countdown timeout message:", L"Countdown timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL2, 
                           GetLocalizedString(L"Pomodoro timeout message:", L"Pomodoro timeout message:"));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_LABEL3,
                           GetLocalizedString(L"Pomodoro cycle complete message:", L"Pomodoro cycle complete message:"));
            
            // Localize button text
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(L"OK", L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(L"Cancel", L"Cancel"));
            
            // Subclass edit boxes to support Ctrl+A for select all
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            // Save original window procedure
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Apply the same subclassing process to other edit boxes
            SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Select all text in the first edit box
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_EDIT1, EM_SETSEL, 0, -1);
            
            // Set focus to the first edit box
            SetFocus(GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1));
            
            return FALSE;  // Return FALSE because we manually set focus
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                // Get text from edit boxes (Unicode method)
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // Get Unicode text
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // Convert to UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // Save to configuration file and update global variables
                WriteConfigNotificationMessages(timeout_msg, pomodoro_msg, cycle_complete_msg);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationMessagesDialog = NULL;
                return TRUE;
            }
            break;
            
        case WM_DESTROY:
            // Restore original window procedure
            {
            HWND hEdit1 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT1);
            HWND hEdit2 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT2);
            HWND hEdit3 = GetDlgItem(hwndDlg, IDC_NOTIFICATION_EDIT3);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEdit1, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit2, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEdit3, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display notification message settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays the notification message settings dialog for modifying various notification prompt texts.
 */
void ShowNotificationMessagesDialog(HWND hwndParent) {
    if (!g_hwndNotificationMessagesDialog) {
        // Ensure latest configuration values are read first
        ReadNotificationMessagesConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_MESSAGES_DIALOG), 
                 hwndParent, 
                 NotificationMessagesDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationMessagesDialog);
    }
}

// Add global variable to track notification display settings dialog handle
static HWND g_hwndNotificationDisplayDialog = NULL;

// Add notification display settings dialog procedure
INT_PTR CALLBACK NotificationDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBRUSH hBackgroundBrush = NULL;
    static HBRUSH hEditBrush = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Set window to topmost
            SetWindowPos(hwndDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            
            // Create brushes
            hBackgroundBrush = CreateSolidBrush(RGB(0xF3, 0xF3, 0xF3));
            hEditBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
            
            // Read latest configuration
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            
            // Set current values to edit boxes
            char buffer[32];
            
            // Display time (seconds, support decimal) - convert milliseconds to seconds
            StringCbPrintfA(buffer, sizeof(buffer), "%.1f", (float)NOTIFICATION_TIMEOUT_MS / 1000.0f);
            // Remove trailing .0
            if (strlen(buffer) > 2 && buffer[strlen(buffer)-2] == '.' && buffer[strlen(buffer)-1] == '0') {
                buffer[strlen(buffer)-2] = '\0';
            }
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, buffer);
            
            // Opacity (percentage)
            StringCbPrintfA(buffer, sizeof(buffer), "%d", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, buffer);
            
            // Localize label text
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_LABEL, 
                           GetLocalizedString(L"Notification display time (sec):", L"Notification display time (sec):"));
            
            // Modify edit box style, remove ES_NUMBER to allow decimal point
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            LONG style = GetWindowLong(hEditTime, GWL_STYLE);
            SetWindowLong(hEditTime, GWL_STYLE, style & ~ES_NUMBER);
            
            // Subclass edit boxes to support Enter key submission and input restrictions
            wpOrigEditProc = (WNDPROC)SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            
            // Set focus to time edit box
            SetFocus(hEditTime);
            
            return FALSE;
        }
        
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBackgroundBrush;
        
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wParam, RGB(0xF3, 0xF3, 0xF3));
            return (INT_PTR)hBackgroundBrush;
            
        case WM_CTLCOLOREDIT:
            SetBkColor((HDC)wParam, RGB(0xFF, 0xFF, 0xFF));
            return (INT_PTR)hEditBrush;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                char timeStr[32] = {0};
                char opacityStr[32] = {0};
                
                // Get user input values
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, timeStr, sizeof(timeStr));
                GetDlgItemTextA(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT, opacityStr, sizeof(opacityStr));
                
                // Use more robust method to replace Chinese period
                // First get the text in Unicode format
                wchar_t wTimeStr[32] = {0};
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, wTimeStr, sizeof(wTimeStr)/sizeof(wchar_t));
                
                // Replace Chinese punctuation marks in Unicode text
                for (int i = 0; wTimeStr[i] != L'\0'; i++) {
                    // Recognize various punctuation marks as decimal point
                    if (wTimeStr[i] == L'。' ||  // Chinese period
                        wTimeStr[i] == L'，' ||  // Chinese comma
                        wTimeStr[i] == L',' ||   // English comma
                        wTimeStr[i] == L'·' ||   // Chinese middle dot
                        wTimeStr[i] == L'`' ||   // Backtick
                        wTimeStr[i] == L'：' ||  // Chinese colon
                        wTimeStr[i] == L':' ||   // English colon
                        wTimeStr[i] == L'；' ||  // Chinese semicolon
                        wTimeStr[i] == L';' ||   // English semicolon
                        wTimeStr[i] == L'/' ||   // Forward slash
                        wTimeStr[i] == L'\\' ||  // Backslash
                        wTimeStr[i] == L'~' ||   // Tilde
                        wTimeStr[i] == L'～' ||  // Full-width tilde
                        wTimeStr[i] == L'、' ||  // Chinese enumeration comma
                        wTimeStr[i] == L'．') {  // Full-width period
                        wTimeStr[i] = L'.';      // Replace with English decimal point
                    }
                }
                
                // Convert processed Unicode text back to ASCII
                WideCharToMultiByte(CP_ACP, 0, wTimeStr, -1, 
                                    timeStr, sizeof(timeStr), NULL, NULL);
                
                // Parse time (seconds) and convert to milliseconds
                float timeInSeconds = atof(timeStr);
                int timeInMs = (int)(timeInSeconds * 1000.0f);
                
                // Allow time to be set to 0 (no notification) or at least 100 milliseconds
                if (timeInMs > 0 && timeInMs < 100) timeInMs = 100;
                
                // Parse opacity
                int opacity = atoi(opacityStr);
                
                // Ensure opacity is in range 1-100
                if (opacity < 1) opacity = 1;
                if (opacity > 100) opacity = 100;
                
                // Write to configuration
                WriteConfigNotificationTimeout(timeInMs);
                WriteConfigNotificationOpacity(opacity);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationDisplayDialog = NULL;
                return TRUE;
            }
            break;
            
        // Add handling for WM_CLOSE message
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationDisplayDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // Restore original window procedure
            {
            HWND hEditTime = GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT);
            HWND hEditOpacity = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            
            if (wpOrigEditProc) {
                SetWindowLongPtr(hEditTime, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
                SetWindowLongPtr(hEditOpacity, GWLP_WNDPROC, (LONG_PTR)wpOrigEditProc);
            }
            
            if (hBackgroundBrush) DeleteObject(hBackgroundBrush);
            if (hEditBrush) DeleteObject(hEditBrush);
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display notification display settings dialog
 * @param hwndParent Parent window handle
 * 
 * Displays the notification display settings dialog for modifying notification display time and opacity.
 */
void ShowNotificationDisplayDialog(HWND hwndParent) {
    if (!g_hwndNotificationDisplayDialog) {
        // Ensure latest configuration values are read first
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_DISPLAY_DIALOG), 
                 hwndParent, 
                 NotificationDisplayDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationDisplayDialog);
    }
}

// 添加全局变量来跟踪整合后的通知设置对话框句柄
static HWND g_hwndNotificationSettingsDialog = NULL;

/**
 * @brief Audio playback completion callback function
 * @param hwnd Window handle
 * 
 * When audio playback completes, changes "Stop" button back to "Test" button
 */
static void OnAudioPlaybackComplete(HWND hwnd) {
    if (hwnd && IsWindow(hwnd)) {
        const wchar_t* testText = GetLocalizedString(L"Test", L"Test");
        SetDlgItemTextW(hwnd, IDC_TEST_SOUND_BUTTON, testText);
        
        // Get dialog data
        HWND hwndTestButton = GetDlgItem(hwnd, IDC_TEST_SOUND_BUTTON);
        
        // Send WM_SETTEXT message to update button text
        if (hwndTestButton && IsWindow(hwndTestButton)) {
            SendMessageW(hwndTestButton, WM_SETTEXT, 0, (LPARAM)testText);
        }
        
        // Update global playback state
        if (g_hwndNotificationSettingsDialog == hwnd) {
            // Send message to dialog to notify state change
            SendMessage(hwnd, WM_APP + 100, 0, 0);
        }
    }
}

/**
 * @brief 填充音频下拉框
 * @param hwndDlg 对话框句柄
 */
static void PopulateSoundComboBox(HWND hwndDlg) {
    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
    if (!hwndCombo) return;

    // Clear dropdown list
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    // Add "None" option
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"None", L"None"));
    
    // Add "System Beep" option
    SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)GetLocalizedString(L"System Beep", L"System Beep"));

    // Get audio folder path
    char audio_path[MAX_PATH];
    GetAudioFolderPath(audio_path, MAX_PATH);
    
    // Convert to wide character path
    wchar_t wAudioPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);

    // Build search path
    wchar_t wSearchPath[MAX_PATH];
    StringCbPrintfW(wSearchPath, sizeof(wSearchPath), L"%s\\*.*", wAudioPath);

    // Find audio files - using Unicode version of API
    WIN32_FIND_DATAW find_data;
    HANDLE hFind = FindFirstFileW(wSearchPath, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Check file extension
            wchar_t* ext = wcsrchr(find_data.cFileName, L'.');
            if (ext && (
                _wcsicmp(ext, L".flac") == 0 ||
                _wcsicmp(ext, L".mp3") == 0 ||
                _wcsicmp(ext, L".wav") == 0
            )) {
                // Add Unicode filename directly to dropdown
                SendMessageW(hwndCombo, CB_ADDSTRING, 0, (LPARAM)find_data.cFileName);
            }
        } while (FindNextFileW(hFind, &find_data));
        FindClose(hFind);
    }

    // Set currently selected audio file
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // Check if it's the special system beep marker
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            // Select "System Beep" option (index 1)
            SendMessage(hwndCombo, CB_SETCURSEL, 1, 0);
        } else {
            wchar_t wSoundFile[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, NOTIFICATION_SOUND_FILE, -1, wSoundFile, MAX_PATH);
            
            // Get filename part
            wchar_t* fileName = wcsrchr(wSoundFile, L'\\');
            if (fileName) fileName++;
            else fileName = wSoundFile;
            
            // Find and select the file in dropdown
            int index = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)fileName);
            if (index != CB_ERR) {
                SendMessage(hwndCombo, CB_SETCURSEL, index, 0);
            } else {
                SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // Select "None"
            }
        }
    } else {
        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0); // Select "None"
    }
}

/**
 * @brief 整合后的通知设置对话框处理程序
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 * 
 * 整合了通知内容和通知显示的统一设置界面
 */
INT_PTR CALLBACK NotificationSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static BOOL isPlaying = FALSE; // 添加一个静态变量来跟踪播放状态
    static int originalVolume = 0; // 添加一个静态变量保存原始音量
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 读取最新配置到全局变量
            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            
            // 保存原始音量值，用于取消操作时恢复
            originalVolume = NOTIFICATION_SOUND_VOLUME;
            
            // 应用多语言支持
            ApplyDialogLanguage(hwndDlg, CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG);
            
            // 设置通知消息文本 - 使用Unicode函数
            wchar_t wideText[256];
            
            // 第一个编辑框 - 倒计时超时提示
            MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wideText);
            
            // 第二个编辑框 - 番茄钟超时提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_TIMEOUT_MESSAGE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wideText);
            
            // 第三个编辑框 - 番茄钟循环完成提示
            MultiByteToWideChar(CP_UTF8, 0, POMODORO_CYCLE_COMPLETE_TEXT, -1, 
                               wideText, sizeof(wideText)/sizeof(wchar_t));
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wideText);
            
            // 设置通知显示时间
            SYSTEMTIME st = {0};
            GetLocalTime(&st);
            
            // 读取禁用通知设置
            ReadNotificationDisabledConfig();
            
            // 根据禁用状态设置复选框
            CheckDlgButton(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK, NOTIFICATION_DISABLED ? BST_CHECKED : BST_UNCHECKED);
            
            // 设置时间控件的启用/禁用状态
            EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !NOTIFICATION_DISABLED);
            
            // 设置时间控件的值 - 无论是否禁用都显示实际配置的时间
            int totalSeconds = NOTIFICATION_TIMEOUT_MS / 1000;
            st.wHour = totalSeconds / 3600;
            st.wMinute = (totalSeconds % 3600) / 60;
            st.wSecond = totalSeconds % 60;
            
            // 设置时间控件的初始值
            SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_SETSYSTEMTIME, 
                              GDT_VALID, (LPARAM)&st);

            // 设置通知透明度滑动条
            HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
            SendMessage(hwndOpacitySlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100));
            SendMessage(hwndOpacitySlider, TBM_SETPOS, TRUE, NOTIFICATION_MAX_OPACITY);
            
            // 更新透明度文本
            wchar_t opacityText[16];
            StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", NOTIFICATION_MAX_OPACITY);
            SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
            
            // 设置通知类型单选按钮
            switch (NOTIFICATION_TYPE) {
                case NOTIFICATION_TYPE_CATIME:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_OS:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_OS, BST_CHECKED);
                    break;
                case NOTIFICATION_TYPE_SYSTEM_MODAL:
                    CheckDlgButton(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL, BST_CHECKED);
                    break;
            }
            
            // 填充音频下拉框
            PopulateSoundComboBox(hwndDlg);
            
            // 设置音量滑块
            HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
            SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hwndSlider, TBM_SETPOS, TRUE, NOTIFICATION_SOUND_VOLUME);
            
            // 更新音量文本
            wchar_t volumeText[16];
            StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", NOTIFICATION_SOUND_VOLUME);
            SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
            
            // 在初始化时重置播放状态
            isPlaying = FALSE;
            
            // 设置音频播放完成回调函数
            SetAudioPlaybackCompleteCallback(hwndDlg, OnAudioPlaybackComplete);
            
            // 保存对话框句柄
            g_hwndNotificationSettingsDialog = hwndDlg;
            
            return TRUE;
        }
        
        case WM_HSCROLL: {
            // 处理滑块拖动事件
            if (GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER) == (HWND)lParam) {
                // 获取滑块当前位置
                int volume = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // 更新音量百分比文本
                wchar_t volumeText[16];
                StringCbPrintfW(volumeText, sizeof(volumeText), L"%d%%", volume);
                SetDlgItemTextW(hwndDlg, IDC_VOLUME_TEXT, volumeText);
                
                // 实时应用音量设置
                SetAudioVolume(volume);
                
                return TRUE;
            }
            else if (GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT) == (HWND)lParam) {
                // 获取滑块当前位置
                int opacity = (int)SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
                
                // 更新透明度百分比文本
                wchar_t opacityText[16];
                StringCbPrintfW(opacityText, sizeof(opacityText), L"%d%%", opacity);
                SetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_OPACITY_TEXT, opacityText);
                
                return TRUE;
            }
            break;
        }
        
        case WM_COMMAND:
            // 处理禁用通知复选框状态变化
            if (LOWORD(wParam) == IDC_DISABLE_NOTIFICATION_CHECK && HIWORD(wParam) == BN_CLICKED) {
                BOOL isChecked = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                EnableWindow(GetDlgItem(hwndDlg, IDC_NOTIFICATION_TIME_EDIT), !isChecked);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDOK) {
                // 获取通知消息文本 - 使用Unicode函数
                wchar_t wTimeout[256] = {0};
                wchar_t wPomodoro[256] = {0};
                wchar_t wCycle[256] = {0};
                
                // 获取Unicode文本
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT1, wTimeout, sizeof(wTimeout)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT2, wPomodoro, sizeof(wPomodoro)/sizeof(wchar_t));
                GetDlgItemTextW(hwndDlg, IDC_NOTIFICATION_EDIT3, wCycle, sizeof(wCycle)/sizeof(wchar_t));
                
                // 转换为UTF-8
                char timeout_msg[256] = {0};
                char pomodoro_msg[256] = {0};
                char cycle_complete_msg[256] = {0};
                
                WideCharToMultiByte(CP_UTF8, 0, wTimeout, -1, 
                                    timeout_msg, sizeof(timeout_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wPomodoro, -1, 
                                    pomodoro_msg, sizeof(pomodoro_msg), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, wCycle, -1, 
                                    cycle_complete_msg, sizeof(cycle_complete_msg), NULL, NULL);
                
                // 获取通知显示时间
                SYSTEMTIME st = {0};
                
                // 检查是否勾选了禁用通知复选框
                BOOL isDisabled = (IsDlgButtonChecked(hwndDlg, IDC_DISABLE_NOTIFICATION_CHECK) == BST_CHECKED);
                
                // 保存禁用状态
                NOTIFICATION_DISABLED = isDisabled;
                WriteConfigNotificationDisabled(isDisabled);
                
                // 获取通知时间设置
                if (SendDlgItemMessage(hwndDlg, IDC_NOTIFICATION_TIME_EDIT, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID) {
                    // 计算总秒数: 时*3600 + 分*60 + 秒
                    int totalSeconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    
                    if (totalSeconds == 0) {
                        // 如果时间为00:00:00，设置为0（表示禁用通知）
                        NOTIFICATION_TIMEOUT_MS = 0;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                        
                    } else if (!isDisabled) {
                        // 只有在不禁用的情况下才更新非零的通知时间
                        NOTIFICATION_TIMEOUT_MS = totalSeconds * 1000;
                        WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                    }
                }
                // 如果禁用通知，则不修改通知时间配置
                
                // 获取通知透明度（从滑动条获取）
                HWND hwndOpacitySlider = GetDlgItem(hwndDlg, IDC_NOTIFICATION_OPACITY_EDIT);
                int opacity = (int)SendMessage(hwndOpacitySlider, TBM_GETPOS, 0, 0);
                if (opacity >= 1 && opacity <= 100) {
                    NOTIFICATION_MAX_OPACITY = opacity;
                }
                
                // 获取通知类型
                if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_CATIME)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_CATIME;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_OS)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_OS;
                } else if (IsDlgButtonChecked(hwndDlg, IDC_NOTIFICATION_TYPE_SYSTEM_MODAL)) {
                    NOTIFICATION_TYPE = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
                
                // 获取选中的音频文件
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                if (index > 0) { // 0是"无"选项
                    wchar_t wFileName[MAX_PATH];
                    SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                    
                    // 检查是否选择了"系统提示音"
                    const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                    if (wcscmp(wFileName, sysBeepText) == 0) {
                        // 使用特殊标记来表示系统提示音
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                    } else {
                        // 获取音频文件夹路径
                        char audio_path[MAX_PATH];
                        GetAudioFolderPath(audio_path, MAX_PATH);
                        
                        // 转换为UTF-8路径
                        char fileName[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                        
                        // 构建完整的文件路径
                        memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                        StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                    }
                } else {
                    NOTIFICATION_SOUND_FILE[0] = '\0';
                }
                
                // 获取音量滑块位置
                HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                NOTIFICATION_SOUND_VOLUME = volume;
                
                // 保存所有设置
                WriteConfigNotificationMessages(
                    timeout_msg,
                    pomodoro_msg,
                    cycle_complete_msg
                );
                WriteConfigNotificationTimeout(NOTIFICATION_TIMEOUT_MS);
                WriteConfigNotificationOpacity(NOTIFICATION_MAX_OPACITY);
                WriteConfigNotificationType(NOTIFICATION_TYPE);
                WriteConfigNotificationSound(NOTIFICATION_SOUND_FILE);
                WriteConfigNotificationVolume(NOTIFICATION_SOUND_VOLUME);
                
                // 确保停止正在播放的音频
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // 关闭对话框前清理回调
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDOK);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                // 确保停止正在播放的音频
                if (isPlaying) {
                    StopNotificationSound();
                    isPlaying = FALSE;
                }
                
                // 恢复原始音量设置
                NOTIFICATION_SOUND_VOLUME = originalVolume;
                
                // 重新应用原始音量
                SetAudioVolume(originalVolume);
                
                // 关闭对话框前清理回调
                SetAudioPlaybackCompleteCallback(NULL, NULL);
                
                EndDialog(hwndDlg, IDCANCEL);
                g_hwndNotificationSettingsDialog = NULL;
                return TRUE;
            } else if (LOWORD(wParam) == IDC_TEST_SOUND_BUTTON) {
                if (!isPlaying) {
                    // 当前不在播放，开始播放并更改按钮文本为"结束"
                    HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                    int index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                    
                    if (index > 0) { // 0是"无"选项
                        // 获取当前滑块音量并应用
                        HWND hwndSlider = GetDlgItem(hwndDlg, IDC_VOLUME_SLIDER);
                        int volume = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                        SetAudioVolume(volume);
                        
                        wchar_t wFileName[MAX_PATH];
                        SendMessageW(hwndCombo, CB_GETLBTEXT, index, (LPARAM)wFileName);
                        
                        // 临时保存当前音频设置
                        char tempSoundFile[MAX_PATH];
                        StringCbCopyA(tempSoundFile, sizeof(tempSoundFile), NOTIFICATION_SOUND_FILE);
                        
                        // 临时设置音频文件
                        const wchar_t* sysBeepText = GetLocalizedString(L"System Beep", L"System Beep");
                        if (wcscmp(wFileName, sysBeepText) == 0) {
                            // 使用特殊标记
                            StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), "SYSTEM_BEEP");
                        } else {
                            // 获取音频文件夹路径
                            char audio_path[MAX_PATH];
                            GetAudioFolderPath(audio_path, MAX_PATH);
                            
                            // 转换为UTF-8路径
                            char fileName[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, wFileName, -1, fileName, MAX_PATH, NULL, NULL);
                            
                            // 构建完整的文件路径
                            memset(NOTIFICATION_SOUND_FILE, 0, MAX_PATH);
                            StringCbPrintfA(NOTIFICATION_SOUND_FILE, MAX_PATH, "%s\\%s", audio_path, fileName);
                        }
                        
                        // 播放音频
                        if (PlayNotificationSound(hwndDlg)) {
                            // 播放成功，更改按钮文本为"结束"
                            SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Stop", L"Stop"));
                            isPlaying = TRUE;
                        }
                        
                        // 恢复之前的设置
                        StringCbCopyA(NOTIFICATION_SOUND_FILE, sizeof(NOTIFICATION_SOUND_FILE), tempSoundFile);
                    }
                } else {
                    // 当前正在播放，停止播放并恢复按钮文本
                    StopNotificationSound();
                    SetDlgItemTextW(hwndDlg, IDC_TEST_SOUND_BUTTON, GetLocalizedString(L"Test", L"Test"));
                    isPlaying = FALSE;
                }
                return TRUE;
            } else if (LOWORD(wParam) == IDC_OPEN_SOUND_DIR_BUTTON) {
                // 获取音频目录路径
                char audio_path[MAX_PATH];
                GetAudioFolderPath(audio_path, MAX_PATH);
                
                // 确保目录存在
                wchar_t wAudioPath[MAX_PATH];
                MultiByteToWideChar(CP_UTF8, 0, audio_path, -1, wAudioPath, MAX_PATH);
                
                // 打开目录
                ShellExecuteW(hwndDlg, L"open", wAudioPath, NULL, NULL, SW_SHOWNORMAL);
                
                // 记录当前选择的音频文件
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // 重新填充音频下拉框
                PopulateSoundComboBox(hwndDlg);
                
                // 尝试恢复之前的选择
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    } else {
                        // 如果找不到之前的选择，默认选择"无"
                        SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
                    }
                }
                
                return TRUE;
            } else if (LOWORD(wParam) == IDC_NOTIFICATION_SOUND_COMBO && HIWORD(wParam) == CBN_DROPDOWN) {
                // 下拉列表将要打开时，重新加载文件列表
                HWND hwndCombo = GetDlgItem(hwndDlg, IDC_NOTIFICATION_SOUND_COMBO);
                
                // 记录当前选择的文件
                int selectedIndex = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);
                wchar_t selectedFile[MAX_PATH] = {0};
                if (selectedIndex > 0) {
                    SendMessageW(hwndCombo, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedFile);
                }
                
                // 重新填充下拉框
                PopulateSoundComboBox(hwndDlg);
                
                // 恢复之前的选择
                if (selectedFile[0] != L'\0') {
                    int newIndex = SendMessageW(hwndCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)selectedFile);
                    if (newIndex != CB_ERR) {
                        SendMessage(hwndCombo, CB_SETCURSEL, newIndex, 0);
                    }
                }
                
                return TRUE;
            }
            break;
            
        // 添加自定义消息处理，用于音频播放完成通知
        case WM_APP + 100:
            // 音频播放已完成，更新按钮状态
            isPlaying = FALSE;
            return TRUE;
            
        case WM_CLOSE:
            // 关闭对话框时确保停止播放
            if (isPlaying) {
                StopNotificationSound();
            }
            
            // 清理回调
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            
            EndDialog(hwndDlg, IDCANCEL);
            g_hwndNotificationSettingsDialog = NULL;
            return TRUE;
            
        case WM_DESTROY:
            // 对话框销毁时清理回调
            SetAudioPlaybackCompleteCallback(NULL, NULL);
            g_hwndNotificationSettingsDialog = NULL;
            break;
    }
    return FALSE;
}

/**
 * @brief 显示整合后的通知设置对话框
 * @param hwndParent 父窗口句柄
 * 
 * 显示同时包含通知内容和通知显示设置的整合对话框
 */
void ShowNotificationSettingsDialog(HWND hwndParent) {
    if (!g_hwndNotificationSettingsDialog) {
        // 确保首先读取最新的配置值
        ReadNotificationMessagesConfig();
        ReadNotificationTimeoutConfig();
        ReadNotificationOpacityConfig();
        ReadNotificationTypeConfig();
        ReadNotificationSoundConfig();
        ReadNotificationVolumeConfig();
        
        DialogBox(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(CLOCK_IDD_NOTIFICATION_SETTINGS_DIALOG), 
                 hwndParent, 
                 NotificationSettingsDlgProc);
    } else {
        SetForegroundWindow(g_hwndNotificationSettingsDialog);
    }
}