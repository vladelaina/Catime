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

// 添加版本信息结构体定义
typedef struct {
    const char* currentVersion;
    const char* latestVersion;
    const char* downloadUrl;
} UpdateVersionInfo;

// 更新源URL
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

// 下载配置
#define CONNECTION_TEST_TIMEOUT 3000 // 连接测试超时时间(3秒)

// 函数声明
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd);
void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg);
void ShowNoUpdateDialog(HWND hwnd, const wchar_t* versionInfo);
void ShowExitMessageDialog(HWND hwnd, const wchar_t* message);

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
    // 调试输出
    FILE *debugFile = fopen("update_source.log", "w");
    if (debugFile) {
        fprintf(debugFile, "Selecting update source...\n");
    }
    
    // 检查GitHub的连接速度
    DWORD githubSpeed = CheckConnectionSpeed(GITHUB_API_URL);
    
    if (debugFile) {
        fprintf(debugFile, "GitHub API URL: %s\n", GITHUB_API_URL);
        fprintf(debugFile, "GitHub connection speed: %u ms\n", githubSpeed);
    }
    
    // 如果连接失败，返回失败
    if (githubSpeed == MAXDWORD) {
        if (debugFile) {
            fprintf(debugFile, "Connection failed\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 使用GitHub作为更新源
    strncpy(apiUrl, GITHUB_API_URL, maxLen);
    
    if (debugFile) {
        fprintf(debugFile, "Selected API URL: %s\n", apiUrl);
        fclose(debugFile);
    }
    
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
    // 调试输出
    FILE *debugFile = fopen("update_debug.log", "w");
    if (debugFile) {
        fprintf(debugFile, "Parsing JSON response:\n%s\n\n", jsonResponse);
    }
    
    // 查找版本号 - 在"tag_name"之后
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find tag_name\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 查找第一个引号
    const char* firstQuote = strchr(tagNamePos + 11, '\"');
    if (!firstQuote) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find first quote in tag_name\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 查找第二个引号
    const char* secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find second quote in tag_name\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
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
    
    if (debugFile) {
        fprintf(debugFile, "Found version: %s\n", latestVersion);
    }
    
    // 查找下载URL - 在"browser_download_url"之后
    const char* downloadUrlPos = strstr(jsonResponse, "\"browser_download_url\":");
    if (!downloadUrlPos) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find browser_download_url\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 查找第一个引号
    firstQuote = strchr(downloadUrlPos + 22, '\"');
    if (!firstQuote) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find first quote in download URL\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 查找第二个引号
    secondQuote = strchr(firstQuote + 1, '\"');
    if (!secondQuote) {
        if (debugFile) {
            fprintf(debugFile, "Failed to find second quote in download URL\n");
            fclose(debugFile);
        }
        return FALSE;
    }
    
    // 计算URL长度
    size_t urlLen = secondQuote - (firstQuote + 1);
    if (urlLen >= urlMaxLen) urlLen = urlMaxLen - 1;
    
    // 复制下载URL
    strncpy(downloadUrl, firstQuote + 1, urlLen);
    downloadUrl[urlLen] = '\0';
    
    if (debugFile) {
        fprintf(debugFile, "Found download URL: %s\n", downloadUrl);
        fprintf(debugFile, "Current version: %s\n", CATIME_VERSION);
        fprintf(debugFile, "Parse succeeded\n");
        fclose(debugFile);
    }
    
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
 * @brief 退出消息对话框处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 获取消息文本
            const wchar_t* exitMsg = (const wchar_t*)lParam;
            if (exitMsg) {
                // 设置对话框文本
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT, exitMsg);
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, L"");  // 清空版本文本
                
                // 隐藏是否按钮，只显示确定按钮
                ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_HIDE);
                ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_HIDE);
                ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_SHOW);
                
                // 设置窗口标题
                SetWindowTextW(hwndDlg, GetLocalizedString(L"更新提示", L"Update Notice"));
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief 显示自定义的退出消息对话框
 * @param hwnd 父窗口句柄
 * @param message 显示的消息
 */
void ShowExitMessageDialog(HWND hwnd, const wchar_t* message) {
    DialogBoxParam(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(IDD_UPDATE_DIALOG), 
                 hwnd, 
                 ExitMsgDlgProc, 
                 (LPARAM)message);
}

