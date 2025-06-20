/**
 * @file update_checker.c
 * @brief 极简的应用程序更新检查功能实现
 * 
 * 本文件实现了检查版本、打开浏览器下载和删除配置文件的功能。
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

// 更新源URL
#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

// 添加版本信息结构体定义
typedef struct {
    const char* currentVersion;
    const char* latestVersion;
    const char* downloadUrl;
} UpdateVersionInfo;

// 函数声明
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @brief 比较版本号
 * @param version1 第一个版本号字符串
 * @param version2 第二个版本号字符串
 * @return 如果version1 > version2返回1，如果相等返回0，如果version1 < version2返回-1
 */
int CompareVersions(const char* version1, const char* version2) {
    LOG_DEBUG("比较版本: '%s' vs '%s'", version1, version2);
    
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    // 解析版本号
    sscanf(version1, "%d.%d.%d", &major1, &minor1, &patch1);
    sscanf(version2, "%d.%d.%d", &major2, &minor2, &patch2);
    
    LOG_DEBUG("解析版本1: %d.%d.%d, 版本2: %d.%d.%d", major1, minor1, patch1, major2, minor2, patch2);
    
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
    LOG_DEBUG("开始解析JSON响应，提取版本信息");
    
    // 查找版本号
    const char* tagNamePos = strstr(jsonResponse, "\"tag_name\":");
    if (!tagNamePos) {
        LOG_ERROR("JSON解析失败：找不到tag_name字段");
        return FALSE;
    }
    
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
    if (!downloadUrlPos) {
        LOG_ERROR("JSON解析失败：找不到browser_download_url字段");
        return FALSE;
    }
    
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
 * @brief 退出消息对话框处理过程
 */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 应用对话框多语言支持
            ApplyDialogLanguage(hwndDlg, IDD_UPDATE_DIALOG);
            
            // 获取本地化的退出文本
            const wchar_t* exitText = GetLocalizedString(L"程序即将退出", L"The application will exit now");
            
            // 设置对话框文本
            SetDlgItemTextW(hwndDlg, IDC_UPDATE_EXIT_TEXT, exitText);
            SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, L"");  // 清空版本文本
            
            // 设置确定按钮文本
            const wchar_t* okText = GetLocalizedString(L"确定", L"OK");
            SetDlgItemTextW(hwndDlg, IDOK, okText);
            
            // 隐藏是否按钮，只显示确定按钮
            ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_HIDE);
            ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_HIDE);
            ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_SHOW);
            
            // 设置对话框标题
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
 * @brief 显示自定义的退出消息对话框
 */
void ShowExitMessageDialog(HWND hwnd) {
    DialogBox(GetModuleHandle(NULL), 
             MAKEINTRESOURCE(IDD_UPDATE_DIALOG), 
             hwnd, 
             ExitMsgDlgProc);
}

