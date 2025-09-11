/**
 * @file update_checker.c
 * @brief Automatic update checking system with GitHub API integration
 * Handles version comparison, JSON parsing, and user update dialogs
 */
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strsafe.h>
#include "../include/update_checker.h"
#include "../include/log.h"
#include "../include/language.h"
#include "../include/dialog_language.h"
#include "../include/dialog_procedure.h"
#include "../resource/resource.h"

#pragma comment(lib, "wininet.lib")

/** @brief GitHub API endpoint for retrieving latest release information */
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"

/** @brief User agent string for HTTP requests to GitHub API */
#define USER_AGENT "Catime Update Checker"

/** @brief Structure containing version information for update dialogs */
typedef struct {
    const char* currentVersion;     /**< Current application version */
    const char* latestVersion;      /**< Latest available version from GitHub */
    const char* downloadUrl;        /**< Direct download URL for latest release */
    const char* releaseNotes;       /**< Release notes/body from GitHub release */
} UpdateVersionInfo;

/** @brief Dialog procedure declarations for various update scenarios */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Compare two semantic version strings using major.minor.patch format
 * @param version1 First version string to compare
 * @param version2 Second version string to compare  
 * @return 1 if version1 > version2, -1 if version1 < version2, 0 if equal
 * Uses semantic versioning comparison with hierarchical precedence
 */
int CompareVersions(const char* version1, const char* version2) {
    LOG_DEBUG("Comparing versions: '%s' vs '%s'", version1, version2);
    
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    LOG_DEBUG("Parsed version1: %d.%d.%d, version2: %d.%d.%d", major1, minor1, patch1, major2, minor2, patch2);
    
    /** Compare major version first (highest precedence) */
    if (major1 > major2) return 1;
    if (major1 < major2) return -1;
    
    /** Compare minor version if major versions are equal */
    if (minor1 > minor2) return 1;
    if (minor1 < minor2) return -1;
    
    /** Compare patch version if major and minor are equal */
    if (patch1 > patch2) return 1;
    if (patch1 < patch2) return -1;
    
    return 0;
}

/**
 * @brief Parse GitHub API JSON response to extract version, download URL, and release notes
 * @param jsonResponse Raw JSON response from GitHub API
 * @param latestVersion Output buffer for extracted version string
 * @param maxLen Maximum length of version buffer
 * @param downloadUrl Output buffer for download URL
 * @param urlMaxLen Maximum length of URL buffer
 * @param releaseNotes Output buffer for release notes
 * @param notesMaxLen Maximum length of release notes buffer
 * @return TRUE if parsing succeeded, FALSE on error
 * Extracts tag_name, browser_download_url, and body fields from GitHub releases API
 */