/**
 * @brief 更新对话框处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UpdateVersionInfo* versionInfo = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 保存版本信息
            versionInfo = (UpdateVersionInfo*)lParam;
            
            // 格式化显示文本
            if (versionInfo) {
                wchar_t displayText[256];
                swprintf(displayText, sizeof(displayText)/sizeof(wchar_t),
                        L"当前版本: %S\n新版本: %S",
                        versionInfo->currentVersion, versionInfo->latestVersion);
                
                // 设置对话框文本
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                
                // 设置窗口标题
                SetWindowTextW(hwndDlg, GetLocalizedString(L"更新可用", L"Update Available"));
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                EndDialog(hwndDlg, LOWORD(wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief 显示更新通知对话框
 * @param hwnd 窗口句柄
 * @param currentVersion 当前版本
 * @param latestVersion 最新版本
 * @param downloadUrl 下载URL
 * @return 用户选择结果，IDYES表示用户要更新
 */
int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion, const char* downloadUrl) {
    // 创建版本信息结构体
    UpdateVersionInfo versionInfo;
    versionInfo.currentVersion = currentVersion;
    versionInfo.latestVersion = latestVersion;
    versionInfo.downloadUrl = downloadUrl;
    
    // 显示自定义对话框
    return DialogBoxParam(GetModuleHandle(NULL), 
                        MAKEINTRESOURCE(IDD_UPDATE_DIALOG), 
                        hwnd, 
                        UpdateDlgProc, 
                        (LPARAM)&versionInfo);
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
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"));
        return;
    }
    
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"));
        return;
    }
    
    // 连接到更新API
    HINTERNET hConnect = InternetOpenUrlA(hInternet, updateApiUrl, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"));
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
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"));
        return;
    }
    
    // 缓冲区已不再需要
    free(buffer);
    
    // 比较版本
    if (CompareVersions(latestVersion, CATIME_VERSION) > 0) {
        // 有新版本可用
        int result = ShowUpdateNotification(hwnd, CATIME_VERSION, latestVersion, downloadUrl);
        
        if (result == IDYES) {
            // 在浏览器中打开下载链接并退出程序
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else {
        // 已经是最新版本
        wchar_t message[256];
        swprintf(message, sizeof(message)/sizeof(wchar_t),
                GetLocalizedString(L"您已经使用的是最新版本!\n当前版本: %S", 
                                 L"You are already using the latest version!\nCurrent version: %S"), 
                CATIME_VERSION);
        ShowNoUpdateDialog(hwnd, message);
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
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Cannot connect to update server"));
        }
        return;
    }
    
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法初始化网络连接", L"Failed to initialize network connection"));
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
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Cannot connect to update server"));
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
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法获取版本信息", L"Failed to get version information"));
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
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法解析版本信息", L"Failed to parse version information"));
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
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl);
        
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
        
        ShowNoUpdateDialog(hwnd, message);
    }
    
    // 强制执行垃圾回收
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
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
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"));
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
            L"即将退出程序");
    
    // 使用自定义对话框显示退出消息
    ShowExitMessageDialog(hwnd, message);
    
    // 退出程序
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief 更新错误对话框处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 */
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 获取错误消息文本
            const wchar_t* errorMsg = (const wchar_t*)lParam;
            if (errorMsg) {
                // 设置对话框文本
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
    }
    return FALSE;
}

/**
 * @brief 显示更新错误对话框
 * @param hwnd 父窗口句柄
 * @param errorMsg 错误消息
 */
void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    DialogBoxParam(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(IDD_UPDATE_ERROR_DIALOG), 
                 hwnd, 
                 UpdateErrorDlgProc, 
                 (LPARAM)errorMsg);
}

/**
 * @brief 无需更新对话框处理过程
 * @param hwndDlg 对话框句柄
 * @param msg 消息类型
 * @param wParam 消息参数
 * @param lParam 消息参数
 * @return INT_PTR 消息处理结果
 */
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 可以接收额外信息如当前版本
            const wchar_t* versionInfo = (const wchar_t*)lParam;
            if (versionInfo) {
                // 设置对话框文本，添加当前版本信息
                SetDlgItemTextW(hwndDlg, IDC_NO_UPDATE_TEXT, versionInfo);
            }
            return TRUE;
        }
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

/**
 * @brief 显示无需更新对话框
 * @param hwnd 父窗口句柄
 * @param versionInfo 版本信息，可为NULL
 */
void ShowNoUpdateDialog(HWND hwnd, const wchar_t* versionInfo) {
    DialogBoxParam(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(IDD_NO_UPDATE_DIALOG), 
                 hwnd, 
                 NoUpdateDlgProc, 
                 (LPARAM)versionInfo);
} 
