/**
 * @file update_checker.c
 * @brief Minimalist application update check functionality implementation
 * 
 * This file implements functions for checking versions, opening browser for downloads, and deleting configuration files.
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
#include "../resource/resource.h"

#pragma comment(lib, "wininet.lib")

// Update source URL
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

// Version information structure definition
typedef struct {
    const char* currentVersion;
    const char* latestVersion;
    const char* downloadUrl;
} UpdateVersionInfo;

// Function declarations
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief Compare version numbers
 * @param version1 First version string
 * @param version2 Second version string
 * @return Returns 1 if version1 > version2, 0 if equal, -1 if version1 < version2
 */
int CompareVersions(const char* version1, const char* version2) {
    LOG_DEBUG("Comparing versions: '%s' vs '%s'", version1, version2);
    
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    // Parse version numbers
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    LOG_DEBUG("Parsed version1: %d.%d.%d, version2: %d.%d.%d", major1, minor1, patch1, major2, minor2, patch2);
    
    // Compare major version
    if (major1 > major2) return 1;
    if (major1 < major2) return -1;
    
    // Compare minor version
    if (minor1 > minor2) return 1;
    if (minor1 < minor2) return -1;
    
    // Compare patch version
    if (patch1 > patch2) return 1;
    if (patch1 < patch2) return -1;
    
    return 0;
}

/**
 * @brief Parse JSON response to get latest version and download URL
 */
BOOL ParseLatestVersionFromJson(const char* jsonResponse, char* latestVersion, size_t maxLen, 
                               char* downloadUrl, size_t urlMaxLen) {
    LOG_DEBUG("Starting to parse JSON response, extracting version information");
    
    // Find version number
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) {
        LOG_ERROR("JSON parsing failed: tag_name field not found");
        return FALSE;
    }
    
    const char* firstQuote = strchr(tagNamePos + 11, '\"');
    if (!firstQuote) return FALSE;
    
    const char* secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // Copy version number
    size_t versionLen = secondQuote - (firstQuote + 1);
    if (versionLen >= maxLen) versionLen = maxLen - 1;
    
    strncpy(latestVersion, firstQuote + 1, versionLen);
    latestVersion[versionLen] = '\0';
    
    // If version starts with 'v', remove it
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, versionLen);
    }
    
    // Find download URL
    const char* downloadUrlPos = strstr(jsonResponse, "\"browser_download_url\":");
    if (!downloadUrlPos) {
        LOG_ERROR("JSON parsing failed: browser_download_url field not found");
        return FALSE;
    }
    
    firstQuote = strchr(downloadUrlPos + 22, '\"');
    if (!firstQuote) return FALSE;
    
    secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // Copy download URL
    size_t urlLen = secondQuote - (firstQuote + 1);
    if (urlLen >= urlMaxLen) urlLen = urlMaxLen - 1;
    
    strncpy(downloadUrl, firstQuote + 1, urlLen);
    downloadUrl[urlLen] = '\0';
    
    return TRUE;
}

/**
 * @brief Exit message dialog procedure
 */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Apply dialog multilingual support
            ApplyDialogLanguage(hwndDlg, IDD_UPDATE_DIALOG);
            
            // Get localized exit text
            const wchar_t* exitText = GetLocalizedString(L"程序即将退出", L"The application will exit now");
            
            // Set dialog text
            SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT, exitText);
            SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, L"");  // Clear version text
            
            // Set OK button text
            const wchar_t* okText = GetLocalizedString(L"确定", L"OK");
            SetDlgItemTextW(hwndDlg, IDOK, okText);
            
            // Hide Yes/No buttons, only show OK button
            ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_HIDE);
            ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_HIDE);
            ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_SHOW);
            
            // Set dialog title
            const wchar_t* titleText = GetLocalizedString(L"Catime - 更新提示", L"Catime - Update Notice");
            SetWindowTextW(hwndDlg, titleText);
            
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
 * @brief Display custom exit message dialog
 */
