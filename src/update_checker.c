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
#include "../include/update_checker.h"
#include "../include/language.h"
#include "../resource/resource.h"

#pragma comment(lib, "wininet.lib")

// 更新源URL
#define UPDATE_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"

// 进度条对话框布局结构，需要添加到资源文件
#define IDD_DOWNLOAD_PROGRESS 1000
#define IDC_PROGRESS_BAR 1001
#define IDC_PROGRESS_TEXT 1002

// 记录下载进度的全局变量
static BOOL g_bCancelDownload = FALSE;
static DWORD g_dwTotalSize = 0;
static DWORD g_dwDownloaded = 0;
static char g_szProgressText[256] = {0};

/**
 * @brief 解析JSON响应获取最新版本号
 * @param jsonResponse GitHub API返回的JSON响应
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
 * @brief 进度条对话框消息处理回调
 */
INT_PTR CALLBACK DownloadProgressDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG:
            // 初始化进度条
            SendDlgItemMessage(hwndDlg, IDC_PROGRESS_BAR, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            return TRUE;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDCANCEL) {
                g_bCancelDownload = TRUE;
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
            
        case WM_TIMER:
            // 更新进度条和文本
            SendDlgItemMessage(hwndDlg, IDC_PROGRESS_BAR, PBM_SETPOS, 
                              (g_dwTotalSize > 0) ? (g_dwDownloaded * 100 / g_dwTotalSize) : 0, 0);
            SetDlgItemTextA(hwndDlg, IDC_PROGRESS_TEXT, g_szProgressText);
            return TRUE;
    }
    return FALSE;
}

/**
 * @brief 下载文件到用户桌面并可选择安装更新
 * @param url 文件下载URL
 * @param fileName 保存的文件名
 * @param hwnd 窗口句柄，用于显示消息
 * @return 下载成功返回TRUE，失败返回FALSE
 */
