/**
 * @file update_checker.c
 * @brief 极简的应用程序更新检查功能实现
 * 
 * 本文件实现了检查版本、打开浏览器下载和删除配置文件的功能。
 */

#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <shlobj.h>
#include "../include/update_checker.h"
#include "../include/language.h"
#include "../resource/resource.h"

#pragma comment(lib, "wininet.lib")

// 更新源URL
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

/**
 * @brief 比较版本号
 * @param version1 第一个版本号字符串
 * @param version2 第二个版本号字符串
 * @return 如果version1 > version2返回1，如果相等返回0，如果version1 < version2返回-1
 */
int CompareVersions(const char* version1, const char* version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    // 解析版本号
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    // 比较主版本号
    if (major1 > major2) return 1;
    if (major1 < major2) return -1;
    
    // 比较次版本号
    if (minor1 > minor2) return 1;
    if (minor1 < minor2) return -1;
    
    // 比较修订版本号
    if (patch1 > patch2) return 1;
    if (patch1 < patch2) return -1;
    
    return 0;
}

/**
 * @brief 解析JSON响应获取最新版本号和下载URL
 */
BOOL ParseLatestVersionFromJson(const char* jsonResponse, char* latestVersion, size_t maxLen, 
                               char* downloadUrl, size_t urlMaxLen) {
    // 查找版本号
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) return FALSE;
    
    const char* firstQuote = strchr(tagNamePos + 11, '\"');
    if (!firstQuote) return FALSE;
    
    const char* secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // 复制版本号
    size_t versionLen = secondQuote - (firstQuote + 1);
    if (versionLen >= maxLen) versionLen = maxLen - 1;
    
    strncpy(latestVersion, firstQuote + 1, versionLen);
    latestVersion[versionLen] = '\0';
    
    // 如果版本号以'v'开头，移除它
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, versionLen);
    }
    
    // 查找下载URL
    const char* downloadUrlPos = strstr(jsonResponse, "\"browser_download_url\":");
    if (!downloadUrlPos) return FALSE;
    
    firstQuote = strchr(downloadUrlPos + 22, '\"');
    if (!firstQuote) return FALSE;
    
    secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // 复制下载URL
    size_t urlLen = secondQuote - (firstQuote + 1);
    if (urlLen >= urlMaxLen) urlLen = urlMaxLen - 1;
    
    strncpy(downloadUrl, firstQuote + 1, urlLen);
    downloadUrl[urlLen] = '\0';
    
    return TRUE;
}

/**
 * @brief 打开浏览器下载更新并退出程序
 */
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd) {
    // 打开浏览器
    HINSTANCE hInstance = ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
    
    if ((INT_PTR)hInstance <= 32) {
        MessageBoxW(hwnd, 
                  GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"),
                  GetLocalizedString(L"更新错误", L"Update Error"),
                  MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    // 删除配置文件
    char config_path[MAX_PATH];
    extern void GetConfigPath(char* path, size_t size);
    GetConfigPath(config_path, MAX_PATH);
    DeleteFileA(config_path);
    
    // 提示用户
    MessageBoxW(hwnd,
              GetLocalizedString(L"已打开浏览器下载新版本，程序即将退出。", 
                              L"Browser opened to download new version. The application will now exit."),
              GetLocalizedString(L"更新提示", L"Update Notice"),
              MB_ICONINFORMATION | MB_OK);
    
    // 退出程序
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief 通用的更新检查函数
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        if (!silentCheck) {
            MessageBoxW(hwnd,
                      GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"),
                      GetLocalizedString(L"更新错误", L"Update Error"),
                      MB_ICONERROR | MB_OK);
        }
        return;
    }
    
    // 连接到更新API
    HINTERNET hConnect = InternetOpenUrlA(hInternet, GITHUB_API_URL, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        if (!silentCheck) {
            MessageBoxW(hwnd,
                      GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"),
                      GetLocalizedString(L"更新错误", L"Update Error"),
                      MB_ICONERROR | MB_OK);
        }
        return;
    }
    
    // 分配缓冲区
    char* buffer = (char*)malloc(8192);
    if (!buffer) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    // 读取响应
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    size_t bufferSize = 8192;
    
    while (InternetReadFile(hConnect, buffer + totalBytes, 
                          bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        totalBytes += bytesRead;
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) break;
            buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    buffer[totalBytes] = '\0';
    
    // 关闭连接
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // 解析版本和下载URL
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                  downloadUrl, sizeof(downloadUrl))) {
        free(buffer);
        if (!silentCheck) {
            MessageBoxW(hwnd,
                      GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"),
                      GetLocalizedString(L"更新错误", L"Update Error"),
                      MB_ICONERROR | MB_OK);
        }
        return;
    }
    
    free(buffer);
    
    // 获取当前版本
    const char* currentVersion = CATIME_VERSION;
    
    // 比较版本
    if (CompareVersions(latestVersion, currentVersion) > 0) {
        // 有新版本
        wchar_t msgBuffer[512];
        swprintf(msgBuffer, sizeof(msgBuffer)/sizeof(wchar_t),
                GetLocalizedString(
                    L"发现新版本！\n当前版本: %S\n最新版本: %S\n\n是否立即打开浏览器下载新版本？",
                    L"New version available!\nCurrent version: %S\nLatest version: %S\n\nOpen browser to download new version now?"),
                currentVersion, latestVersion);
                
        int response = MessageBoxW(hwnd, msgBuffer,
                                GetLocalizedString(L"更新可用", L"Update Available"),
                                MB_ICONINFORMATION | MB_YESNO);
        
        if (response == IDYES) {
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else if (!silentCheck) {
        // 已是最新版本
        wchar_t message[256];
        swprintf(message, sizeof(message)/sizeof(wchar_t),
                GetLocalizedString(
                    L"您的软件已是最新版本！\n当前版本: %S",
                    L"Your software is up to date!\nCurrent version: %S"),
                currentVersion);
        
        MessageBoxW(hwnd, message,
                  GetLocalizedString(L"已是最新版本", L"No Updates Available"),
                  MB_ICONINFORMATION | MB_OK);
    }
}

/**
 * @brief 检查应用程序更新
 */
void CheckForUpdate(HWND hwnd) {
    CheckForUpdateInternal(hwnd, FALSE);
}

/**
 * @brief 静默检查应用程序更新
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    CheckForUpdateInternal(hwnd, silentCheck);
} 
