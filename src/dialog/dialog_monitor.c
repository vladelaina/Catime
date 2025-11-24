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
static MonitorPlatformType s_currentPlatform = MONITOR_PLATFORM_UNKNOWN;

// Map platform enum to string for internal storage
static const struct {
    MonitorPlatformType type;
    const char* prefix;
    const wchar_t* label;
} PLATFORMS[] = {
    { MONITOR_PLATFORM_GITHUB, "GitHub", L"GitHub" },
    { MONITOR_PLATFORM_BILIBILI_USER, "BilibiliUser", L"Bilibili-User" },
    { MONITOR_PLATFORM_BILIBILI_VIDEO, "BilibiliVideo", L"Bilibili-Video" }
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

static void PopulateItemCombo(HWND hDlg, const char* selectedValue) {
    HWND hCombo = GetDlgItem(hDlg, IDC_MONITOR_PARAM2_COMBO);
    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    
    MonitorOption options[10];
    int count = Monitor_GetPlatformOptions(s_currentPlatform, options, 10);
    
    for (int i = 0; i < count; i++) {
        int idx = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)options[i].label);
        // Store index to options array as data
        SendMessage(hCombo, CB_SETITEMDATA, idx, i);
        
        if (selectedValue && strcmp(options[i].value, selectedValue) == 0) {
            SendMessage(hCombo, CB_SETCURSEL, idx, 0);
        }
    }
    
    if (SendMessage(hCombo, CB_GETCURSEL, 0, 0) == CB_ERR && count > 0) {
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
}

static void UpdateLabelsForPlatform(HWND hDlg, MonitorPlatformType type);

// Helper: Update current platform state and UI
static void UpdateCurrentPlatform(HWND hDlg, MonitorPlatformType type) {
    if (s_currentPlatform != type) {
        s_currentPlatform = type;
        UpdateLabelsForPlatform(hDlg, type);
        PopulateItemCombo(hDlg, NULL);
    }
}

// Try to extract user/repo from a GitHub URL or string
// e.g. "https://github.com/vladelaina/Catime" -> "vladelaina/Catime"
//      "vladelaina/Catime" -> "vladelaina/Catime"
static BOOL DetectGitHub(const wchar_t* input, wchar_t* outBuf, size_t outSize) {
    if (!input || !outBuf) return FALSE;
    
    // 1. Check for URL
    const wchar_t* host = wcsstr(input, L"github.com/");
    if (host) {
        const wchar_t* start = host + 11; // skip "github.com/"
        size_t len = 0;
        const wchar_t* p = start;
        int slashCount = 0;
        while (*p && *p != L'?' && *p != L'#') {
            if (*p == L'/') {
                slashCount++;
                if (slashCount > 1) break; 
            }
            len++; p++;
        }
        if (len < 3 || slashCount < 1) return FALSE;
        if (start[len-1] == L'/') len--;
        if (len >= outSize) return FALSE;
        wcsncpy(outBuf, start, len);
        outBuf[len] = L'\0';
        return TRUE;
    }

    // 2. Check for "user/repo" format (must have exactly one slash, no spaces)
    if (wcschr(input, L'/')) {
        // Must not start or end with slash
        if (input[0] == L'/' || input[wcslen(input)-1] == L'/') return FALSE;
        
        int slashes = 0;
        const wchar_t* p = input;
        while (*p) {
            if (*p == L'/') slashes++;
            if (iswspace(*p)) return FALSE; // No spaces allowed
            p++;
        }
        
        if (slashes == 1) {
            wcsncpy(outBuf, input, outSize - 1);
            outBuf[outSize - 1] = L'\0';
            return TRUE;
        }
    }
    
    return FALSE;
}