/**
 * @brief 更新对话框处理过程
 */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UpdateVersionInfo* versionInfo = NULL;
    
    switch (msg) {
        case WM_INITDIALOG: {
            // 应用对话框多语言支持
            ApplyDialogLanguage(hwndDlg, IDD_UPDATE_DIALOG);
            
            // 保存版本信息
            versionInfo = (UpdateVersionInfo*)lParam;
            
            // 格式化显示文本
            if (versionInfo) {
                // 将ASCII版本号转换为Unicode版本
                wchar_t currentVersionW[64] = {0};
                wchar_t newVersionW[64] = {0};
                
                // 转换版本号为宽字符
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->currentVersion, -1, 
                                   currentVersionW, sizeof(currentVersionW)/sizeof(wchar_t));
                MultiByteToWideChar(CP_UTF8, 0, versionInfo->latestVersion, -1, 
                                   newVersionW, sizeof(newVersionW)/sizeof(wchar_t));
                
                // 直接使用格式化好的字符串，而不是尝试自己格式化
                wchar_t displayText[256];
                
                // 获取本地化的版本文本（已格式化好的）
                const wchar_t* currentVersionText = GetLocalizedString(L"当前版本:", L"Current version:");
                const wchar_t* newVersionText = GetLocalizedString(L"新版本:", L"New version:");

                // 手动构建格式化字符串
                StringCbPrintfW(displayText, sizeof(displayText),
                              L"%s %s\n%s %s",
                              currentVersionText, currentVersionW,
                              newVersionText, newVersionW);
                
                // 设置对话框文本
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                
                // 设置按钮文本
                const wchar_t* yesText = GetLocalizedString(L"是", L"Yes");
                const wchar_t* noText = GetLocalizedString(L"否", L"No");
                
                // 明确设置按钮文本，而不是依赖对话框资源
                SetDlgItemTextW(hwndDlg, IDYES, yesText);
                SetDlgItemTextW(hwndDlg, IDNO, noText);
                
                // 设置对话框标题
                const wchar_t* titleText = GetLocalizedString(L"发现新版本", L"Update Available");
                SetWindowTextW(hwndDlg, titleText);
                
                // 隐藏退出文本和确定按钮，显示是/否按钮
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
 * @brief 显示更新通知对话框
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
 * @brief 更新错误对话框处理过程
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
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

/**
 * @brief 显示更新错误对话框
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
 */
INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // 应用对话框多语言支持
            ApplyDialogLanguage(hwndDlg, IDD_NO_UPDATE_DIALOG);
            
            // 获取当前版本信息
            const char* currentVersion = (const char*)lParam;
            if (currentVersion) {
                // 获取本地化的基本文本
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) {
                    // 如果找不到本地化文本，使用默认文本
                    baseText = L"You are already using the latest version!";
                }
                
                // 获取本地化的"当前版本"文本
                const wchar_t* versionText = GetLocalizedString(L"当前版本:", L"Current version:");
                
                // 创建完整的消息，包含版本号
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage),
                        L"%s\n%s %hs", baseText, versionText, currentVersion);
                
                // 设置对话框文本
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
 * @brief 显示无需更新对话框
 * @param hwnd 父窗口句柄
 * @param currentVersion 当前版本号
 */
void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    DialogBoxParam(GetModuleHandle(NULL), 
                 MAKEINTRESOURCE(IDD_NO_UPDATE_DIALOG), 
                 hwnd, 
                 NoUpdateDlgProc, 
                 (LPARAM)currentVersion);
}

/**
 * @brief 打开浏览器下载更新并退出程序
 */
BOOL OpenBrowserForUpdateAndExit(const char* url, HWND hwnd) {
    // 打开浏览器
    HINSTANCE hInstance = ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
    
    if ((INT_PTR)hInstance <= 32) {
        // 打开浏览器失败
        ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法打开浏览器下载更新", L"Could not open browser to download update"));
        return FALSE;
    }
    
    LOG_INFO("成功打开浏览器，准备退出程序");
    
    // 提示用户
    wchar_t message[512];
    StringCbPrintfW(message, sizeof(message),
            L"即将退出程序");
    
    LOG_INFO("发送退出消息到主窗口");
    // 使用自定义对话框显示退出消息
    ShowExitMessageDialog(hwnd);
    
    // 退出程序
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;
}

/**
 * @brief 通用的更新检查函数
 */
