/**
 * @file update_checker.c
 * @brief 应用程序更新检查功能实现
 * 
 * 本文件实现了应用程序检查更新和下载更新的功能。
 */

#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <shlobj.h>
#include <direct.h>
#include <io.h>  // 添加对_access的支持
#include "../include/update_checker.h"
#include "../include/language.h"
#include "../resource/resource.h"

#pragma comment(lib, "wininet.lib")

// 更新源URL
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

// 下载配置
#define CONNECTION_TEST_TIMEOUT 3000 // 连接测试超时时间(3秒)

/**
 * @brief 检查与API的连接速度
 * @param apiUrl API的URL
 * @return 连接时间(毫秒)，如果连接失败则返回MAXDWORD
 */
DWORD CheckConnectionSpeed(const char* apiUrl) {
    DWORD startTime = GetTickCount();
    DWORD endTime;
    BOOL result = FALSE;
    
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        // 设置连接超时
        DWORD timeout = CONNECTION_TEST_TIMEOUT;
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        
        // 尝试连接
        HINTERNET hConnect = InternetOpenUrlA(hInternet, apiUrl, NULL, 0, 
                                            INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (hConnect) {
            // 读取一小段数据以确认连接有效
            char buffer[1024];
            DWORD bytesRead = 0;
            result = InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead);
            
            InternetCloseHandle(hConnect);
        }
        
        InternetCloseHandle(hInternet);
    }
    
    endTime = GetTickCount();
    
    // 如果连接失败，返回最大值表示无效
    if (!result) {
        return MAXDWORD;
    }
    
    // 返回连接所花费的时间
    return (endTime - startTime);
}

/**
 * @brief 选择更新源
 * @param apiUrl 用于存储选择的API URL的缓冲区
 * @param maxLen 缓冲区的最大长度
 * @return 是否成功选择了更新源
 */
BOOL SelectFastestUpdateSource(char* apiUrl, size_t maxLen) {
    // 检查GitHub的连接速度
    DWORD githubSpeed = CheckConnectionSpeed(GITHUB_API_URL);
    
    // 如果连接失败，返回失败
    if (githubSpeed == MAXDWORD) {
        return FALSE;
    }
    
    // 使用GitHub作为更新源
    strncpy(apiUrl, GITHUB_API_URL, maxLen);
    
    return TRUE;
}

/**
 * @brief 解析JSON响应获取最新版本号
 * @param jsonResponse GitHub/Gitee API返回的JSON响应
 * @param latestVersion 用于存储解析出的版本号的缓冲区
 * @param maxLen 缓冲区最大长度
 * @param downloadUrl 用于存储下载URL的缓冲区
 * @param urlMaxLen 下载URL缓冲区最大长度
 * @return 解析成功返回TRUE，失败返回FALSE
 */
BOOL ParseLatestVersionFromJson(const char* jsonResponse, char* latestVersion, size_t maxLen, 
                               char* downloadUrl, size_t urlMaxLen) {
    // 查找版本号 - 在"tag_name"之后
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) return FALSE;
    
    // 查找第一个引号
    const char* firstQuote = strchr(tagNamePos + 11, '\"');
    if (!firstQuote) return FALSE;
    
    // 查找第二个引号
    const char* secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // 计算版本号长度
    size_t versionLen = secondQuote - (firstQuote + 1);
    if (versionLen >= maxLen) versionLen = maxLen - 1;
    
    // 复制版本号
    strncpy(latestVersion, firstQuote + 1, versionLen);
    latestVersion[versionLen] = '\0';
    
    // 如果版本号以'v'开头，移除它
    if (latestVersion[0] == 'v' || latestVersion[0] == 'V') {
        memmove(latestVersion, latestVersion + 1, versionLen);
    }
    
    // 查找下载URL - 在"browser_download_url"之后
    const char* downloadUrlPos = strstr(jsonResponse, "\"browser_download_url\":");
    if (!downloadUrlPos) return FALSE;
    
    // 查找第一个引号
    firstQuote = strchr(downloadUrlPos + 22, '\"');
    if (!firstQuote) return FALSE;
    
    // 查找第二个引号
    secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) return FALSE;
    
    // 计算URL长度
    size_t urlLen = secondQuote - (firstQuote + 1);
    if (urlLen >= urlMaxLen) urlLen = urlMaxLen - 1;
    
    // 复制下载URL
    strncpy(downloadUrl, firstQuote + 1, urlLen);
    downloadUrl[urlLen] = '\0';
    
    return TRUE;
}

/**
 * @brief 比较版本号
 * @param version1 第一个版本号字符串
 * @param version2 第二个版本号字符串
 * @return 如果version1 > version2返回1，如果相等返回0，如果version1 < version2返回-1
 */