// Try to extract UID from a Bilibili Space URL or pure number
static BOOL DetectBilibiliUser(const wchar_t* input, wchar_t* outBuf, size_t outSize) {
    if (!input || !outBuf) return FALSE;
    
    // 1. URL
    const wchar_t* host = wcsstr(input, L"space.bilibili.com/");
    if (host) {
        const wchar_t* start = host + 19;
        size_t len = 0;
        const wchar_t* p = start;
        while (*p && iswdigit(*p)) { len++; p++; }
        if (len > 0 && len < outSize) {
            wcsncpy(outBuf, start, len);
            outBuf[len] = L'\0';
            return TRUE;
        }
    }
    
    // 2. Pure Number (heuristic)
    // Must be > 3 digits to avoid confusion with other small numbers?
    // Bilibili UIDs are usually long.
    size_t len = wcslen(input);
    if (len > 0) {
        BOOL allDigits = TRUE;
        for (size_t i = 0; i < len; i++) {
            if (!iswdigit(input[i])) {
                allDigits = FALSE;
                break;
            }
        }
        if (allDigits) {
            wcsncpy(outBuf, input, outSize - 1);
            outBuf[outSize - 1] = L'\0';
            return TRUE;
        }
    }
    
    return FALSE;
}

// Try to extract BV ID from a Bilibili Video URL or BV string
static BOOL DetectBilibiliVideo(const wchar_t* input, wchar_t* outBuf, size_t outSize) {
    if (!input || !outBuf) return FALSE;
    
    // 1. URL
    const wchar_t* host = wcsstr(input, L"bilibili.com/video/");
    if (host) {
        const wchar_t* start = host + 19;
        size_t len = 0;
        const wchar_t* p = start;
        while (*p && iswalnum(*p)) { len++; p++; }
        if (len > 2 && len < outSize) {
            wcsncpy(outBuf, start, len);
            outBuf[len] = L'\0';
            return TRUE;
        }
    }
    
    // 2. BV String (starts with BV or bv, alphanumeric)
    if ((input[0] == L'B' || input[0] == L'b') && 
        (input[1] == L'V' || input[1] == L'v')) {
        
        size_t len = wcslen(input);
        BOOL valid = TRUE;
        for (size_t i = 2; i < len; i++) {
            if (!iswalnum(input[i])) {
                valid = FALSE;
                break;
            }
        }
        if (valid && len > 2) {
            wcsncpy(outBuf, input, outSize - 1);
            outBuf[outSize - 1] = L'\0';
            return TRUE;
        }
    }
    
    return FALSE;
}


static void UpdateLabelsForPlatform(HWND hDlg, MonitorPlatformType type) {
    HWND hParam1Label = GetDlgItem(hDlg, IDC_MONITOR_PARAM1_LABEL);
    HWND hTokenLabel = GetDlgItem(hDlg, IDC_MONITOR_TOKEN_LABEL);
    HWND hTokenEdit = GetDlgItem(hDlg, IDC_MONITOR_TOKEN_EDIT);
    HWND hItemLabel = GetDlgItem(hDlg, IDC_MONITOR_ITEM_LABEL);

    // Always show "Target Link" to encourage link usage
    SetWindowTextW(hParam1Label, L"Link:");

    if (type == MONITOR_PLATFORM_GITHUB) {
        SetWindowTextW(hItemLabel, L"Item (GitHub):");
        SetWindowTextW(hTokenLabel, L"Token (Optional):");
        ShowWindow(hTokenLabel, SW_SHOW);
        ShowWindow(hTokenEdit, SW_SHOW);
    } else if (type == MONITOR_PLATFORM_BILIBILI_USER) {
        SetWindowTextW(hItemLabel, L"Item (Bilibili User):");
        ShowWindow(hTokenLabel, SW_HIDE);
        ShowWindow(hTokenEdit, SW_HIDE);
    } else if (type == MONITOR_PLATFORM_BILIBILI_VIDEO) {
        SetWindowTextW(hItemLabel, L"Item (Bilibili Video):");
        ShowWindow(hTokenLabel, SW_HIDE);
        ShowWindow(hTokenEdit, SW_HIDE);
    } else {
        SetWindowTextW(hItemLabel, L"Item (Unknown):");
        // Bilibili etc don't use token in UI for now
        ShowWindow(hTokenLabel, SW_HIDE);
        ShowWindow(hTokenEdit, SW_HIDE);
    }
}