void ShowExitMessageDialog(HWND hwnd) {
    DialogBoxW(GetModuleHandle(NULL), 
              MAKEINTRESOURCEW(IDD_UPDATE_DIALOG), 
              hwnd, 
              ExitMsgDlgProc);
}

/**
 * @brief Update dialog procedure
 */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UpdateVersionInfo* versionInfo = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // Apply dialog multilingual support
            ApplyDialogLanguage(hwndDlg, IDD_UPDATE_DIALOG);
            
            // Save version information
            versionInfo = (UpdateVersionInfo*)lParam;
            
            // Format display text
            if (versionInfo) {
                // Convert ASCII version numbers to Unicode
                wchar_t currentVersionW[64] = {0};
                wchar_t newVersionW[64] = {0};
                
                // Convert version numbers to wide characters
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->currentVersion, -1, 
                                   currentVersionW, sizeof(currentVersionW)/sizeof(wchar_t));
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->latestVersion, -1, 
                                   newVersionW, sizeof(newVersionW)/sizeof(wchar_t));
                
                // Use pre-formatted strings instead of trying to format ourselves
                wchar_t displayText[256];
                
                // Get localized version text (pre-formatted)
                const wchar_t* currentVersionText = GetLocalizedString(L"当前版本:", L"Current version:");
                const wchar_t* newVersionText = GetLocalizedString(L"新版本:", L"New version:");

                // Manually build formatted string
                StringCbPrintfW(displayText, sizeof(displayText),
                              L"%s %s\n%s %s",
                              currentVersionText, currentVersionW,
                              newVersionText, newVersionW);
                
                // Set dialog text
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                
                // Set button text
                const wchar_t* yesText = GetLocalizedString(L"是", L"Yes");
                const wchar_t* noText = GetLocalizedString(L"否", L"No");
                
                // Explicitly set button text, not relying on dialog resource
                SetDlgItemTextW(hwndDlg, IDYES, yesText);
                SetDlgItemTextW(hwndDlg, IDNO, noText);
                
                // Set dialog title
                const wchar_t* titleText = GetLocalizedString(L"发现新版本", L"Update Available");
                SetWindowTextW(hwndDlg, titleText);
                
                // Hide exit text and OK button, show Yes/No buttons
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT, L"");
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

/**
 * @brief Display update notification dialog
 */
int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion, const char* downloadUrl) {
    // Create version info structure
    UpdateVersionInfo versionInfo;
    versionInfo.currentVersion = currentVersion;
    versionInfo.latestVersion = latestVersion;
    versionInfo.downloadUrl = downloadUrl;
    
    // Display custom dialog
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_UPDATE_DIALOG), 
                          hwnd, 
                          UpdateDlgProc, 
                          (LPARAM)&versionInfo);
}

/**
 * @brief Update error dialog procedure
 */
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Get error message text
            const wchar_t* errorMsg = (const wchar_t*)lParam;
            if (errorMsg) {
                // Set dialog text
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_ERROR_TEXT, errorMsg);
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

/**
 * @brief Display update error dialog
 */
void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    DialogBoxParamW(GetModuleHandle(NULL), 
                   MAKEINTRESOURCEW(IDD_UPDATE_ERROR_DIALOG), 
                   hwnd, 
                   UpdateErrorDlgProc, 
                   (LPARAM)errorMsg);
}

/**
 * @brief No update required dialog procedure
 */
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Apply dialog multilingual support
            ApplyDialogLanguage(hwndDlg, IDD_NO_UPDATE_DIALOG);
            
            // Get current version information
            const char* currentVersion = (const char*)lParam;
            if (currentVersion) {
                // Get localized basic text
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) {
                    // If localized text not found, use default text
                    baseText = L"You are already using the latest version!";
                }
                
                // Get localized "Current version" text
                const wchar_t* versionText = GetLocalizedString(L"当前版本:", L"Current version:");
                
                // Create complete message including version number
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage),
                        L"%s\n%s %hs", baseText, versionText, currentVersion);
                
                // Set dialog text
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

