#include "dialog/dialog_monitor.h"
#include "monitor/monitor_types.h"
#include "monitor/monitor_core.h"
#include "monitor/platforms/github.h"
#include "../resource/resource.h"
#include <commctrl.h>
#include <windowsx.h>
#include <stdio.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>
#include <wincrypt.h>

static int s_currentSel = -1;

// Map platform enum to string for internal storage
static const struct {
    MonitorPlatformType type;
    const char* prefix;
    const wchar_t* label;
} PLATFORMS[] = {
    { MONITOR_PLATFORM_GITHUB, "GitHub", L"GitHub" },
    { MONITOR_PLATFORM_BILIBILI, "Bilibili", L"Bilibili" }
};

static void TrimW(wchar_t* str) {
    if (!str) return;
    size_t len = wcslen(str);
    while (len > 0 && iswspace(str[len - 1])) str[--len] = L'\0';
    wchar_t* start = str;
    while (*start && iswspace(*start)) start++;
    if (start > str) wmemmove(str, start, (wcslen(start) + 1) * sizeof(wchar_t));
}

static void RefreshList(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_MONITOR_LIST);
    SendMessage(hList, LB_RESETCONTENT, 0, 0);
    
    int count = Monitor_GetConfigCount();
    for (int i = 0; i < count; i++) {
        MonitorConfig cfg;
        if (Monitor_GetConfigAt(i, &cfg)) {
            wchar_t wSource[256];
            // Show label in list instead of raw source for better UX
            if (wcslen(cfg.label) > 0) {
                wcscpy(wSource, cfg.label);
            } else {
                MultiByteToWideChar(CP_UTF8, 0, cfg.sourceString, -1, wSource, 256);
            }
            int idx = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)wSource);
            SendMessage(hList, LB_SETITEMDATA, idx, i);
        }
    }
    
    if (s_currentSel >= 0 && s_currentSel < count) {
        int lbCount = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
        for (int i = 0; i < lbCount; i++) {
            if ((int)SendMessage(hList, LB_GETITEMDATA, i, 0) == s_currentSel) {
                SendMessage(hList, LB_SETCURSEL, i, 0);
                break;
            }
        }
    }
}

static void MaskToken(const char* token, wchar_t* outBuffer, size_t outSize) {
    size_t len = strlen(token);
    if (len <= 12) {
        wcscpy(outBuffer, L"********");
    } else {
        wchar_t wToken[128];
        MultiByteToWideChar(CP_UTF8, 0, token, -1, wToken, 128);
        wchar_t prefix[9] = {0}; wcsncpy(prefix, wToken, 8);
        wchar_t suffix[5] = {0}; wcscpy(suffix, wToken + (len - 4));
        _snwprintf_s(outBuffer, outSize, _TRUNCATE, L"%s**********%s", prefix, suffix);
    }
}

static void PopulatePlatformCombo(HWND hDlg) {
    HWND hCombo = GetDlgItem(hDlg, IDC_MONITOR_PLATFORM_COMBO);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    
    for (int i = 0; i < sizeof(PLATFORMS)/sizeof(PLATFORMS[0]); i++) {
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)PLATFORMS[i].label);
        SendMessage(hCombo, CB_SETITEMDATA, idx, PLATFORMS[i].type);
    }
    // Default select first
    SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

// Try to extract user/repo from a GitHub URL
// e.g. "https://github.com/vladelaina/Catime" -> "vladelaina/Catime"
static BOOL ExtractGitHubRepoFromUrl(const wchar_t* url, wchar_t* outBuf, size_t outSize) {
    if (!url || !outBuf) return FALSE;
    
    // Simple check for github.com
    const wchar_t* host = wcsstr(url, L"github.com/");
    if (!host) return FALSE;
    
    const wchar_t* start = host + 11; // skip "github.com/"
    
    // Copy until end or next slash (if it's like /blob/main/...)
    // Actually we want user/repo, so we need exactly one slash inside
    
    // Find end of string or query param
    size_t len = 0;
    const wchar_t* p = start;
    int slashCount = 0;
    
    while (*p && *p != L'?' && *p != L'#') {
        if (*p == L'/') {
            slashCount++;
            if (slashCount > 1) break; // Stop after user/repo/
        }
        len++;
        p++;
    }
    
    // user/repo should have at least 3 chars (a/b)
    if (len < 3 || slashCount < 1) return FALSE; 
    
    // If it ended with a slash, remove it
    if (start[len-1] == L'/') len--;
    
    if (len >= outSize) return FALSE;
    
    wcsncpy(outBuf, start, len);
    outBuf[len] = L'\0';
    return TRUE;
}