BOOL DownloadUpdateToDesktop(const char* url, const char* fileName, HWND hwnd) {
    char desktopPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath) != S_OK) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法获取桌面路径", L"Could not get desktop path"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 构建完整的保存路径
    char filePath[MAX_PATH];
    sprintf(filePath, "%s\\%s", desktopPath, fileName);
    
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 连接到URL
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, 
                                     INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法连接到下载服务器", L"Could not connect to download server"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 获取文件大小
    char sizeBuffer[32];
    DWORD sizeBufferLength = sizeof(sizeBuffer);
    g_dwTotalSize = 0;
    
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH, sizeBuffer, &sizeBufferLength, NULL)) {
        g_dwTotalSize = atol(sizeBuffer);
    }
    
    // 创建本地文件
    HANDLE hFile = CreateFileA(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法创建本地文件", L"Could not create local file"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 重置下载状态
    g_bCancelDownload = FALSE;
    g_dwDownloaded = 0;
    
    // 创建进度条对话框作为子窗口
    HWND hProgressDlg = CreateDialog(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_DOWNLOAD_PROGRESS),
        hwnd,
        DownloadProgressDlgProc
    );
    
    if (!hProgressDlg) {
        CloseHandle(hFile);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法创建进度条窗口", L"Could not create progress window"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 设置进度条窗口标题
    SetWindowTextW(hProgressDlg, GetLocalizedString(L"下载更新中...", L"Downloading update..."));
    
    // 设置定时器更新进度
    SetTimer(hProgressDlg, 1, 100, NULL);
    
    // 显示进度条窗口
    ShowWindow(hProgressDlg, SW_SHOW);
    
    // 下载文件
    BYTE buffer[4096];
    DWORD bytesRead = 0;
    BOOL result = TRUE;
    
    while (!g_bCancelDownload && 
           InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && 
           bytesRead > 0) {
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            result = FALSE;
            break;
        }
        
        // 更新进度
        g_dwDownloaded += bytesRead;
        
        // 更新进度文本
        if (g_dwTotalSize > 0) {
            sprintf(g_szProgressText, "%s: %.1f MB / %.1f MB (%.1f%%)",
                   (CURRENT_LANGUAGE == APP_LANG_ENGLISH) ? "Downloading" : "下载中",
                   g_dwDownloaded / 1048576.0,
                   g_dwTotalSize / 1048576.0,
                   (g_dwDownloaded * 100.0) / g_dwTotalSize);
        } else {
            sprintf(g_szProgressText, "%s: %.1f MB",
                   (CURRENT_LANGUAGE == APP_LANG_ENGLISH) ? "Downloading" : "下载中",
                   g_dwDownloaded / 1048576.0);
        }
        
        // 处理Windows消息，确保UI响应
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // 清理
    KillTimer(hProgressDlg, 1);
    DestroyWindow(hProgressDlg);
    
    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    if (g_bCancelDownload) {
        // 用户取消了下载，删除不完整的文件
        DeleteFileA(filePath);
        return FALSE;
    }
    
    if (!result) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"下载更新时出错", L"Error downloading update"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return FALSE;
    }
    
    // 检查文件扩展名，确定是否为可执行文件或压缩包
    BOOL isZipFile = FALSE;
    BOOL isExeFile = FALSE;
    char* extension = strrchr(fileName, '.');
    if (extension) {
        if (_stricmp(extension, ".zip") == 0) {
            isZipFile = TRUE;
        } else if (_stricmp(extension, ".exe") == 0) {
            isExeFile = TRUE;
        }
    }
    
    // 询问用户是否立即安装新版本
    wchar_t successMsg[512];
    wchar_t filePathW[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, filePath, -1, filePathW, MAX_PATH);
    
    swprintf(successMsg, 512, 
             GetLocalizedString(
                 L"更新已下载到:\n%ls\n\n是否立即安装新版本？\n(当前程序将会关闭)", 
                 L"Update downloaded to:\n%ls\n\nInstall new version now?\n(Current program will close)"), 
             filePathW);
    
    int installNow = MessageBoxW(hwnd, successMsg, 
                               GetLocalizedString(L"更新成功", L"Update Successful"), 
                               MB_YESNO | MB_ICONQUESTION);
    
    if (installNow == IDYES) {
        if (isExeFile) {
            // 对于EXE文件，采用简单的替换方式
            char currentExePath[MAX_PATH];
            
            // 获取当前程序路径
            GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
            
            // 创建一个简单的命令行命令，使用Windows内置命令
            char cmdLine[1024];
            sprintf(cmdLine, "cmd.exe /c timeout /t 2 > nul && move /y \"%s\" \"%s\" && start \"\" \"%s\"", 
                   filePath, currentExePath, currentExePath);
            
            // 创建进程执行命令
            STARTUPINFOA si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);
            
            if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 
                              CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                // 退出当前程序
                PostMessage(hwnd, WM_CLOSE, 0, 0);
                return TRUE;
            } else {
                DWORD error = GetLastError();
                char errorMsg[256];
                sprintf(errorMsg, "启动更新进程失败，错误代码: %lu", error);
                
                wchar_t errorMsgW[256];
                MultiByteToWideChar(CP_ACP, 0, errorMsg, -1, errorMsgW, 256);
                
                MessageBoxW(hwnd, errorMsgW, 
                           GetLocalizedString(L"更新错误", L"Update Error"), 
                           MB_ICONERROR);
                return TRUE; // 仍然返回TRUE，因为下载成功了
            }
        }
        else if (isZipFile) {
            // 如果是ZIP文件，保留原来的批处理脚本处理方式
            // ... 批处理脚本处理ZIP文件的代码 ...
            
            // 创建安装批处理文件
            char batchPath[MAX_PATH];
            char currentExePath[MAX_PATH];
            char installDir[MAX_PATH];
            
            // 获取当前程序路径
            GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
            
            // 获取安装目录（当前程序所在目录）
            strcpy(installDir, currentExePath);
            char* lastSlash = strrchr(installDir, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
            }
            
            // 创建批处理文件路径（临时文件夹中）
            char tempPath[MAX_PATH];
            GetTempPathA(MAX_PATH, tempPath);
            sprintf(batchPath, "%s\\catime_update.bat", tempPath);
            
            // 创建批处理文件
            FILE* batchFile = fopen(batchPath, "w");
            if (!batchFile) {
                MessageBoxW(hwnd, 
                           GetLocalizedString(L"无法创建更新脚本", L"Could not create update script"), 
                           GetLocalizedString(L"更新错误", L"Update Error"), 
                           MB_ICONERROR);
                return TRUE; // 仍然返回TRUE，因为下载成功了
            }
            
            // 编写批处理脚本
            fprintf(batchFile, "@echo off\n");
            fprintf(batchFile, "echo 正在更新 Catime，请稍候...\n");
            fprintf(batchFile, "timeout /t 1 > nul\n");
            
            // 等待原程序退出
            fprintf(batchFile, ":wait_loop\n");
            fprintf(batchFile, "tasklist | find /i \"Catime.exe\" > nul\n");
            fprintf(batchFile, "if not errorlevel 1 (\n");
            fprintf(batchFile, "  timeout /t 1 > nul\n");
            fprintf(batchFile, "  goto wait_loop\n");
            fprintf(batchFile, ")\n");
            
            // 如果是ZIP文件，解压并运行
            // 首先创建一个唯一的临时目录
            fprintf(batchFile, "set TEMP_DIR=%%TEMP%%\\catime_update_%%RANDOM%%\n");
            fprintf(batchFile, "mkdir %%TEMP_DIR%%\n");
            
            // 解压ZIP文件到临时目录
            fprintf(batchFile, "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath %%TEMP_DIR%% -Force\"\n", filePath);
            
            // 复制解压的文件到原安装目录
            fprintf(batchFile, "xcopy /E /Y /I \"%%TEMP_DIR%%\\*\" \"%s\\\"\n", installDir);
            
            // 启动新版本
            fprintf(batchFile, "start \"\" \"%s\"\n", currentExePath);
            
            // 清理临时文件
            fprintf(batchFile, "rmdir /S /Q %%TEMP_DIR%%\n");
            
            // 删除自身
            fprintf(batchFile, "del \"%%~f0\"\n");
            fclose(batchFile);
            
            // 构建命令行来运行批处理文件
            char cmdLine[MAX_PATH + 20];
            sprintf(cmdLine, "cmd.exe /c \"%s\"", batchPath);
            
            // 创建进程运行批处理文件
            STARTUPINFOA si = {0};
            PROCESS_INFORMATION pi = {0};
            si.cb = sizeof(si);
            
            if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 
                              CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                
                // 退出当前程序
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            } else {
                MessageBoxW(hwnd, 
                           GetLocalizedString(L"无法启动更新程序", L"Could not start update program"), 
                           GetLocalizedString(L"更新错误", L"Update Error"), 
                           MB_ICONERROR);
            }
        } else {
            // 其他类型文件，打开文件位置
            ShellExecuteA(NULL, "open", desktopPath, NULL, NULL, SW_SHOW);
        }
    } else {
        // 用户选择稍后安装，只显示下载位置
        MessageBoxW(hwnd, filePathW, 
                   GetLocalizedString(L"更新文件已保存", L"Update Saved"), 
                   MB_ICONINFORMATION);
    }
    
    return TRUE;
}