int CompareVersions(const char* version1, const char* version2) {
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    // 解析第一个版本号
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    
    // 解析第二个版本号
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    // 比较主版本号
    if (major1 > major2) return 1;
    if (major1 < major2) return -1;
    
    // 主版本号相等，比较次版本号
    if (minor1 > minor2) return 1;
    if (minor1 < minor2) return -1;
    
    // 次版本号也相等，比较修订版本号
    if (patch1 > patch2) return 1;
    if (patch1 < patch2) return -1;
    
    // 完全相等
    return 0;
}

/**
 * @brief 打开浏览器下载更新并退出程序
 * @param url 文件下载URL
 * @param hwnd 窗口句柄，用于显示消息和退出程序
 * @return 操作成功返回TRUE，失败返回FALSE
 */
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd) {
    // 使用ShellExecute打开浏览器到下载链接
    HINSTANCE hInstance = ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
    
    if ((INT_PTR)hInstance <= 32) {
        // 打开浏览器失败
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 删除配置文件
    char config_path[MAX_PATH];
    extern void GetConfigPath(char* path, size_t size);
    GetConfigPath(config_path, MAX_PATH);
    
    BOOL configDeleted = DeleteFileA(config_path);
    
    // 提示用户将退出程序
    wchar_t message[512];
    swprintf(message, sizeof(message)/sizeof(wchar_t),
            GetLocalizedString(
                L"即将退出程序，请从网页下载并安装新版本。\n%s",
                L"The program will now exit. Please download and install the new version from the website.\n%s"),
            configDeleted ? 
                GetLocalizedString(L"配置文件已清除，新版本将使用默认设置。", 
                                 L"Configuration file has been deleted, the new version will use default settings.") : 
                GetLocalizedString(L"无法清除配置文件，新版本可能会继承旧版本设置。", 
                                 L"Failed to delete configuration file, the new version may inherit old settings.")
    );
    
    MessageBoxW(hwnd, message, 
               GetLocalizedString(L"更新提示", L"Update Notice"), 
               MB_ICONINFORMATION);
    
    // 退出程序
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief 检查应用程序更新
 * @param hwnd 窗口句柄
 * 
 * 连接到GitHub检查是否有新版本。如果有，会提示用户是否在浏览器中下载。
 */
void CheckForUpdate(HWND hwnd) {
    // 选择更新源
    char updateApiUrl[256] = {0};
    if (!SelectFastestUpdateSource(updateApiUrl, sizeof(updateApiUrl))) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 连接到更新API
    HINTERNET hConnect = InternetOpenUrlA(hInternet, updateApiUrl, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 使用堆内存动态分配缓冲区
    char* buffer = (char*)malloc(8192);
    if (!buffer) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    size_t bufferSize = 8192;
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    
    while (InternetReadFile(hConnect, buffer + totalBytes, 
                          bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        totalBytes += bytesRead;
        // 如果缓冲区接近填满，则扩展它
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) {
                // 内存分配失败，使用现有数据
                break;
            }
            buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    if (totalBytes < bufferSize) {
        buffer[totalBytes] = '\0';
    } else {
        buffer[bufferSize - 1] = '\0';
    }
    
    // 关闭连接
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // 解析版本和下载URL
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                  downloadUrl, sizeof(downloadUrl))) {
        free(buffer);  // 释放缓冲区
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 缓冲区已不再需要
    free(buffer);
    
    // 比较版本
    if (CompareVersions(latestVersion, CATIME_VERSION) > 0) {
        // 有新版本可用
        wchar_t message[256];
        wchar_t latestVersionW[32];
        wchar_t currentVersionW[32];
        
        MultiByteToWideChar(CP_ACP, 0, latestVersion, -1, latestVersionW, 32);
        MultiByteToWideChar(CP_ACP, 0, CATIME_VERSION, -1, currentVersionW, 32);
        
        swprintf(message, 256, 
                GetLocalizedString(
                    L"当前版本: %ls\n新版本: %ls\n\n是否在浏览器中打开下载页面并退出程序?",
                    L"Current version: %ls\nNew version: %ls\n\nOpen download page in browser and exit the program?"
                ), currentVersionW, latestVersionW);
        
        int result = MessageBoxW(hwnd, message, 
                               GetLocalizedString(L"更新可用", L"Update Available"), 
                               MB_YESNO | MB_ICONINFORMATION);
        
        if (result == IDYES) {
            // 在浏览器中打开下载链接并退出程序
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else {
        // 已经是最新版本
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"您已经使用的是最新版本!", L"You are already using the latest version!"), 
                   GetLocalizedString(L"无需更新", L"No Update Needed"), 
                   MB_ICONINFORMATION);
    }
    
    // 强制执行垃圾回收
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
}

/**
 * @brief 静默检查应用程序更新
 * @param hwnd 窗口句柄
 * @param silentCheck 是否仅在有更新时显示提示
 * 
 * 连接到GitHub检查是否有新版本。
 * 如果silentCheck为TRUE，仅在有更新时才显示提示；
 * 如果为FALSE，则无论是否有更新都会显示结果。
 */
void CheckForUpdateSilent(HWND hwnd, BOOL silentCheck) {
    char apiUrl[256] = {0};  // 减小缓冲区大小
    
    // 选择更新源
    if (!SelectFastestUpdateSource(apiUrl, sizeof(apiUrl) - 1)) {
        if (!silentCheck) {
            MessageBoxW(hwnd, 
                       GetLocalizedString(L"无法连接到更新服务器", L"Cannot connect to update server"), 
                       GetLocalizedString(L"更新检查", L"Update Check"), 
                       MB_ICONINFORMATION);
        }
        return;
    }
    
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        if (!silentCheck) {
            MessageBoxW(hwnd, 
                       GetLocalizedString(L"无法初始化网络连接", L"Failed to initialize network connection"), 
                       GetLocalizedString(L"更新检查", L"Update Check"), 
                       MB_ICONERROR);
        }
        return;
    }
    
    // 设置连接和接收超时
    DWORD timeout = 10000;  // 10秒
    InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    
    HINTERNET hConnect = InternetOpenUrlA(hInternet, apiUrl, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        if (!silentCheck) {
            MessageBoxW(hwnd, 
                       GetLocalizedString(L"无法连接到更新服务器", L"Cannot connect to update server"), 
                       GetLocalizedString(L"更新检查", L"Update Check"), 
                       MB_ICONINFORMATION);
        }
        return;
    }
    
    // 使用堆内存而不是栈内存来存储响应，并使用更小的初始缓冲区
    char* buffer = (char*)malloc(8192);  // 8KB 缓冲区
    if (!buffer) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    size_t bufferSize = 8192;
    DWORD bytesRead = 0;
    DWORD totalBytesRead = 0;
    BOOL readSuccess = FALSE;
    
    while (InternetReadFile(hConnect, buffer + totalBytesRead, 
                          bufferSize - totalBytesRead - 1, &bytesRead) && bytesRead > 0) {
        totalBytesRead += bytesRead;
        
        // 如果缓冲区即将用完，扩展它
        if (totalBytesRead >= bufferSize - 1) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) {
                // 内存分配失败，使用已有数据
                break;
            }
            buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    if (totalBytesRead < bufferSize) {
        buffer[totalBytesRead] = '\0';
    } else {
        buffer[bufferSize - 1] = '\0';
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    if (totalBytesRead == 0) {
        free(buffer);  // 释放缓冲区
        if (!silentCheck) {
            MessageBoxW(hwnd, 
                       GetLocalizedString(L"无法获取版本信息", L"Failed to get version information"), 
                       GetLocalizedString(L"更新检查", L"Update Check"), 
                       MB_ICONINFORMATION);
        }
        return;
    }
    
    // 解析最新版本号和下载链接
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};  // 减小下载URL缓冲区大小
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                   downloadUrl, sizeof(downloadUrl))) {
        free(buffer);  // 释放缓冲区
        if (!silentCheck) {
            MessageBoxW(hwnd, 
                       GetLocalizedString(L"无法解析版本信息", L"Failed to parse version information"), 
                       GetLocalizedString(L"更新检查", L"Update Check"), 
                       MB_ICONINFORMATION);
        }
        return;
    }
    
    // 缓冲区内容已提取，可以释放了
    free(buffer);
    
    // 获取当前版本号（从resource.h中定义）
    const char* currentVersion = CATIME_VERSION;
    
    // 比较版本号
    int compareResult = CompareVersions(latestVersion, currentVersion);
    
    if (compareResult > 0) {
        // 有新版本可用
        wchar_t message[256];  // 减小消息缓冲区大小
        swprintf(message, sizeof(message)/sizeof(wchar_t),
                GetLocalizedString(
                    L"当前版本: %S\n新版本: %S\n\n是否前往下载页面并退出程序?",
                    L"Current version: %S\nNew version: %S\n\nDo you want to go to download page and exit the program?"),
                currentVersion, latestVersion);
        
        int response = MessageBoxW(hwnd, message, 
                                  GetLocalizedString(L"发现更新", L"Update Available"), 
                                  MB_YESNO | MB_ICONINFORMATION);
        
        if (response == IDYES) {
            // 在浏览器中打开下载链接并退出程序
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else if (!silentCheck) {
        // 如果没有新版本且不是静默检查，则显示已是最新版本的消息
        wchar_t message[256];
        swprintf(message, sizeof(message)/sizeof(wchar_t),
                GetLocalizedString(
                    L"您的软件已是最新版本！\n当前版本: %S",
                    L"Your software is up to date!\nCurrent version: %S"),
                currentVersion);
        
        MessageBoxW(hwnd, message, 
                   GetLocalizedString(L"检查更新", L"Update Check"), 
                   MB_ICONINFORMATION);
    }
    
    // 强制执行垃圾回收
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
} 
