#include <windows.h>
#include <shellapi.h>
#include "../include/tray_events.h"
#include "../include/tray_menu.h"
#include "../include/color.h"
#include "../include/timer.h"
#include "../include/language.h"
#include "../include/window_events.h"
#include "../resource/resource.h"


extern void ReadTimeoutActionFromConfig(void);

void HandleTrayIconMessage(HWND hwnd, UINT uID, UINT uMouseMsg) {

    SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(IDC_ARROW)));
    
    if (uMouseMsg == WM_RBUTTONUP) {
        ShowColorMenu(hwnd);
    }
    else if (uMouseMsg == WM_LBUTTONUP) {
        ShowContextMenu(hwnd);
    }
}

void PauseResumeTimer(HWND hwnd) {

    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        

        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {

            CLOCK_LAST_TIME_UPDATE = time(NULL);

            KillTimer(hwnd, 1);
            

            extern BOOL PauseNotificationSound(void);
            PauseNotificationSound();
        } else {

            SetTimer(hwnd, 1, 1000, NULL);
            

            extern BOOL ResumeNotificationSound(void);
            ResumeNotificationSound();
        }
        

        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void RestartTimer(HWND hwnd) {

    extern void StopNotificationSound(void);
    StopNotificationSound();
    

    if (!CLOCK_COUNT_UP) {

        if (CLOCK_TOTAL_TIME > 0) {
            countdown_elapsed_time = 0;
            countdown_message_shown = FALSE;
            CLOCK_IS_PAUSED = FALSE;
            KillTimer(hwnd, 1);
            SetTimer(hwnd, 1, 1000, NULL);
        }
    } else {

        countup_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        KillTimer(hwnd, 1);
        SetTimer(hwnd, 1, 1000, NULL);
    }
    

    InvalidateRect(hwnd, NULL, TRUE);
    

    HandleWindowReset(hwnd);
}

void SetStartupMode(HWND hwnd, const char* mode) {

    WriteConfigStartupMode(mode);
    

    HMENU hMenu = GetMenu(hwnd);
    if (hMenu) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void OpenUserGuide(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/guide", NULL, NULL, SW_SHOWNORMAL);
}

void OpenSupportPage(void) {
    ShellExecuteW(NULL, L"open", L"https://vladelaina.github.io/Catime/support", NULL, NULL, SW_SHOWNORMAL);
}

void OpenFeedbackPage(void) {
    extern AppLanguage CURRENT_LANGUAGE;
    

    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP) {

        ShellExecuteW(NULL, L"open", URL_FEEDBACK, NULL, NULL, SW_SHOWNORMAL);
    } else {

        ShellExecuteW(NULL, L"open", L"https://github.com/vladelaina/Catime/issues", NULL, NULL, SW_SHOWNORMAL);
    }
}