// Forward declarations to handle dependencies
static void HandleInputDetection(HWND hDlg);
static void UpdateEditFields(HWND hDlg, int index, BOOL skipLabel);
static BOOL GetConfigFromUI(HWND hDlg, MonitorConfig* outCfg, BOOL silent);

static void AutoSaveConfig(HWND hDlg) {
    if (s_currentSel < 0) return;
    
    MonitorConfig cfg;
    if (GetConfigFromUI(hDlg, &cfg, TRUE)) { // Silent autosave
        Monitor_UpdateConfigAt(s_currentSel, &cfg);
        // Label update is handled by RefreshList in caller
    }
}

static void TriggerPreview(HWND hDlg) {
    // Auto-save first
    AutoSaveConfig(hDlg);

    MonitorConfig cfg;
    if (GetConfigFromUI(hDlg, &cfg, TRUE)) { // Silent check
        // Check if we have enough info to preview (e.g. param1 is set)
        if (strlen(cfg.param1) > 0) {
            Monitor_SetPreviewConfig(&cfg);
        } else {
            // Clear preview if config is empty/invalid
            MonitorConfig emptyCfg = {0};
            Monitor_SetPreviewConfig(&emptyCfg);
        }
    }
}

// Central logic to detect platform from text and update UI
static void HandleInputDetection(HWND hDlg) {
    wchar_t text[512];
    GetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, text, 512);
    TrimW(text);
    
    if (wcslen(text) == 0) return;

    wchar_t extracted[128];
    
    // 1. Check for Bilibili Video (BV...) - High specificity
    if (DetectBilibiliVideo(text, extracted, 128)) {
        UpdateCurrentPlatform(hDlg, MONITOR_PLATFORM_BILIBILI_VIDEO);
        return;
    }
    
    // 2. Check for GitHub (URL or user/repo)
    if (DetectGitHub(text, extracted, 128)) {
        UpdateCurrentPlatform(hDlg, MONITOR_PLATFORM_GITHUB);
        return;
    }
    
    // 3. Check for Bilibili User (digits) - Lowest specificity
    if (DetectBilibiliUser(text, extracted, 128)) {
        UpdateCurrentPlatform(hDlg, MONITOR_PLATFORM_BILIBILI_USER);
        return;
    }

    UpdateCurrentPlatform(hDlg, MONITOR_PLATFORM_UNKNOWN);
}

static void UpdateEditFields(HWND hDlg, int index, BOOL skipLabel) {
    SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, L""); // Clear test result
    
    MonitorConfig cfg = {0};
    if (index >= 0 && Monitor_GetConfigAt(index, &cfg)) {
        if (!skipLabel) {
            SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, cfg.label);
        }
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
            
            // Set current platform state
            UpdateCurrentPlatform(hDlg, type);
            
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
                    
                    // Reconstruct URL for display to allow easy editing
                    wchar_t wUrl[256];
                    if (type == MONITOR_PLATFORM_GITHUB) {
                        swprintf(wUrl, 256, L"https://github.com/%s", wParam1);
                    } else if (type == MONITOR_PLATFORM_BILIBILI_USER) {
                        swprintf(wUrl, 256, L"https://space.bilibili.com/%s", wParam1);
                    } else if (type == MONITOR_PLATFORM_BILIBILI_VIDEO) {
                         swprintf(wUrl, 256, L"https://www.bilibili.com/video/%s", wParam1);
                    } else {
                        wcscpy(wUrl, wParam1);
                    }
                    
                    SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, wUrl);
                }
                
                // param2 is after p2
                PopulateItemCombo(hDlg, p2 + 1);
            } else {
                PopulateItemCombo(hDlg, NULL);
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
        if (!skipLabel) {
            SetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, L"");
        }
        SetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, L"");
        SetDlgItemTextW(hDlg, IDC_MONITOR_TOKEN_EDIT, L"");
        
        // Default to first platform or unknown? Let's say unknown until typed
        UpdateCurrentPlatform(hDlg, MONITOR_PLATFORM_UNKNOWN);
    }
}

