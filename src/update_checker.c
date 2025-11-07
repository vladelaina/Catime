/**
 * @file update_checker.c
 * @brief GitHub version checking and update management
 * 
 * Key features:
 * - Semantic versioning comparison (supports pre-release tags)
 * - GitHub API integration for release info
 * - Multi-language update dialogs
 */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "update_checker.h"
#include "log.h"
#include "language.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_procedure.h"
#include "../resource/resource.h"
#include "utils/string_convert.h"

#pragma comment(lib, "wininet.lib")

#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"
#define VERSION_BUFFER_SIZE 32
#define URL_BUFFER_SIZE 512
#define NOTES_BUFFER_SIZE 4096
#define INITIAL_HTTP_BUFFER_SIZE 8192
#define ERROR_MSG_BUFFER_SIZE 256

/** @brief Version info for dialog display */
typedef struct {
    const char* currentVersion;
    const char* latestVersion;
    const char* downloadUrl;
    const char* releaseNotes;
} VersionInfo;

/** @brief Pre-release type priority (alpha < beta < rc < stable) */
typedef struct {
    const char* prefix;
    int prefixLen;
    int priority;
} PreReleaseType;

/** @brief HTTP resource handles for cleanup */
typedef struct {
    HINTERNET hInternet;
    HINTERNET hConnect;
    char* buffer;
} HttpResources;

static const PreReleaseType PRE_RELEASE_TYPES[] = {
    {"alpha", 5, 1},
    {"beta", 4, 2},
    {"rc", 2, 3}
};

static const int PRE_RELEASE_TYPE_COUNT = sizeof(PRE_RELEASE_TYPES) / sizeof(PreReleaseType);

/* Thin wrappers using utils/string_convert.h */
static inline wchar_t* LocalUtf8ToWideAlloc(const char* utf8Str) {
    return Utf8ToWideAlloc(utf8Str);
}

static inline BOOL LocalUtf8ToWideFixed(const char* utf8Str, wchar_t* wideBuf, int bufSize) {
    return Utf8ToWide(utf8Str, wideBuf, (size_t)bufSize);
}

/**
 * @brief Extract string field from GitHub JSON response
 * @param fieldName Field to extract (e.g., "tag_name", "body")
 * @return TRUE if field found and extracted
 */
static BOOL ExtractJsonStringField(const char* json, const char* fieldName, char* output, size_t maxLen) {
    if (!json || !fieldName || !output || maxLen == 0) return FALSE;
    
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", fieldName);
    
    const char* fieldPos = strstr(json, pattern);
    if (!fieldPos) {
        LOG_ERROR("JSON field not found: %s", fieldName);
        return FALSE;
    }
    
    const char* valueStart = strchr(fieldPos + strlen(pattern), '\"');
    if (!valueStart) return FALSE;
    valueStart++;
    
    const char* valueEnd = valueStart;
    int escapeCount = 0;
    while (*valueEnd) {
        if (*valueEnd == '\\') {
            escapeCount++;
        } else if (*valueEnd == '\"' && (escapeCount % 2 == 0)) {
            break;
        } else if (*valueEnd != '\\') {
            escapeCount = 0;
        }
        valueEnd++;
    }
    
    if (*valueEnd != '\"') return FALSE;
    
    size_t valueLen = valueEnd - valueStart;
    if (valueLen >= maxLen) valueLen = maxLen - 1;
    strncpy(output, valueStart, valueLen);
    output[valueLen] = '\0';
    
    return TRUE;
}

/** @brief Process JSON escape sequences (\n, \r, \", \\) */
static void ProcessJsonEscapes(const char* input, char* output, size_t maxLen) {
    if (!input || !output || maxLen == 0) return;
    
    size_t writePos = 0;
    size_t inputLen = strlen(input);
    
    for (size_t i = 0; i < inputLen && writePos < maxLen - 1; i++) {
        if (input[i] == '\\' && i + 1 < inputLen) {
            switch (input[i + 1]) {
                case 'n':
                    output[writePos++] = '\r';
                    if (writePos < maxLen - 1) output[writePos++] = '\n';
                    i++;
                    break;
                case 'r':
                    output[writePos++] = '\r';
                    i++;
                    break;
                case '\"':
                    output[writePos++] = '\"';
                    i++;
                    break;
                case '\\':
                    output[writePos++] = '\\';
                    i++;
                    break;
                default:
                    output[writePos++] = input[i];
            }
        } else {
            output[writePos++] = input[i];
        }
    }
    output[writePos] = '\0';
}

