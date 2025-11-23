#include "dialog/dialog_monitor.h"
#include "monitor/monitor_types.h"
#include "monitor/monitor_core.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <stdio.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>

static int s_currentSel = -1;

static void TrimW(wchar_t* str) {
    if (!str) return;
    
    // Trim trailing spaces
    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) {
        str[--len] = L'\0';
    }
    
    // Trim leading spaces
    wchar_t* start = str;
    while (*start && iswspace(*start)) {
        start++;
    }
    
    if (start > str) {
        wmemmove(str, start, (wcslen(start) + 1) * sizeof(wchar_t));
    }
}

static void RefreshList(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_MONITOR_LIST);
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    
    int count = Monitor_GetConfigCount();
    for (int i = 0; i < count; i++) {
        MonitorConfig cfg;
        if (Monitor_GetConfigAt(i, &cfg)) {
            wchar_t wSource[256];
            MultiByteToWideChar(CP_UTF8, 0, cfg.sourceString, -1, wSource, 256);
            int idx = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wSource);
            SendMessage(hList, LB_SETITEMDATA, idx, i);
        }
    }
    
    // Restore selection if valid
    // If active index matches an item, maybe highlight it differently? 
    // For now just standard selection logic.
    // If s_currentSel is valid, select it
    
    if (s_currentSel >= 0 && s_currentSel < count) {
        // Find which list item has this data index
        int lbCount = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
        for (int i = 0; i < lbCount; i++) {
            if ((int)SendMessage(hList, LB_GETITEMDATA, i, 0) == s_currentSel) {
                SendMessage(hList, LB_SETCURSEL, i, 0);
                break;
            }
        }
    }
}

static void UpdateEditFields(HWND hDlg, int index) {
    MonitorConfig cfg;
    if (index >= 0 && Monitor_GetConfigAt(index, &cfg)) {
        SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, cfg.label);
        
        wchar_t wSource[256];
        MultiByteToWideChar(CP_UTF8, 0, cfg.sourceString, -1, wSource, 256);
        SetDlgItemTextW(hDlg, IDC_MONITOR_SOURCE_EDIT, wSource);
        
        wchar_t wToken[128];
        MultiByteToWideChar(CP_UTF8, 0, cfg.token, -1, wToken, 128);
        SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, wToken);
    } else {
        // Clear fields
        SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_SOURCE_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, L"");
    }
}

static INT_PTR CALLBACK MonitorDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            // Center the dialog
            RECT rc, rcOwner;
            GetWindowRect(hDlg, &rc);
            
            // Get the monitor that contains the owner window or primary monitor
            HMONITOR hMonitor = MonitorFromWindow(GetParent(hDlg), MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(MONITORINFO)};
            GetMonitorInfo(hMonitor, &mi);
            
            rcOwner = mi.rcWork;
            
            int dlgWidth = rc.right - rc.left;
            int dlgHeight = rc.bottom - rc.top;
            int ownerWidth = rcOwner.right - rcOwner.left;
            int ownerHeight = rcOwner.bottom - rcOwner.top;
            
            int x = rcOwner.left + (ownerWidth - dlgWidth) / 2;
            int y = rcOwner.top + (ownerHeight - dlgHeight) / 2;
            
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

            s_currentSel = Monitor_GetActiveIndex();
            RefreshList(hDlg);
            UpdateEditFields(hDlg, s_currentSel);
            return TRUE;
        }
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_MONITOR_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        HWND hList = GetDlgItem(hDlg, IDC_MONITOR_LIST);
                        int lbIdx = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                        if (lbIdx != LB_ERR) {
                            s_currentSel = (int)SendMessage(hList, LB_GETITEMDATA, lbIdx, 0);
                            UpdateEditFields(hDlg, s_currentSel);
                        }
                    }
                    break;
                    
                case IDC_MONITOR_ADD_BTN:
                    s_currentSel = -1; // New item mode
                    // Deselect list
                    SendMessage(GetDlgItem(hDlg, IDC_MONITOR_LIST), LB_SETCURSEL, -1, 0);
                    UpdateEditFields(hDlg, -1);
                    SetFocus(GetDlgItem(hDlg, IDC_MONITOR_LABEL_EDIT));
                    break;
                    
                case IDC_MONITOR_DEL_BTN:
                    if (s_currentSel >= 0) {
                        Monitor_DeleteConfigAt(s_currentSel);
                        s_currentSel = -1;
                        RefreshList(hDlg);
                        UpdateEditFields(hDlg, -1);
                    }
                    break;
                    
                case IDC_MONITOR_SAVE_BTN: {
                    MonitorConfig cfg = {0};
                    
                    // 1. Get and trim Source String first (needed for Label fallback)
                    wchar_t wSource[256];
                    GetDlgItemTextW(hDlg, IDC_MONITOR_SOURCE_EDIT, wSource, 256);
                    TrimW(wSource);
                    WideCharToMultiByte(CP_UTF8, 0, wSource, -1, cfg.sourceString, 256, NULL, NULL);
                    
                    // 2. Get and trim Token
                    wchar_t wToken[128];
                    GetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, wToken, 128);
                    TrimW(wToken);
                    WideCharToMultiByte(CP_UTF8, 0, wToken, -1, cfg.token, 128, NULL, NULL);

                    // 3. Get and trim Label
                    wchar_t wLabel[32];
                    GetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, wLabel, 32);
                    TrimW(wLabel);
                    
                    // 4. Handle empty Label: fallback to Source String (truncated to 31 chars)
                    if (wcslen(wLabel) == 0) {
                        wcsncpy(cfg.label, wSource, 31);
                        cfg.label[31] = L'\0';
                    } else {
                        wcscpy(cfg.label, wLabel);
                    }
                    
                    // Check if we have at least a source string
                    if (strlen(cfg.sourceString) == 0) {
                        MessageBoxW(hDlg, L"Please enter a Source String.", L"Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    cfg.refreshInterval = 300; // Default 5 mins
                    cfg.enabled = TRUE;
                    
                    if (s_currentSel >= 0) {
                        Monitor_UpdateConfigAt(s_currentSel, &cfg);
                    } else {
                        Monitor_AddConfig(&cfg);
                        s_currentSel = Monitor_GetConfigCount() - 1;
                    }
                    RefreshList(hDlg);
                    // Re-select current
                    UpdateEditFields(hDlg, s_currentSel); // Just to be sure
                    break;
                }
                    
                case IDC_MONITOR_APPLY_BTN:
                    if (s_currentSel >= 0) {
                        Monitor_SetActiveIndex(s_currentSel);
                    }
                    EndDialog(hDlg, IDOK);
                    break;
                    
                case IDCANCEL:
                    EndDialog(hDlg, IDCANCEL);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

void ShowMonitorConfigDialog(HWND hParent) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MONITOR_CONFIG_DIALOG), hParent, MonitorDialogProc);
}