static void PopulateItemCombo(HWND hDlg, MonitorPlatformType type, const char* selectedValue) {
    HWND hCombo = GetDlgItem(hDlg, IDC_MONITOR_PARAM2_COMBO);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    
    MonitorOption options[10];
    int count = Monitor_GetPlatformOptions(type, options, 10);
    
    for (int i = 0; i < count; i++) {
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)options[i].label);
        // Store index to options array as data, since we can't store string pointer easily
        // We will re-fetch options when saving
        SendMessage(hCombo, CB_SETITEMDATA, idx, i);
        
        if (selectedValue && strcmp(options[i].value, selectedValue) == 0) {
            SendMessage(hCombo, CB_SETCURSEL, idx, 0);
        }
    }
    
    if (SendMessage(hCombo, CB_GETCURSEL, 0, 0) == CB_ERR && count > 0) {
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
}

static void UpdateEditFields(HWND hDlg, int index);
static BOOL GetConfigFromUI(HWND hDlg, MonitorConfig* outCfg);

static void AutoSaveConfig(HWND hDlg) {
    if (s_currentSel < 0) return;
    
    MonitorConfig cfg;
    if (GetConfigFromUI(hDlg, &cfg)) {
        Monitor_UpdateConfigAt(s_currentSel, &cfg);
        // Update list item text in case Label changed
        HWND hList = GetDlgItem(hDlg, IDC_MONITOR_LIST);
        int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
        // Simple redraw of list might be needed if text changed, 
        // but LB doesn't update text automatically unless we delete/add.
        // For now, just refreshing list is safer but might flicker.
        // Let's only refresh if Label changed?
        // Getting old config to compare is expensive (decrypt etc).
        // Just refresh.
        // RefreshList(hDlg); // This resets selection, annoying.
        // Let's manually update the string at index?
        // ListBox_DeleteString + InsertString is the way, but complex.
        // For now, let's trust that RefreshList is called explicitly when needed (e.g. label edit killfocus)
    }
}

static void TriggerPreview(HWND hDlg) {
    // Auto-save first
    AutoSaveConfig(hDlg);

    MonitorConfig cfg;
    if (GetConfigFromUI(hDlg, &cfg)) {
        // Check if we have enough info to preview (e.g. param1 is set)
        if (strlen(cfg.param1) > 0) {
            Monitor_SetPreviewConfig(&cfg);
        }
    }
}