BOOL ParseLatestVersionFromJson(const char* jsonResponse, char* latestVersion, size_t maxLen, 
                               char* downloadUrl, size_t urlMaxLen, char* releaseNotes, size_t notesMaxLen) {
    LOG_DEBUG("Starting to parse JSON response, extracting version information");
    
    /** Extract version from tag_name field */
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) {
        LOG_ERROR("JSON parsing failed: tag_name field not found");
        return FALSE;
    }
    
    const char* firstQuote = strchr(tagNamePos + 11, '\"');
    if (!firstQuote) return FALSE;
    
    const char* secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    size_t versionLen = secondQuote - (firstQuote + 1);
    if (versionLen >= maxLen) versionLen = maxLen - 1;
    
    strncpy(latestVersion, firstQuote + 1, versionLen);
    latestVersion[versionLen] = '\0';
    
    /** Remove 'v' or 'V' prefix from version string if present */
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, versionLen);
    }
    
    /** Extract download URL from browser_download_url field */
    const char* downloadUrlPos = strstr(jsonResponse, "\"browser_download_url\":");
    if (!downloadUrlPos) {
        LOG_ERROR("JSON parsing failed: browser_download_url field not found");
        return FALSE;
    }
    
    firstQuote = strchr(downloadUrlPos + 22, '\"');
    if (!firstQuote) return FALSE;
    
    secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    size_t urlLen = secondQuote - (firstQuote + 1);
    if (urlLen >= urlMaxLen) urlLen = urlMaxLen - 1;
    
    strncpy(downloadUrl, firstQuote + 1, urlLen);
    downloadUrl[urlLen] = '\0';
    
    /** Extract release notes from body field */
    const char* bodyPos = strstr(jsonResponse, "\"body\":");
    if (bodyPos) {
        firstQuote = strchr(bodyPos + 7, '\"');
        if (firstQuote) {
            secondQuote = firstQuote + 1;
            int escapeCount = 0;
            
            /** Find end of body field, handling escaped quotes */
            while (*secondQuote) {
                if (*secondQuote == '\\') {
                    escapeCount++;
                } else if (*secondQuote == '\"' && (escapeCount % 2 == 0)) {
                    break;
                } else if (*secondQuote != '\\') {
                    escapeCount = 0;
                }
                secondQuote++;
            }
            
            if (*secondQuote == '\"') {
                size_t notesLen = secondQuote - (firstQuote + 1);
                if (notesLen >= notesMaxLen) notesLen = notesMaxLen - 1;
                
                /** Copy and process release notes, converting common escape sequences */
                size_t writePos = 0;
                for (size_t i = 0; i < notesLen && writePos < notesMaxLen - 1; i++) {
                    char c = firstQuote[1 + i];
                    if (c == '\\' && i + 1 < notesLen) {
                        char next = firstQuote[1 + i + 1];
                        if (next == 'n') {
                            releaseNotes[writePos++] = '\r';
                            releaseNotes[writePos++] = '\n';
                            i++; // Skip the 'n'
                        } else if (next == 'r') {
                            releaseNotes[writePos++] = '\r';
                            i++; // Skip the 'r'
                        } else if (next == '\"') {
                            releaseNotes[writePos++] = '\"';
                            i++; // Skip the quote
                        } else if (next == '\\') {
                            releaseNotes[writePos++] = '\\';
                            i++; // Skip the second backslash
                        } else {
                            releaseNotes[writePos++] = c;
                        }
                    } else {
                        releaseNotes[writePos++] = c;
                    }
                }
                releaseNotes[writePos] = '\0';
                
                LOG_DEBUG("Extracted %zu bytes of release notes", writePos);
            } else {
                LOG_WARNING("Could not find closing quote for body field");
                StringCbCopyA(releaseNotes, notesMaxLen, "No release notes available.");
            }
        } else {
            LOG_WARNING("Could not find opening quote for body field");
            StringCbCopyA(releaseNotes, notesMaxLen, "No release notes available.");
        }
    } else {
        LOG_WARNING("body field not found in JSON response");
        StringCbCopyA(releaseNotes, notesMaxLen, "No release notes available.");
    }
    
    return TRUE;
}

/**
 * @brief Dialog procedure for application exit notification
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter
 * @return Message processing result
 * Shows confirmation dialog before application exits for update
 */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            ApplyDialogLanguage(hwndDlg, IDD_EXIT_DIALOG);
            
            const wchar_t* exitText = GetLocalizedString(L"程序即将退出", L"The application will exit now");
            
            SetDlgItemTextW(hwndDlg, IDC_EXIT_TEXT, exitText);
            
            const wchar_t* okText = GetLocalizedString(L"确定", L"OK");
            SetDlgItemTextW(hwndDlg, IDOK, okText);
            
            const wchar_t* titleText = GetLocalizedString(L"Catime - 更新提示", L"Catime - Update Notice");
            SetWindowTextW(hwndDlg, titleText);
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

/**
 * @brief Display exit notification dialog to user
 * @param hwnd Parent window handle
 * Shows modal dialog informing user that application will exit for update
 */
void ShowExitMessageDialog(HWND hwnd) {
    DialogBoxW(GetModuleHandle(NULL), 
              MAKEINTRESOURCEW(IDD_EXIT_DIALOG), 
              hwnd, 
              ExitMsgDlgProc);
}

