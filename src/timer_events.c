/**
 * @file timer_events.c
 * @brief Implementation of timer event handling
 * 
 * This file implements the functionality related to the application's timer event handling,
 * including countdown and count-up mode event processing.
 */

#include <windows.h>
#include <stdlib.h>
#include "../include/timer_events.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/notification.h"
#include "../include/pomodoro.h"
#include "../include/config.h"
#include <stdio.h>
#include <string.h>
#include "../include/window.h"
#include "audio_player.h"  // Include header reference

// Maximum capacity of Pomodoro time list
#define MAX_POMODORO_TIMES 10
extern int POMODORO_TIMES[MAX_POMODORO_TIMES]; // Store all Pomodoro times
extern int POMODORO_TIMES_COUNT;              // Actual number of Pomodoro times

// Index of the currently executing Pomodoro time
int current_pomodoro_time_index = 0;

// Define current_pomodoro_phase variable, which is declared as extern in pomodoro.h
POMODORO_PHASE current_pomodoro_phase = POMODORO_PHASE_IDLE;

// Number of completed Pomodoro cycles
int complete_pomodoro_cycles = 0;

// Function declarations imported from main.c
extern void ShowNotification(HWND hwnd, const char* message);

// Variable declarations imported from main.c, for timeout actions
extern int elapsed_time;
extern BOOL message_shown;

// Custom message text imported from config.c
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100]; // New Pomodoro-specific prompt
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

// Define ClockState type
typedef enum {
    CLOCK_STATE_IDLE,
    CLOCK_STATE_COUNTDOWN,
    CLOCK_STATE_COUNTUP,
    CLOCK_STATE_POMODORO
} ClockState;

// Define PomodoroState type
typedef struct {
    BOOL isLastCycle;
    int cycleIndex;
    int totalCycles;
} PomodoroState;

extern HWND g_hwnd; // Main window handle
extern ClockState g_clockState;
extern PomodoroState g_pomodoroState;

// Timer behavior function declarations
extern void ShowTrayNotification(HWND hwnd, const char* message);
extern void ShowNotification(HWND hwnd, const char* message);
extern void OpenFileByPath(const char* filePath);
extern void OpenWebsite(const char* url);
extern void SleepComputer(void);
extern void ShutdownComputer(void);
extern void RestartComputer(void);
extern void SetTimeDisplay(void);
extern void ShowCountUp(HWND hwnd);

// Add external function declaration to the beginning of the file or before the function
extern void StopNotificationSound(void);

/**
 * @brief Convert UTF-8 encoded char* string to wchar_t* string
 * @param utf8String Input UTF-8 string
 * @return Converted wchar_t* string, memory needs to be freed with free() after use. Returns NULL if conversion fails.
 */
static wchar_t* Utf8ToWideChar(const char* utf8String) {
    if (!utf8String || utf8String[0] == '\0') {
        return NULL; // Return NULL to handle empty strings or NULL pointers
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, NULL, 0);
    if (size_needed == 0) {
        // Conversion failed
        return NULL;
    }
    wchar_t* wideString = (wchar_t*)malloc(size_needed * sizeof(wchar_t));
    if (!wideString) {
        // Memory allocation failed
        return NULL;
    }
    int result = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideString, size_needed);
    if (result == 0) {
        // Conversion failed
        free(wideString);
        return NULL;
    }
    return wideString;
}

/**
 * @brief Convert wide character string to UTF-8 encoded regular string and display notification
 * @param hwnd Window handle
 * @param message Wide character string message to display (read from configuration and converted)
 */
static void ShowLocalizedNotification(HWND hwnd, const wchar_t* message) {
    // Don't display if message is empty
    if (!message || message[0] == L'\0') {
        return;
    }

    // Calculate required buffer size
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, message, -1, NULL, 0, NULL, NULL);
    if (size_needed == 0) return; // Conversion failed

    // Allocate memory
    char* utf8Msg = (char*)malloc(size_needed);
    if (utf8Msg) {
        // Convert to UTF-8
        int result = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8Msg, size_needed, NULL, NULL);

        if (result > 0) {
            // Display notification using the new ShowNotification function
            ShowNotification(hwnd, utf8Msg);
            
            // If timeout action is MESSAGE, play notification audio
            if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_MESSAGE) {
                // Read the latest audio settings
                ReadNotificationSoundConfig();
                
                // Play notification audio
                PlayNotificationSound(hwnd);
            }
        }

        // Free memory
        free(utf8Msg);
    }
}

/**
 * @brief Set Pomodoro to work phase
 * 
 * Reset all timer counts and set Pomodoro to work phase
 */