/**
 * @brief Display no update required dialog
 * @param hwnd Parent window handle
 * @param currentVersion Current version number
 */
void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    DialogBoxParamW(GetModuleHandle(NULL), 
                   MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG), 
                   hwnd, 
                   NoUpdateDlgProc, 
                   (LPARAM)currentVersion);
}

/**
 * @brief Open browser to download update and exit program
 */
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd) {
    // Convert URL to wide string
    wchar_t urlW[512];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, urlW, sizeof(urlW)/sizeof(wchar_t));
    
    // Open browser
    HINSTANCE hInstance = ShellExecuteW(hwnd, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
    
    if ((INT_PTR)hInstance <= 32) {
        // Failed to open browser
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"));
        return FALSE;
    }
    
    LOG_INFO("Successfully opened browser, preparing to exit program");
    
    // Prompt user
    wchar_t message[512];
    StringCbPrintfW(message, sizeof(message),
            L"即将退出程序");
    
    LOG_INFO("Sending exit message to main window");
    // Use custom dialog to display exit message
    ShowExitMessageDialog(hwnd);
    
    // Exit program
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief General update check function
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("Starting update check process, silent mode: %s", silentCheck ? "yes" : "no");
    
    // Create Internet session
    LOG_INFO("Attempting to create Internet session");
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
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
    
    // Connect to update API
    LOG_INFO("Attempting to connect to GitHub API: %s", GITHUB_API_URL);
    HINTERNET hConnect = InternetOpenUrlA(hInternet, GITHUB_API_URL, NULL, 0, 
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
    
    // Allocate buffer
    LOG_INFO("Allocating memory buffer for API response");
    char* buffer = (char*)malloc(8192);
    if (!buffer) {
        LOG_ERROR("Memory allocation failed, could not allocate buffer for API response");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    // Read response
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
                // Fix: If realloc fails, free the original buffer and abort
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
    
    // Close connection
    LOG_INFO("Closing Internet connection");
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // Parse version and download URL
    LOG_INFO("Starting to parse API response, extracting version info and download URL");
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                  downloadUrl, sizeof(downloadUrl))) {
        LOG_ERROR("Failed to parse version information, response may not be valid JSON format");
        free(buffer);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"));
        }
        return;
    }
    LOG_INFO("Successfully parsed version information, GitHub latest version: %s, download URL: %s", latestVersion, downloadUrl);
    
    free(buffer);
    
    // Get current version
    const char* currentVersion = CATIME_VERSION;
    LOG_INFO("Current application version: %s", currentVersion);
    
    // Compare versions
    LOG_INFO("Comparing version numbers: current version %s vs. latest version %s", currentVersion, latestVersion);
    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        // New version available
        LOG_INFO("New version found! Current: %s, Available update: %s", currentVersion, latestVersion);
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl);
        LOG_INFO("Update prompt dialog result: %s", response == IDYES ? "User agreed to update" : "User declined update");
        
        if (response == IDYES) {
            LOG_INFO("User chose to update now, preparing to open browser and exit program");
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else if (!silentCheck) {
        // Already using latest version
        LOG_INFO("Current version %s is already the latest, no update needed", currentVersion);
        
        // Use localized strings instead of building complete message
        ShowNoUpdateDialog(hwnd, currentVersion);
    } else {
        LOG_INFO("Silent check mode: Current version %s is already the latest, no prompt shown", currentVersion);
    }
    
    LOG_INFO("Update check process complete");
}

/**
 * @brief Check for application updates
 */
void CheckForUpdate(HWND hwnd) {
    CheckForUpdateInternal(hwnd, FALSE);
}

/**
 * @brief Silently check for application updates
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    CheckForUpdateInternal(hwnd, silentCheck);
} 