/**
 * @brief Dialog procedure for update available notification
 * @param hwndDlg Dialog window handle
 * @param msg Window message
 * @param wParam Message parameter
 * @param lParam Message parameter containing UpdateVersionInfo pointer
 * @return Message processing result
 * Displays current vs. new version and asks user whether to update
 */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UpdateVersionInfo* versionInfo = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            ApplyDialogLanguage(hwndDlg, IDD_UPDATE_DIALOG);
            
            versionInfo = (UpdateVersionInfo*)lParam;
            
            if (versionInfo) {
                wchar_t currentVersionW[64] = {0};
                wchar_t newVersionW[64] = {0};
                
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->currentVersion, -1, 
                                   currentVersionW, sizeof(currentVersionW)/sizeof(wchar_t));
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->latestVersion, -1, 
                                   newVersionW, sizeof(newVersionW)/sizeof(wchar_t));
                
                wchar_t displayText[256];
                
                const wchar_t* currentVersionText = GetLocalizedString(L"当前版本:", L"Current version:");
                const wchar_t* newVersionText = GetLocalizedString(L"新版本:", L"New version:");

                StringCbPrintfW(displayText, sizeof(displayText),
                              L"%s %s\n%s %s",
                              currentVersionText, currentVersionW,
                              newVersionText, newVersionW);
                
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                
                /** Convert and display release notes */
                if (versionInfo->releaseNotes && strlen(versionInfo->releaseNotes) > 0) {
                    int releaseNotesLen = MultiByteToWideChar(CP_UTF8, 0, versionInfo->releaseNotes, -1, NULL, 0);
                    if (releaseNotesLen > 0) {
                        wchar_t* releaseNotesW = (wchar_t*)malloc(releaseNotesLen * sizeof(wchar_t));
                        if (releaseNotesW) {
                            MultiByteToWideChar(CP_UTF8, 0, versionInfo->releaseNotes, -1, releaseNotesW, releaseNotesLen);
                            SetDlgItemTextW(hwndDlg, IDC_UPDATE_NOTES, releaseNotesW);
                            free(releaseNotesW);
                        }
                    }
                } else {
                    const wchar_t* noNotesText = GetLocalizedString(L"暂无更新说明", L"No release notes available.");
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_NOTES, noNotesText);
                }
                
                const wchar_t* yesText = GetLocalizedString(L"立即更新", L"Update Now");
                const wchar_t* noText = GetLocalizedString(L"稍后更新", L"Later");
                
                SetDlgItemTextW(hwndDlg, IDYES, yesText);
                SetDlgItemTextW(hwndDlg, IDNO, noText);
                
                const wchar_t* titleText = GetLocalizedString(L"发现新版本", L"Update Available");
                SetWindowTextW(hwndDlg, titleText);
                
                const wchar_t* exitText = GetLocalizedString(L"点击\"立即更新\"将打开浏览器下载新版本", L"Click 'Update Now' to open browser and download the new version");
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT, exitText);
                
                ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_HIDE);
            }
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
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

/**
 * @brief Show update notification dialog with version comparison and release notes
 * @param hwnd Parent window handle
 * @param currentVersion Current application version string
 * @param latestVersion Latest available version from GitHub
 * @param downloadUrl Download URL for the latest version
 * @param releaseNotes Release notes/changelog from GitHub
 * @return Dialog result (IDYES if user wants to update, IDNO otherwise)
 * Creates modal dialog showing version information, release notes and update prompt
 */
int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion, const char* downloadUrl, const char* releaseNotes) {
    UpdateVersionInfo versionInfo;
    versionInfo.currentVersion = currentVersion;
    versionInfo.latestVersion = latestVersion;
    versionInfo.downloadUrl = downloadUrl;
    versionInfo.releaseNotes = releaseNotes;
    
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_UPDATE_DIALOG), 
                          hwnd, 
                          UpdateDlgProc, 
                          (LPARAM)&versionInfo);
}

INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            const wchar_t* errorMsg = (const wchar_t*)lParam;
            if (errorMsg) {
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_ERROR_TEXT, errorMsg);
            }
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
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

void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    DialogBoxParamW(GetModuleHandle(NULL), 
                   MAKEINTRESOURCEW(IDD_UPDATE_ERROR_DIALOG), 
                   hwnd, 
                   UpdateErrorDlgProc, 
                   (LPARAM)errorMsg);
}

INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            ApplyDialogLanguage(hwndDlg, IDD_NO_UPDATE_DIALOG);
            
            const char* currentVersion = (const char*)lParam;
            if (currentVersion) {
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) {
                    baseText = L"You are already using the latest version!";
                }
                
                const wchar_t* versionText = GetLocalizedString(L"当前版本:", L"Current version:");
                
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage),
                        L"%s\n%s %hs", baseText, versionText, currentVersion);
                
                SetDlgItemTextW(hwndDlg, IDC_NO_UPDATE_TEXT, fullMessage);
            }
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
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

void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    DialogBoxParamW(GetModuleHandle(NULL), 
                   MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG), 
                   hwnd, 
                   NoUpdateDlgProc, 
                   (LPARAM)currentVersion);
}