static void UpdateEditFields(HWND hDlg, int index) {
    SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, L""); // Clear test result
    
    MonitorConfig cfg = {0};
    if (index >= 0 && Monitor_GetConfigAt(index, &cfg)) {
        SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, cfg.label);
        SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, L"");
        
        // Parse Source String: Platform-Param1-Param2
        // e.g. GitHub-user/repo-star
        char* p1 = strchr(cfg.sourceString, '-');
        if (p1) {
            // Match platform
            size_t platLen = p1 - cfg.sourceString;
            char platStr[32];
            strncpy(platStr, cfg.sourceString, platLen);
            platStr[platLen] = '\0';
            
            MonitorPlatformType type = MONITOR_PLATFORM_UNKNOWN;
            for (int i = 0; i < sizeof(PLATFORMS)/sizeof(PLATFORMS[0]); i++) {
                if (_stricmp(PLATFORMS[i].prefix, platStr) == 0) {
                    type = PLATFORMS[i].type;
                    break;
                }
            }
            
            // Select Platform
            HWND hPlat = GetDlgItem(hDlg, IDC_MONITOR_PLATFORM_COMBO);
            int count = (int)SendMessage(hPlat, CB_GETCOUNT, 0, 0);
            for (int i = 0; i < count; i++) {
                if ((MonitorPlatformType)SendMessage(hPlat, CB_GETITEMDATA, i, 0) == type) {
                    SendMessage(hPlat, CB_SETCURSEL, i, 0);
                    break;
                }
            }
            
            // Parse Param1 and Param2
            char* p2 = strrchr(p1 + 1, '-');
            if (p2) {
                // param1 is between p1 and p2
                size_t len1 = p2 - (p1 + 1);
                char param1[128];
                if (len1 < 128) {
                    strncpy(param1, p1 + 1, len1);
                    param1[len1] = '\0';
                    wchar_t wParam1[128];
                    MultiByteToWideChar(CP_UTF8, 0, param1, -1, wParam1, 128);
                    SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, wParam1);
                }
                
                // param2 is after p2
                PopulateItemCombo(hDlg, type, p2 + 1);
            } else {
                PopulateItemCombo(hDlg, type, NULL);
            }
        }
        
        // Check if token is not empty (check first byte not 0, but it's encrypted, so we need to check if it was set)
        // Actually Monitor_LoadConfig ensures valid tokens are encrypted.
        // We can check if the buffer is all zeros to denote empty.
        BOOL hasToken = FALSE;
        for (int i = 0; i < sizeof(cfg.token); i++) {
            if (cfg.token[i] != 0) {
                hasToken = TRUE;
                break;
            }
        }

        if (hasToken) {
            // Decrypt token for display (masked)
            char tempToken[128];
            memcpy(tempToken, cfg.token, sizeof(tempToken));
            CryptUnprotectMemory(tempToken, sizeof(tempToken), CRYPTPROTECTMEMORY_SAME_PROCESS);
            
            wchar_t wMasked[128];
            MaskToken(tempToken, wMasked, 128);
            SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, wMasked);
            
            SecureZeroMemory(tempToken, sizeof(tempToken));
        } else {
             SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, L"");
        }
    } else {
        // Defaults for new item
        SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, L"");
        
        // Default to first platform
        HWND hPlat = GetDlgItem(hDlg, IDC_MONITOR_PLATFORM_COMBO);
        SendMessage(hPlat, CB_SETCURSEL, 0, 0);
        
        // Trigger population of item combo
        MonitorPlatformType type = (MonitorPlatformType)SendMessage(hPlat, CB_GETITEMDATA, 0, 0);
        PopulateItemCombo(hDlg, type, NULL);
    }
}