/**
 * @brief Parse pre-release type and number
 * @param preRelease String like "alpha2", "beta1", "rc3"
 * @param outType Priority: 1=alpha, 2=beta, 3=rc, 0=unknown
 * @param outNum Version number after prefix
 */
static void ParsePreReleaseInfo(const char* preRelease, int* outType, int* outNum) {
    *outType = 0;
    *outNum = 0;
    
    if (!preRelease || !preRelease[0]) return;
    
    for (int i = 0; i < PRE_RELEASE_TYPE_COUNT; i++) {
        const PreReleaseType* type = &PRE_RELEASE_TYPES[i];
        if (strncmp(preRelease, type->prefix, type->prefixLen) == 0) {
            *outType = type->priority;
            sscanf(preRelease + type->prefixLen, "%d", outNum);
            return;
        }
    }
}

/**
 * @brief Extract pre-release tag from version
 * @param version Full version like "1.3.0-alpha2"
 * @return TRUE if pre-release tag found
 */
static BOOL ExtractPreRelease(const char* version, char* preRelease, size_t maxLen) {
    const char* dash = strchr(version, '-');
    if (dash && *(dash + 1)) {
        size_t len = strlen(dash + 1);
        if (len >= maxLen) len = maxLen - 1;
        strncpy(preRelease, dash + 1, len);
        preRelease[len] = '\0';
        return TRUE;
    }
    preRelease[0] = '\0';
    return FALSE;
}

/**
 * @brief Compare pre-release tags
 * @return 1 if pre1 > pre2, -1 if pre1 < pre2, 0 if equal
 * @note Stable > rc > beta > alpha
 */
static int ComparePreRelease(const char* pre1, const char* pre2) {
    if (!pre1[0] && !pre2[0]) return 0;
    
    if (!pre1[0]) return 1;
    if (!pre2[0]) return -1;
    
    int type1, num1, type2, num2;
    ParsePreReleaseInfo(pre1, &type1, &num1);
    ParsePreReleaseInfo(pre2, &type2, &num2);
    
    if (type1 != type2) {
        return (type1 > type2) ? 1 : -1;
    }
    
    if (num1 != num2) {
        return (num1 > num2) ? 1 : -1;
    }
    
    return strcmp(pre1, pre2);
}

/**
 * @brief Compare semantic versions (major.minor.patch-prerelease)
 * @return 1 if v1 > v2, -1 if v1 < v2, 0 if equal
 */
int CompareVersions(const char* version1, const char* version2) {
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    // Parse major.minor.patch
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    if (major1 != major2) return (major1 > major2) ? 1 : -1;
    if (minor1 != minor2) return (minor1 > minor2) ? 1 : -1;
    if (patch1 != patch2) return (patch1 > patch2) ? 1 : -1;
    
    char preRelease1[64] = {0};
    char preRelease2[64] = {0};
    ExtractPreRelease(version1, preRelease1, sizeof(preRelease1));
    ExtractPreRelease(version2, preRelease2, sizeof(preRelease2));
    
    return ComparePreRelease(preRelease1, preRelease2);
}

/**
 * @brief Parse GitHub release JSON
 * @return TRUE if all required fields extracted
 * @note Strips 'v' prefix from tag_name
 */
static BOOL ParseGitHubRelease(const char* jsonResponse, char* latestVersion, size_t versionMaxLen,
                               char* downloadUrl, size_t urlMaxLen, char* releaseNotes, size_t notesMaxLen) {
    if (!ExtractJsonStringField(jsonResponse, "tag_name", latestVersion, versionMaxLen)) {
        return FALSE;
    }
    
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, strlen(latestVersion));
    }
    
    if (!ExtractJsonStringField(jsonResponse, "browser_download_url", downloadUrl, urlMaxLen)) {
        return FALSE;
    }
    
    char rawNotes[NOTES_BUFFER_SIZE];
    if (ExtractJsonStringField(jsonResponse, "body", rawNotes, sizeof(rawNotes))) {
        ProcessJsonEscapes(rawNotes, releaseNotes, notesMaxLen);
    } else {
        LOG_WARNING("Release notes not found, using default text");
        StringCbCopyA(releaseNotes, notesMaxLen, "No release notes available.");
    }
    
    return TRUE;
}

/** @brief Initialize dialog (center, localize) */
static void InitializeDialog(HWND hwndDlg, int dialogId) {
    ApplyDialogLanguage(hwndDlg, dialogId);
    MoveDialogToPrimaryScreen(hwndDlg);
}