/**
 * @brief Open browser to download update and initiate application exit
 * @param url Download URL for the latest version
 * @param hwnd Parent window handle for error dialogs
 * @return TRUE if browser opened successfully, FALSE on error
 * Launches default browser with download URL and posts exit message to application
 */
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd) {
    wchar_t urlW[512];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, urlW, sizeof(urlW)/sizeof(wchar_t));
    
    HINSTANCE hInstance = ShellExecuteW(hwnd, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
    
    if ((INT_PTR)hInstance <= 32) {
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"));
        return FALSE;
    }
    
    LOG_INFO("Successfully opened browser, preparing to exit program");
    
    wchar_t message[512];
    StringCbPrintfW(message, sizeof(message),
            L"即将退出程序");
    
    LOG_INFO("Sending exit message to main window");
    ShowExitMessageDialog(hwnd);
    
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief Main update checking implementation with GitHub API integration
 * @param hwnd Parent window handle for dialogs
 * @param silentCheck TRUE for background checks, FALSE for user-initiated checks
 * Downloads latest release info from GitHub API, compares versions, and shows appropriate dialogs
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Starting update check process, silent mode: %s", silentCheck ? "yes" : "no");
    
    /** Initialize WinINet session for HTTP requests to GitHub API */
    LOG_INFO("Attempting to create Internet session");
    wchar_t wUserAgent[256];
    MultiByteToWideChar(CP_UTF8, 0, USER_AGENT, -1, wUserAgent, 256);
    
    HINTERNET hInternet = InternetOpenW(wUserAgent, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("Failed to create Internet session, error code: %lu, error message: %s", errorCode, errorMsg);
        
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"));
        }
        return;
    }
    LOG_INFO("Internet session created successfully");
    
    /** Connect to GitHub API endpoint with cache-bypass flags */
    LOG_INFO("Attempting to connect to GitHub API: %s", GITHUB_API_URL);
    wchar_t wGitHubApiUrl[512];
    MultiByteToWideChar(CP_UTF8, 0, GITHUB_API_URL, -1, wGitHubApiUrl, 512);
    
    HINTERNET hConnect = InternetOpenUrlW(hInternet, wGitHubApiUrl, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("Failed to connect to GitHub API, error code: %lu, error message: %s", errorCode, errorMsg);
        
        InternetCloseHandle(hInternet);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"));
        }
        return;
    }
    LOG_INFO("Successfully connected to GitHub API");
    
    LOG_INFO("Allocating memory buffer for API response");
    char* buffer = (char*)malloc(8192);
    if (!buffer) {
        LOG_ERROR("Memory allocation failed, could not allocate buffer for API response");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    LOG_INFO("Starting to read response data from API");
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    size_t bufferSize = 8192;
    
    while (InternetReadFile(hConnect, buffer + totalBytes, 
                          bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        LOG_DEBUG("Read %lu bytes of data, accumulated %lu bytes", bytesRead, totalBytes + bytesRead);
        totalBytes += bytesRead;
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) {
                LOG_ERROR("Failed to reallocate buffer, current size: %zu bytes", bufferSize);
                free(buffer);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return;
            }
            LOG_DEBUG("Buffer expanded, new size: %zu bytes", newSize);
            buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    buffer[totalBytes] = '\0';
    LOG_INFO("Successfully read API response, total %lu bytes of data", totalBytes);
    
    LOG_INFO("Closing Internet connection");
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    LOG_INFO("Starting to parse API response, extracting version info, download URL and release notes");
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};
    char releaseNotes[4096] = {0};
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                  downloadUrl, sizeof(downloadUrl), releaseNotes, sizeof(releaseNotes))) {
        LOG_ERROR("Failed to parse version information, response may not be valid JSON format");
        free(buffer);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"));
        }
        return;
    }
    LOG_INFO("Successfully parsed version information, GitHub latest version: %s, download URL: %s", latestVersion, downloadUrl);
    
    free(buffer);
    
    const char* currentVersion = CATIME_VERSION;
    LOG_INFO("Current application version: %s", currentVersion);
    
    LOG_INFO("Comparing version numbers: current version %s vs. latest version %s", currentVersion, latestVersion);
    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        LOG_INFO("New version found! Current: %s, Available update: %s", currentVersion, latestVersion);
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl, releaseNotes);
        LOG_INFO("Update prompt dialog result: %s", response == IDYES ? "User agreed to update" : "User declined update");
        
        if (response == IDYES) {
            LOG_INFO("User chose to update now, preparing to open browser and exit program");
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else if (!silentCheck) {
        LOG_INFO("Current version %s is already the latest, no update needed", currentVersion);
        
        ShowNoUpdateDialog(hwnd, currentVersion);
    } else {
        LOG_INFO("Silent check mode: Current version %s is already the latest, no prompt shown", currentVersion);
    }
    
    LOG_INFO("Update check process complete");
}

/**
 * @brief Check for application updates with user interaction
 * @param hwnd Parent window handle for dialogs
 * Performs update check and shows dialogs regardless of result (update available or up-to-date)
 */
void CheckForUpdate(HWND hwnd) {
    CheckForUpdateInternal(hwnd, FALSE);
}

/**
 * @brief Check for updates with optional silent mode
 * @param hwnd Parent window handle for dialogs
 * @param silentCheck TRUE to suppress "no update" dialogs, FALSE for full interaction
 * Allows background update checking without disturbing user when already up-to-date
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    CheckForUpdateInternal(hwnd, silentCheck);
}