static BOOL GetConfigFromUI(HWND hDlg, MonitorConfig* outCfg) {
    SecureZeroMemory(outCfg, sizeof(MonitorConfig));
    
    // 1. Get Platform
    HWND hPlat = GetDlgItem(hDlg, IDC_MONITOR_PLATFORM_COMBO);
    int platIdx = (int)SendMessage(hPlat, CB_GETCURSEL, 0, 0);
    if (platIdx == CB_ERR) return FALSE;
    MonitorPlatformType type = (MonitorPlatformType)SendMessage(hPlat, CB_GETITEMDATA, platIdx, 0);
    
    const char* platPrefix = "Unknown";
    for (int i = 0; i < sizeof(PLATFORMS)/sizeof(PLATFORMS[0]); i++) {
        if (PLATFORMS[i].type == type) {
            platPrefix = PLATFORMS[i].prefix;
            break;
        }
    }
    
    // 2. Get Item (Param2)
    HWND hItem = GetDlgItem(hDlg, IDC_MONITOR_PARAM2_COMBO);
    int itemIdx = (int)SendMessage(hItem, CB_GETCURSEL, 0, 0);
    if (itemIdx == CB_ERR) return FALSE;
    int optIndex = (int)SendMessage(hItem, CB_GETITEMDATA, itemIdx, 0);
    
    MonitorOption options[10];
    int optCount = Monitor_GetPlatformOptions(type, options, 10);
    if (optIndex < 0 || optIndex >= optCount) return FALSE;
    
    // 3. Get Param1 (ID)
    wchar_t wParam1[128];
    GetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, wParam1, 128);
    TrimW(wParam1);
    if (wcslen(wParam1) == 0) {
        MessageBoxW(hDlg, L"Please enter a Target ID.", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    char param1[128];
    WideCharToMultiByte(CP_UTF8, 0, wParam1, -1, param1, 128, NULL, NULL);
    
    // 4. Construct Source String
    snprintf(outCfg->sourceString, sizeof(outCfg->sourceString), "%s-%s-%s", 
             platPrefix, param1, options[optIndex].value);
    outCfg->type = type;
    strncpy(outCfg->param1, param1, sizeof(outCfg->param1)-1);
    strncpy(outCfg->param2, options[optIndex].value, sizeof(outCfg->param2)-1);
    
    // 5. Token
    wchar_t wToken[128];
    GetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, wToken, 128);
    TrimW(wToken);
    
    BOOL tokenKept = FALSE;
    if (s_currentSel >= 0) {
        MonitorConfig oldCfg;
        if (Monitor_GetConfigAt(s_currentSel, &oldCfg)) {
            // Decrypt old token to generate mask for comparison
            char tempToken[128];
            memcpy(tempToken, oldCfg.token, sizeof(tempToken));
            if (CryptUnprotectMemory(tempToken, sizeof(tempToken), CRYPTPROTECTMEMORY_SAME_PROCESS)) {
                wchar_t wMasked[128];
                MaskToken(tempToken, wMasked, 128);
                
                if (wcscmp(wToken, wMasked) == 0) {
                    // Token hasn't changed in UI. 
                    // We return the PLAINTEXT token so the caller (Save/Test) can handle it uniformly.
                    // (i.e. caller will encrypt it)
                    memcpy(outCfg->token, tempToken, sizeof(outCfg->token));
                    tokenKept = TRUE;
                }
            }
            SecureZeroMemory(tempToken, sizeof(tempToken));
        }
    }
    
    if (!tokenKept) {
        WideCharToMultiByte(CP_UTF8, 0, wToken, -1, outCfg->token, 128, NULL, NULL);
    }
    
    // 6. Label
    wchar_t wLabel[32];
}