void CheckForUpdateInternal(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("开始执行更新检查流程，静默模式：%s", silentCheck ? "是" : "否");
    
    // 创建Internet会话
    LOG_INFO("尝试创建Internet会话");
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("创建Internet会话失败，错误码: %lu，错误信息: %s", errorCode, errorMsg);
        
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"));
        }
        return;
    }
    LOG_INFO("Internet会话创建成功");
    
    // 连接到更新API
    LOG_INFO("尝试连接到GitHub API: %s", GITHUB_API_URL);
    HINTERNET hConnect = InternetOpenUrlA(hInternet, GITHUB_API_URL, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("连接到GitHub API失败，错误码: %lu，错误信息: %s", errorCode, errorMsg);
        
        InternetCloseHandle(hInternet);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法连接到更新服务器", L"Could not connect to update server"));
        }
        return;
    }
    LOG_INFO("成功连接到GitHub API");
    
    // 分配缓冲区
    LOG_INFO("为API响应分配内存缓冲区");
    char* buffer = (char*)malloc(8192);
    if (!buffer) {
        LOG_ERROR("内存分配失败，无法为API响应分配缓冲区");
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    
    // 读取响应
    LOG_INFO("开始从API读取响应数据");
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    size_t bufferSize = 8192;
    
    while (InternetReadFile(hConnect, buffer + totalBytes, 
                          bufferSize - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        LOG_DEBUG("读取了 %lu 字节数据，累计 %lu 字节", bytesRead, totalBytes + bytesRead);
        totalBytes += bytesRead;
        if (totalBytes >= bufferSize - 256) {
            size_t newSize = bufferSize * 2;
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) {
                // 修复：如果realloc失败，释放原始buffer并中断
                LOG_ERROR("重新分配缓冲区失败，当前大小: %zu 字节", bufferSize);
                free(buffer);
                InternetCloseHandle(hConnect);
                InternetCloseHandle(hInternet);
                return;
            }
            LOG_DEBUG("缓冲区已扩展，新大小: %zu 字节", newSize);
            buffer = newBuffer;
            bufferSize = newSize;
        }
    }
    
    buffer[totalBytes] = '\0';
    LOG_INFO("成功读取API响应，共 %lu 字节数据", totalBytes);
    
    // 关闭连接
    LOG_INFO("关闭Internet连接");
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    
    // 解析版本和下载URL
    LOG_INFO("开始解析API响应，提取版本信息和下载URL");
    char latestVersion[32] = {0};
    char downloadUrl[256] = {0};
    if (!ParseLatestVersionFromJson(buffer, latestVersion, sizeof(latestVersion), 
                                  downloadUrl, sizeof(downloadUrl))) {
        LOG_ERROR("解析版本信息失败，响应可能不是有效的JSON格式");
        free(buffer);
        if (!silentCheck) {
            ShowUpdateErrorDialog(hwnd, GetLocalizedString(L"无法解析版本信息", L"Could not parse version information"));
        }
        return;
    }
    LOG_INFO("成功解析版本信息，GitHub最新版本: %s, 下载URL: %s", latestVersion, downloadUrl);
    
    free(buffer);
    
    // 获取当前版本
    const char* currentVersion = CATIME_VERSION;
    LOG_INFO("当前应用版本: %s", currentVersion);
    
    // 比较版本
    LOG_INFO("比较版本号: 当前版本 %s vs. 最新版本 %s", currentVersion, latestVersion);
    int versionCompare = CompareVersions(latestVersion, currentVersion);
    if (versionCompare > 0) {
        // 有新版本
        LOG_INFO("发现新版本！当前: %s, 可用更新: %s", currentVersion, latestVersion);
        int response = ShowUpdateNotification(hwnd, currentVersion, latestVersion, downloadUrl);
        LOG_INFO("更新提示对话框结果: %s", response == IDYES ? "用户同意更新" : "用户拒绝更新");
        
        if (response == IDYES) {
            LOG_INFO("用户选择立即更新，准备打开浏览器并退出程序");
            OpenBrowserForUpdateAndExit(downloadUrl, hwnd);
        }
    } else if (!silentCheck) {
        // 已是最新版本
        LOG_INFO("当前已是最新版本 %s，无需更新", currentVersion);
        
        // 使用本地化字符串，而不是构建完整消息
        ShowNoUpdateDialog(hwnd, currentVersion);
    } else {
        LOG_INFO("静默检查模式：当前已是最新版本 %s，无需显示提示", currentVersion);
    }
    
    LOG_INFO("更新检查流程完成");
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