void InitializePomodoro(void) {
    // Use existing enum value POMODORO_PHASE_WORK instead of POMODORO_PHASE_RUNNING
    current_pomodoro_phase = POMODORO_PHASE_WORK;
    current_pomodoro_time_index = 0;
    complete_pomodoro_cycles = 0;
    
    // Set initial countdown to the first time value
    if (POMODORO_TIMES_COUNT > 0) {
        CLOCK_TOTAL_TIME = POMODORO_TIMES[0];
    } else {
        // If no time is configured, use default 25 minutes
        CLOCK_TOTAL_TIME = 1500;
    }
    
    countdown_elapsed_time = 0;
    countdown_message_shown = FALSE;
}

/**
 * @brief Handle timer messages
 * @param hwnd Window handle
 * @param wp Message parameter
 * @return BOOL Whether the message was handled
 */
BOOL HandleTimerEvent(HWND hwnd, WPARAM wp) {
    if (wp == 1) {
        if (CLOCK_SHOW_CURRENT_TIME) {
            // In current time display mode, reset the last displayed second on each timer trigger to ensure the latest time is displayed
            extern int last_displayed_second;
            last_displayed_second = -1; // Force reset of seconds cache to ensure the latest system time is displayed each time
            
            // Refresh display
            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }

        // If timer is paused, don't update time
        if (CLOCK_IS_PAUSED) {
            return TRUE;
        }

        if (CLOCK_COUNT_UP) {
            countup_elapsed_time++;
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            if (countdown_elapsed_time < CLOCK_TOTAL_TIME) {
                countdown_elapsed_time++;
                if (countdown_elapsed_time >= CLOCK_TOTAL_TIME && !countdown_message_shown) {
                    countdown_message_shown = TRUE;

                    // Re-read message text from config file before displaying notification
                    ReadNotificationMessagesConfig();
                    // Force re-read notification type config to ensure latest settings are used
                    ReadNotificationTypeConfig();
                    
                    // Variable declaration before branches to ensure availability in all branches
                    wchar_t* timeoutMsgW = NULL;

                    // Check if in Pomodoro mode - must meet all three conditions:
                    // 1. Current Pomodoro phase is not IDLE 
                    // 2. Pomodoro time configuration is valid
                    // 3. Current countdown total time matches the time at current index in the Pomodoro time list
                    if (current_pomodoro_phase != POMODORO_PHASE_IDLE && 
                        POMODORO_TIMES_COUNT > 0 && 
                        current_pomodoro_time_index < POMODORO_TIMES_COUNT &&
                        CLOCK_TOTAL_TIME == POMODORO_TIMES[current_pomodoro_time_index]) {
                        
                        // Use Pomodoro-specific prompt message
                        timeoutMsgW = Utf8ToWideChar(POMODORO_TIMEOUT_MESSAGE_TEXT);
                        
                        // Display timeout message (using config or default value)
                        if (timeoutMsgW) {
                            ShowLocalizedNotification(hwnd, timeoutMsgW);
                        } else {
                            ShowLocalizedNotification(hwnd, L"番茄钟时间到！"); // Fallback
                        }
                        
                        // Move to next time period
                        current_pomodoro_time_index++;
                        
                        // Check if a complete cycle has been finished
                        if (current_pomodoro_time_index >= POMODORO_TIMES_COUNT) {
                            // Reset index back to the first time
                            current_pomodoro_time_index = 0;
                            
                            // Increase completed cycle count
                            complete_pomodoro_cycles++;
                            
                            // Check if all configured loop counts have been completed
                            if (complete_pomodoro_cycles >= POMODORO_LOOP_COUNT) {
                                // All loop counts completed, end Pomodoro
                                countdown_elapsed_time = 0;
                                countdown_message_shown = FALSE;
                                CLOCK_TOTAL_TIME = 0;
                                
                                // Reset Pomodoro state
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                
                                // Try to read and convert completion message from config
                                wchar_t* cycleCompleteMsgW = Utf8ToWideChar(POMODORO_CYCLE_COMPLETE_TEXT);
                                // Display completion prompt (using config or default value)
                                if (cycleCompleteMsgW) {
                                    ShowLocalizedNotification(hwnd, cycleCompleteMsgW);
                                    free(cycleCompleteMsgW); // Free completion message memory
                                } else {
                                    ShowLocalizedNotification(hwnd, L"所有番茄钟循环完成！"); // Fallback
                                }
                                
                                // Switch to idle state - add the following code
                                CLOCK_COUNT_UP = FALSE;       // Ensure not in count-up mode
                                CLOCK_SHOW_CURRENT_TIME = FALSE; // Ensure not in current time display mode
                                message_shown = TRUE;         // Mark message as shown
                                
                                // Force redraw window to clear display
                                InvalidateRect(hwnd, NULL, TRUE);
                                KillTimer(hwnd, 1);
                                if (timeoutMsgW) free(timeoutMsgW); // Free timeout message memory
                                return TRUE;
                            }
                        }
                        
                        // Set countdown for next time period
                        CLOCK_TOTAL_TIME = POMODORO_TIMES[current_pomodoro_time_index];
                        countdown_elapsed_time = 0;
                        countdown_message_shown = FALSE;
                        
                        // If it's the first time period in a new round, display cycle prompt
                        if (current_pomodoro_time_index == 0 && complete_pomodoro_cycles > 0) {
                            wchar_t cycleMsg[100];
                            // GetLocalizedString needs to be reconsidered, or hardcode English/Chinese
                            // Temporarily keep the original approach, but ideally should be configurable
                            const wchar_t* formatStr = GetLocalizedString(L"开始第 %d 轮番茄钟", L"Starting Pomodoro cycle %d");
                            swprintf(cycleMsg, 100, formatStr, complete_pomodoro_cycles + 1);
                            ShowLocalizedNotification(hwnd, cycleMsg); // Call the modified function
                        }
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    } else {
                        // Not in Pomodoro mode, or switched to normal countdown mode
                        // Use normal countdown prompt message
                        timeoutMsgW = Utf8ToWideChar(CLOCK_TIMEOUT_MESSAGE_TEXT);
                        
                        // Only display notification message if timeout action is not open file, lock, shutdown, or restart
                        if (CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_OPEN_FILE && 
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_LOCK &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SHUTDOWN &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_RESTART &&
                            CLOCK_TIMEOUT_ACTION != TIMEOUT_ACTION_SLEEP) {
                            // Display timeout message (using config or default value)
                            if (timeoutMsgW) {
                                ShowLocalizedNotification(hwnd, timeoutMsgW);
                            } else {
                                ShowLocalizedNotification(hwnd, L"时间到！"); // Fallback
                            }
                        }
                        
                        // If current mode is not Pomodoro (manually switched to normal countdown), ensure not to return to Pomodoro cycle
                        if (current_pomodoro_phase != POMODORO_PHASE_IDLE &&
                            (current_pomodoro_time_index >= POMODORO_TIMES_COUNT ||
                             CLOCK_TOTAL_TIME != POMODORO_TIMES[current_pomodoro_time_index])) {
                            // If switched to normal countdown, reset Pomodoro state
                            current_pomodoro_phase = POMODORO_PHASE_IDLE;
                            current_pomodoro_time_index = 0;
                            complete_pomodoro_cycles = 0;
                        }
                        
                        // If sleep option, process immediately, skip other processing logic
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SLEEP) {
                            // Reset display and apply changes
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // Stop timer
                            KillTimer(hwnd, 1);
                            
                            // Immediately force redraw window to clear display
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // Free memory
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // Execute sleep command
                            system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
                            return TRUE;
                        }
                        
                        // If shutdown option, process immediately, skip other processing logic
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_SHUTDOWN) {
                            // Reset display and apply changes
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // Stop timer
                            KillTimer(hwnd, 1);
                            
                            // Immediately force redraw window to clear display
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // Free memory
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // Execute shutdown command
                            system("shutdown /s /t 0");
                            return TRUE;
                        }
                        
                        // If restart option, process immediately, skip other processing logic
                        if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_RESTART) {
                            // Reset display and apply changes
                            CLOCK_TOTAL_TIME = 0;
                            countdown_elapsed_time = 0;
                            
                            // Stop timer
                            KillTimer(hwnd, 1);
                            
                            // Immediately force redraw window to clear display
                            InvalidateRect(hwnd, NULL, TRUE);
                            UpdateWindow(hwnd);
                            
                            // Free memory
                            if (timeoutMsgW) {
                                free(timeoutMsgW);
                            }
                            
                            // Execute restart command
                            system("shutdown /r /t 0");
                            return TRUE;
                        }
                        
                        switch (CLOCK_TIMEOUT_ACTION) {
                            case TIMEOUT_ACTION_MESSAGE:
                                // Notification already displayed, no additional operation needed
                                break;
                            case TIMEOUT_ACTION_LOCK:
                                LockWorkStation();
                                break;
                            case TIMEOUT_ACTION_OPEN_FILE: {
                                if (strlen(CLOCK_TIMEOUT_FILE_PATH) > 0) {
                                    wchar_t wPath[MAX_PATH];
                                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wPath, MAX_PATH);
                                    
                                    HINSTANCE result = ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
                                    
                                    if ((INT_PTR)result <= 32) {
                                        MessageBoxW(hwnd, 
                                            GetLocalizedString(L"无法打开文件", L"Failed to open file"),
                                            GetLocalizedString(L"错误", L"Error"),
                                            MB_ICONERROR);
                                    }
                                }
                                break;
                            }
                            case TIMEOUT_ACTION_SHOW_TIME:
                                // Stop any playing notification audio
                                StopNotificationSound();
                                
                                // Switch to current time display mode
                                CLOCK_SHOW_CURRENT_TIME = TRUE;
                                CLOCK_COUNT_UP = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_COUNT_UP:
                                // Stop any playing notification audio
                                StopNotificationSound();
                                
                                // Switch to count-up mode and reset
                                CLOCK_COUNT_UP = TRUE;
                                CLOCK_SHOW_CURRENT_TIME = FALSE;
                                countup_elapsed_time = 0;
                                elapsed_time = 0;
                                message_shown = FALSE;
                                countdown_message_shown = FALSE;
                                countup_message_shown = FALSE;
                                
                                // Set Pomodoro state to idle
                                CLOCK_IS_PAUSED = FALSE;
                                KillTimer(hwnd, 1);
                                SetTimer(hwnd, 1, 1000, NULL);
                                InvalidateRect(hwnd, NULL, TRUE);
                                break;
                            case TIMEOUT_ACTION_OPEN_WEBSITE:
                                if (strlen(CLOCK_TIMEOUT_WEBSITE_URL) > 0) {
                                    wchar_t wideUrl[MAX_PATH];
                                    MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_WEBSITE_URL, -1, wideUrl, MAX_PATH);
                                    ShellExecuteW(NULL, L"open", wideUrl, NULL, NULL, SW_NORMAL);
                                }
                                break;
                        }
                    }

                    // Free converted wide string memory
                    if (timeoutMsgW) {
                        free(timeoutMsgW);
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return TRUE;
    }
    return FALSE;
}