static BOOL GetConfigFromUI(HWND hDlg, MonitorConfig* outCfg, BOOL silent) {
    SecureZeroMemory(outCfg, sizeof(MonitorConfig));
    
    // 0. Always get Label first so we can save it even if other fields are invalid
    wchar_t wLabel[32];
    GetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, wLabel, 32);
    TrimW(wLabel);
    wcsncpy(outCfg->label, wLabel, 32);
    outCfg->label[31] = L'\0';

    // 1. Get Platform from state (auto-detected)
    if (s_currentPlatform == MONITOR_PLATFORM_UNKNOWN) {
        // Try one last detection pass
        HandleInputDetection(hDlg);
        if (s_currentPlatform == MONITOR_PLATFORM_UNKNOWN) {
            // Fallback to Unknown, never show message box
            strcpy(outCfg->sourceString, "Unknown--");
            outCfg->type = MONITOR_PLATFORM_UNKNOWN;
            return TRUE;
        }
    }
    MonitorPlatformType type = s_currentPlatform;
    
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
    
    // If combo is empty or invalid, we can't proceed with a valid config
    if (itemIdx == CB_ERR) {
        // Fallback
        strcpy(outCfg->sourceString, "Unknown--");
        outCfg->type = MONITOR_PLATFORM_UNKNOWN;
        return TRUE;
    }
    
    int optIndex = (int)SendMessage(hItem, CB_GETITEMDATA, itemIdx, 0);
    MonitorOption options[10];
    int optCount = Monitor_GetPlatformOptions(type, options, 10);
    
    if (optIndex < 0 || optIndex >= optCount) {
        // Fallback
        strcpy(outCfg->sourceString, "Unknown--");
        outCfg->type = MONITOR_PLATFORM_UNKNOWN;
        return TRUE;
    }
    
    // 3. Get Param1 (ID)
    wchar_t wParam1[128];
    GetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, wParam1, 128);
    TrimW(wParam1);
    if (wcslen(wParam1) == 0) {
        // Fallback
        strcpy(outCfg->sourceString, "Unknown--");
        outCfg->type = MONITOR_PLATFORM_UNKNOWN;
        return TRUE;
    }
    
    // Extract ID from URL
    wchar_t extracted[128];
    if (type == MONITOR_PLATFORM_GITHUB && DetectGitHub(wParam1, extracted, 128)) {
        wcscpy(wParam1, extracted);
    } else if (type == MONITOR_PLATFORM_BILIBILI_VIDEO && DetectBilibiliVideo(wParam1, extracted, 128)) {
        wcscpy(wParam1, extracted);
    } else if (type == MONITOR_PLATFORM_BILIBILI_USER && DetectBilibiliUser(wParam1, extracted, 128)) {
        wcscpy(wParam1, extracted);
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
    
    return TRUE;
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

            s_currentSel = Monitor_GetActiveIndex();
            RefreshList(hDlg);
            UpdateEditFields(hDlg, s_currentSel, FALSE);
            
            // Start timer for preview updates
            SetTimer(hDlg, 1, 500, NULL);
            
            return TRUE;
        }
        
        case WM_TIMER:
            if (wParam == 1) {
                // Suppress preview if input is empty
                wchar_t inputCheck[512];
                GetDlgItemTextW(hDlg, IDC_MONITOR_PARAM1_EDIT, inputCheck, 512);
                TrimW(inputCheck);
                if (wcslen(inputCheck) == 0) {
                    SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, L"");
                    break;
                }

                wchar_t previewText[64];
                if (Monitor_GetPreviewText(previewText, 64)) {
                    // Update result label
                    wchar_t currentText[64];
                    GetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, currentText, 64);
                    
                    wchar_t newText[128];
                    if (wcscmp(previewText, L"...") == 0) {
                        wcscpy(newText, L"Fetching...");
                    } else if (wcsncmp(previewText, L"Error", 5) == 0) {
                        // If it's an error (generic or specific), just show it without "Result:" prefix
                        // e.g. "Error", "Error 403", "Error 12002"
                        wcscpy(newText, previewText);
                    } else {
                        // For valid values, also just show the value (cleaner)
                        swprintf(newText, 128, L"%s", previewText);
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
                            UpdateEditFields(hDlg, s_currentSel, FALSE);
                            TriggerPreview(hDlg);
                        }
                    }
                    break;
                    
                // IDC_MONITOR_PLATFORM_COMBO removed
                
                case IDC_MONITOR_PARAM2_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        TriggerPreview(hDlg);
                    }
                    break;

                case IDC_MONITOR_LABEL_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        if (s_currentSel < 0) {
                            int len = GetWindowTextLength(GetDlgItem(hDlg, IDC_MONITOR_LABEL_EDIT));
                            if (len > 0) {
                                // Auto-create new item
                                MonitorConfig newCfg = {0};
                                GetDlgItemTextW(hDlg, IDC_MONITOR_LABEL_EDIT, newCfg.label, 32);
                                
                                strcpy(newCfg.sourceString, "Unknown--"); 
                                newCfg.type = MONITOR_PLATFORM_UNKNOWN;
                                strcpy(newCfg.param1, "");
                                strcpy(newCfg.param2, "");
                                newCfg.refreshInterval = 300;
                                newCfg.enabled = TRUE;
                                
                                Monitor_AddConfig(&newCfg);
                                s_currentSel = Monitor_GetConfigCount() - 1;
                                
                                RefreshList(hDlg);
                                UpdateEditFields(hDlg, s_currentSel, TRUE); // Update others, skip label
                            }
                        } else {
                            // Real-time update for existing item
                            AutoSaveConfig(hDlg);
                            RefreshList(hDlg);
                        }
                    }
                    break;

                case IDC_MONITOR_PARAM1_EDIT:
                    if (HIWORD(wParam) == EN_CHANGE) {
                        // Detect platform as you type/paste
                        HandleInputDetection(hDlg);
                        // Also trigger preview (debouncing handled by timer or preview logic if needed, 
                        // but for now direct trigger is fine as network ops are on thread)
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
                    strcpy(newCfg.sourceString, "Unknown--"); // Empty/Unknown
                    newCfg.type = MONITOR_PLATFORM_UNKNOWN;
                    strcpy(newCfg.param1, "");
                    strcpy(newCfg.param2, "");
                    newCfg.refreshInterval = 300;
                    newCfg.enabled = TRUE;
                    
                    Monitor_AddConfig(&newCfg);
                    s_currentSel = Monitor_GetConfigCount() - 1;
                    RefreshList(hDlg);
                    UpdateEditFields(hDlg, s_currentSel, FALSE);
                    SetFocus(GetDlgItem(hDlg, IDC_MONITOR_PARAM1_EDIT));
                    TriggerPreview(hDlg); // Reset preview for new item
                    break;
                }
                    
                case IDC_MONITOR_DEL_BTN:
                    if (s_currentSel >= 0) {
                        Monitor_DeleteConfigAt(s_currentSel);
                        s_currentSel = -1;
                        RefreshList(hDlg);
                        UpdateEditFields(hDlg, -1, FALSE);
                        SetDlgItemTextW(hDlg, IDC_MONITOR_TEST_RESULT, L"");
                    }
                    break;
                
                case IDCANCEL:
                    // "Done" button
                    AutoSaveConfig(hDlg);
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