/** @brief Exit notification dialog */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            InitializeDialog(hwndDlg, IDD_EXIT_DIALOG);
            
            SetDlgItemTextW(hwndDlg, IDC_EXIT_TEXT, 
                GetLocalizedString(NULL, L"The application will exit now"));
            SetDlgItemTextW(hwndDlg, IDOK, 
                GetLocalizedString(NULL, L"OK"));
            SetWindowTextW(hwndDlg, 
                GetLocalizedString(NULL, L"Catime - Update Notice"));
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

/** @brief Update available dialog */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static VersionInfo* versionInfo = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            InitializeDialog(hwndDlg, IDD_UPDATE_DIALOG);
            versionInfo = (VersionInfo*)lParam;
            
            if (versionInfo) {
                // Convert version numbers to wide characters
                wchar_t* currentVerW = LocalUtf8ToWideAlloc(versionInfo->currentVersion);
                wchar_t* latestVerW = LocalUtf8ToWideAlloc(versionInfo->latestVersion);
                
                if (currentVerW && latestVerW) {
                    wchar_t displayText[256];
                    StringCbPrintfW(displayText, sizeof(displayText), L"%s %s\n%s %s",
                        GetLocalizedString(NULL, L"Current version:"), currentVerW,
                        GetLocalizedString(NULL, L"New version:"), latestVerW);
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                }
                
                free(currentVerW);
                free(latestVerW);
                
                // Display release notes
                wchar_t* notesW = LocalUtf8ToWideAlloc(versionInfo->releaseNotes);
                if (notesW && notesW[0]) {
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_NOTES, notesW);
                } else {
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_NOTES,
                        GetLocalizedString(NULL, L"No release notes available."));
                }
                free(notesW);
                
                // Set button texts
                SetDlgItemTextW(hwndDlg, IDYES, GetLocalizedString(NULL, L"Update Now"));
                SetDlgItemTextW(hwndDlg, IDNO, GetLocalizedString(NULL, L"Later"));
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT,
                    GetLocalizedString(NULL, 
                                      L"Click 'Update Now' to open browser and download the new version"));
                SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Update Available"));
                
                ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_HIDE);
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDNO);
            return TRUE;
    }
    return FALSE;
}

/** @brief Update error dialog */
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            InitializeDialog(hwndDlg, IDD_UPDATE_ERROR_DIALOG);
            if (lParam) {
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_ERROR_TEXT, (const wchar_t*)lParam);
            }
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

/** @brief No update dialog */
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            InitializeDialog(hwndDlg, IDD_NO_UPDATE_DIALOG);
            
            const char* currentVersion = (const char*)lParam;
            if (currentVersion) {
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) baseText = L"You are already using the latest version!";
                
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage), L"%s\n%s %hs",
                    baseText,
                    GetLocalizedString(NULL, L"Current version:"),
                    currentVersion);
                SetDlgItemTextW(hwndDlg, IDC_NO_UPDATE_TEXT, fullMessage);
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

static void ShowExitMessageDialog(HWND hwnd) {
    DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_EXIT_DIALOG), hwnd, ExitMsgDlgProc);
}

static int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion,
                                  const char* downloadUrl, const char* releaseNotes) {
    VersionInfo info = {currentVersion, latestVersion, downloadUrl, releaseNotes};
    return DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_DIALOG), 
                          hwnd, UpdateDlgProc, (LPARAM)&info);
}

static void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_ERROR_DIALOG), 
                   hwnd, UpdateErrorDlgProc, (LPARAM)errorMsg);
}

static void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG), 
                   hwnd, NoUpdateDlgProc, (LPARAM)currentVersion);
}

/** @brief Initialize HTTP session */
static BOOL InitHttpResources(HttpResources* res) {
    memset(res, 0, sizeof(HttpResources));
    
    wchar_t wUserAgent[256];
    LocalUtf8ToWideFixed(USER_AGENT, wUserAgent, 256);
    
    res->hInternet = InternetOpenW(wUserAgent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!res->hInternet) {
        LOG_ERROR("Failed to create Internet session (error code: %lu)", GetLastError());
        return FALSE;
    }
    
    LOG_INFO("Internet session created successfully");
    return TRUE;
}