void OnTimerTimeout(HWND hwnd) {
    // Execute different behaviors based on timeout action
    switch (CLOCK_TIMEOUT_ACTION) {
        case TIMEOUT_ACTION_MESSAGE: {
            char utf8Msg[256] = {0};
            
            // Select different prompt messages based on current state
            if (g_clockState == CLOCK_STATE_POMODORO) {
                // Check if Pomodoro has completed all cycles
                if (g_pomodoroState.isLastCycle && g_pomodoroState.cycleIndex >= g_pomodoroState.totalCycles - 1) {
                    strncpy(utf8Msg, POMODORO_CYCLE_COMPLETE_TEXT, sizeof(utf8Msg) - 1);
                } else {
                    strncpy(utf8Msg, POMODORO_TIMEOUT_MESSAGE_TEXT, sizeof(utf8Msg) - 1);
                }
            } else {
                strncpy(utf8Msg, CLOCK_TIMEOUT_MESSAGE_TEXT, sizeof(utf8Msg) - 1);
            }
            
            utf8Msg[sizeof(utf8Msg) - 1] = '\0'; // Ensure string ends with null character
            
            // Display custom prompt message
            ShowNotification(hwnd, utf8Msg);
            
            // Read latest audio settings and play alert sound
            ReadNotificationSoundConfig();
            PlayNotificationSound(hwnd);
            
            break;
        }

    }
}

// Add missing global variable definitions (if not defined elsewhere)
#ifndef STUB_VARIABLES_DEFINED
#define STUB_VARIABLES_DEFINED
// Main window handle
HWND g_hwnd = NULL;
// Current clock state
ClockState g_clockState = CLOCK_STATE_IDLE;
// Pomodoro state
PomodoroState g_pomodoroState = {FALSE, 0, 1};
#endif

// Add stub function definitions if needed
#ifndef STUB_FUNCTIONS_DEFINED
#define STUB_FUNCTIONS_DEFINED
__attribute__((weak)) void SleepComputer(void) {
    // This is a weak symbol definition, if there's an actual implementation elsewhere, that implementation will be used
    system("rundll32.exe powrprof.dll,SetSuspendState 0,1,0");
}

__attribute__((weak)) void ShutdownComputer(void) {
    system("shutdown /s /t 0");
}

__attribute__((weak)) void RestartComputer(void) {
    system("shutdown /r /t 0");
}

__attribute__((weak)) void SetTimeDisplay(void) {
    // Stub implementation for setting time display
}

__attribute__((weak)) void ShowCountUp(HWND hwnd) {
    // Stub implementation for showing count-up
}
#endif