/**
 * @brief 检查应用程序更新
 * @param hwnd 窗口句柄
 * 
 * 连接到GitHub检查是否有新版本。如果有，会提示用户是否下载。
 * 如果用户确认，会将新版本下载到用户桌面。
 */
void CheckForUpdate(HWND hwnd) {
    // 创建Internet会话
    HINTERNET hInternet = InternetOpenA(USER_AGENT, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法创建Internet连接", L"Could not create Internet connection"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 连接到GitHub API
    HINTERNET hConnect = InternetOpenUrlA(hInternet, UPDATE_API_URL, NULL, 0, 
                                        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法连接到GitHub服务器", L"Could not connect to GitHub server"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
    // 读取响应
    char buffer[8192] = {0};
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    while (InternetReadFile(hConnect, buffer + totalBytes, 
                          sizeof(buffer) - totalBytes - 1, &bytesRead) && bytesRead > 0) {
        totalBytes += bytesRead;
        if (totalBytes >= sizeof(buffer) - 1)
            break;
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
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"无法从GitHub获取版本信息", L"Could not get version info from GitHub"), 
                   GetLocalizedString(L"更新错误", L"Update Error"), 
                   MB_ICONERROR);
        return;
    }
    
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
                    L"发现新版本!\n当前版本: %ls\n最新版本: %ls\n\n是否下载更新?",
                    L"New version available!\nCurrent version: %ls\nLatest version: %ls\n\nDownload update?"
                ), currentVersionW, latestVersionW);
        
        int result = MessageBoxW(hwnd, message, 
                               GetLocalizedString(L"更新可用", L"Update Available"), 
                               MB_YESNO | MB_ICONINFORMATION);
        
        if (result == IDYES) {
            // 提取文件名
            const char* fileName = strrchr(downloadUrl, '/');
            if (fileName) {
                fileName++; // 跳过'/'字符
            } else {
                fileName = "Catime-latest.zip";
            }
            
            // 下载更新
            DownloadUpdateToDesktop(downloadUrl, fileName, hwnd);
        }
    } else {
        // 已经是最新版本
        MessageBoxW(hwnd, 
                   GetLocalizedString(L"您已经使用的是最新版本!", L"You are already using the latest version!"), 
                   GetLocalizedString(L"无需更新", L"No Update Needed"), 
                   MB_ICONINFORMATION);
    }
} 