/** @brief Connect to GitHub API */
static BOOL ConnectToGitHub(HttpResources* res) {
    wchar_t wUrl[URL_BUFFER_SIZE];
    LocalUtf8ToWideFixed(GITHUB_API_URL, wUrl, URL_BUFFER_SIZE);
    
    res->hConnect = InternetOpenUrlW(res->hInternet, wUrl, NULL, 0,
                                     INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!res->hConnect) {
        LOG_ERROR("Failed to connect to GitHub API (error code: %lu)", GetLastError());
        return FALSE;
    }
    
    LOG_INFO("Successfully connected to GitHub API");
    return TRUE;
}

/** @brief Read HTTP response into dynamic buffer */
static BOOL ReadHttpResponse(HttpResources* res) {
    size_t bufferSize = INITIAL_HTTP_BUFFER_SIZE;
    res->buffer = (char*)malloc(bufferSize);
    if (!res->buffer) {
        LOG_ERROR("Memory allocation failed");
        return FALSE;
    }
    
    DWORD totalBytes = 0;
    DWORD bytesRead;
    
    while (InternetReadFile(res->hConnect, res->buffer + totalBytes,
                           bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        totalBytes += bytesRead;
        
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(res->buffer, newSize);
            if (!newBuffer) {
                LOG_ERROR("Buffer expansion failed (current size: %zu)", bufferSize);
                return FALSE;
            }
            res->buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    res->buffer[totalBytes] = '\0';
    LOG_INFO("Successfully read API response (%lu bytes)", totalBytes);
    return TRUE;
}

/** @brief Clean up HTTP resources */
static void CleanupHttpResources(HttpResources* res) {
    if (res->buffer) {
        free(res->buffer);
        res->buffer = NULL;
    }
    if (res->hConnect) {
        InternetCloseHandle(res->hConnect);
        res->hConnect = NULL;
    }
    if (res->hInternet) {
        InternetCloseHandle(res->hInternet);
        res->hInternet = NULL;
    }
}

/**
 * @brief Open browser to download URL and exit app
 * @return TRUE if browser opened successfully
 */
static BOOL OpenBrowserAndExit(const char* url, HWND hwnd) {
    wchar_t* urlW = LocalUtf8ToWideAlloc(url);
    if (!urlW) return FALSE;
    
    HINSTANCE hInstance = ShellExecuteW(hwnd, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
    free(urlW);
    
    if ((INT_PTR)hInstance <= 32) {
        ShowUpdateErrorDialog(hwnd, 
            GetLocalizedString(NULL, L"Could not open browser to download update"));
        return FALSE;
    }
    
    LOG_INFO("Successfully opened browser, application will exit");
    ShowExitMessageDialog(hwnd);
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief Perform update check
 * @param silentCheck TRUE=only show if update found, FALSE=show all results
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Starting update check (silent mode: %s)", silentCheck ? "yes" : "no");
    
    HttpResources res;
    
    if (!InitHttpResources(&res)) {
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not create Internet connection"));
        }
        return;
    }
    
    if (!ConnectToGitHub(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not connect to update server"));
        }
        return;
    }
    
    if (!ReadHttpResponse(&res)) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Failed to read server response"));
        }
        return;
    }
    
    char latestVersion[VERSION_BUFFER_SIZE] = {0};
    char downloadUrl[URL_BUFFER_SIZE] = {0};
    char releaseNotes[NOTES_BUFFER_SIZE] = {0};
    
    if (!ParseGitHubRelease(res.buffer, latestVersion, sizeof(latestVersion),
                           downloadUrl, sizeof(downloadUrl), releaseNotes, sizeof(releaseNotes))) {
        CleanupHttpResources(&res);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, 
                GetLocalizedString(NULL, L"Could not parse version information"));
        }
        return;
    }
    
    CleanupHttpResources(&res);
    
    LOG_INFO("GitHub latest version: %s, download URL: %s", latestVersion, downloadUrl);
    
    const char* currentVersion = CATIME_VERSION;
    LOG_INFO("Current version: %s", currentVersion);
    
    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        LOG_INFO("New version found! Current: %s, Latest: %s", currentVersion, latestVersion);
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl, releaseNotes);
        
        if (response == IDYES) {
            LOG_INFO("User chose to update now");
            OpenBrowserAndExit(downloadUrl, hwnd);
        } else {
            LOG_INFO("User chose to update later");
        }
    } else {
        LOG_INFO("Already using latest version: %s", currentVersion);
        if (!silentCheck) {
            ShowNoUpdateDialog(hwnd, currentVersion);
        }
    }
    
    LOG_INFO("Update check completed");
}

void CheckForUpdate(HWND hwnd) {
    CheckForUpdateInternal(hwnd, FALSE);
}

void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    CheckForUpdateInternal(hwnd, silentCheck);
}