static INT_PTR CALLBACK MonitorDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            RECT rc, rcOwner;
            GetWindowRect(hDlg, &rc);
            HMONITOR hMonitor = MonitorFromWindow(GetParent(hDlg), MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(MONITORINFO)};
            GetMonitorInfo(hMonitor, &mi);
            rcOwner = mi.rcWork;
            int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - (rc.right - rc.left)) / 2;
            int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - (rc.bottom - rc.top)) / 2;
            SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

            PopulatePlatformCombo(hDlg);
            s_currentSel = Monitor_GetActiveIndex();
            RefreshList(hDlg);
            UpdateEditFields(hDlg, s_currentSel);
            
            // Start timer for preview updates
            SetTimer(hDlg, 1, 500, NULL);
            
            return TRUE;
        }
        
        case WM_TIMER:
            if (wParam == 1) {
                wchar_t previewText[64];
                if (Monitor_GetPreviewText(previewText, 64)) {
                    // Update result label
                    wchar_t currentText[64];
                    GetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, currentText, 64);
                    
                    wchar_t newText[128];
                    if (wcscmp(previewText, L"...") == 0) {
                        wcscpy(newText, L"Fetching...");
                    } else if (wcscmp(previewText, L"Error") == 0) {
                        wcscpy(newText, L"Error / Not Found");
                    } else {
                        swprintf(newText, 128, L"Result: %s", previewText);
                    }
                    
                    if (wcscmp(currentText, newText) != 0) {
                        SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, newText);
                    }
                }
            }
            break;
            
        case WM_DESTROY:
            KillTimer(hDlg, 1);
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_MONITOR_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE) {
                        int lbIdx = (int)SendDlgItemMessage(hDlg, IDC_MONITOR_LIST, LB_GETCURSEL, 0, 0);
                        if (lbIdx != LB_ERR) {
                            s_currentSel = (int)SendDlgItemMessage(hDlg, IDC_MONITOR_LIST, LB_GETITEMDATA, lbIdx, 0);
                            UpdateEditFields(hDlg, s_currentSel);
                            TriggerPreview(hDlg);
                        }
                    }
                    break;
                    
                case IDC_MONITOR_PLATFORM_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        int idx = (int)SendDlgItemMessage(hDlg, IDC_MONITOR_PLATFORM_COMBO, CB_GETCURSEL, 0, 0);
                        if (idx != CB_ERR) {
                            MonitorPlatformType type = (MonitorPlatformType)SendDlgItemMessage(hDlg, IDC_MONITOR_PLATFORM_COMBO, CB_GETITEMDATA, idx, 0);
                            PopulateItemCombo(hDlg, type, NULL);
                            TriggerPreview(hDlg);
                        }
                    }
                    break;
                
                case IDC_MONITOR_PARAM2_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        TriggerPreview(hDlg);
                    }
                    break;

                case IDC_MONITOR_PARAM1_EDIT:
                    if (HIWORD(wParam) == EN_KILLFOCUS) {
                        wchar_t text[512];
                        GetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, text, 512);
                        TrimW(text);
                        
                        wchar_t extracted[128];
                        if (ExtractGitHubRepoFromUrl(text, extracted, 128)) {
                            SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, extracted);
                            
                            HWND hPlat = GetDlgItem(hDlg, IDC_MONITOR_PLATFORM_COMBO);
                            int count = (int)SendMessage(hPlat, CB_GETCOUNT, 0, 0);
                            for (int i = 0; i < count; i++) {
                                if ((MonitorPlatformType)SendMessage(hPlat, CB_GETITEMDATA, i, 0) == MONITOR_PLATFORM_GITHUB) {
                                    SendMessage(hPlat, CB_SETCURSEL, i, 0);
                                    SendMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_MONITOR_PLATFORM_COMBO, CBN_SELCHANGE), (LPARAM)hPlat);
                                    break;
                                }
                            }
                        }
                        TriggerPreview(hDlg);
                    }
                    break;
                    
                case IDC_MONITOR_TOKEN_EDIT:
                    if (HIWORD(wParam) == EN_KILLFOCUS) {
                        TriggerPreview(hDlg);
                    }
                    break;
                    
                case IDC_MONITOR_ADD_BTN: {
                    // Create a default config
                    MonitorConfig newCfg = {0};
                    wcscpy(newCfg.label, L"New Item");
                    strcpy(newCfg.sourceString, "GitHub-user/repo-star"); // Default
                    newCfg.type = MONITOR_PLATFORM_GITHUB;
                    strcpy(newCfg.param1, "user/repo");
                    strcpy(newCfg.param2, "star");
                    newCfg.refreshInterval = 300;
                    newCfg.enabled = TRUE;
                    
                    Monitor_AddConfig(&newCfg);
                    s_currentSel = Monitor_GetConfigCount() - 1;
                    RefreshList(hDlg);
                    UpdateEditFields(hDlg, s_currentSel);
                    SetFocus(GetDlgItem(hDlg, IDC_MONITOR_PARAM1_EDIT));
                    break;
                }
                    
                case IDC_MONITOR_DEL_BTN:
                    if (s_currentSel >= 0) {
                        Monitor_DeleteConfigAt(s_currentSel);
                        s_currentSel = -1;
                        RefreshList(hDlg);
                        UpdateEditFields(hDlg, -1);
                        SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, L"");
                    }
                    break;
                
                case IDCANCEL:
                    // "Done" button
                    if (s_currentSel >= 0) {
                        Monitor_SetActiveIndex(s_currentSel);
                    }
                    EndDialog(hDlg, IDOK);
                    break;
            }
            return TRUE;
    }
    return FALSE;
}

void ShowMonitorConfigDialog(HWND hParent) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_MONITOR_CONFIG_DIALOG), hParent, MonitorDialogProc);
}
