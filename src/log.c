/**
 * @file log.c
 * @brief 日志记录功能实现
 * 
 * 实现日志记录功能，包括文件写入、错误代码获取等
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <dbghelp.h>
#include <wininet.h>
#include "../include/log.h"
#include "../include/config.h"
#include "../resource/resource.h"

// 添加对ARM64宏的检查
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

// 日志文件路径
static char LOG_FILE_PATH[MAX_PATH] = {0};
static FILE* logFile = NULL;
static CRITICAL_SECTION logCS;

// 日志级别字符串表示
static const char* LOG_LEVEL_STRINGS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/**
 * @brief 获取操作系统版本信息
 * 
 * 使用Windows API获取操作系统版本、版本号、构建等信息
 */
static void LogSystemInformation(void) {
    // 获取系统信息
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    
    // 使用RtlGetVersion更精确地获取系统版本，因为GetVersionEx在新版Windows上被改变了
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    
    DWORD major = 0, minor = 0, build = 0;
    BOOL isWorkstation = TRUE;
    BOOL isServer = FALSE;
    
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (pRtlGetVersion(&rovi) == 0) { // STATUS_SUCCESS = 0
                major = rovi.dwMajorVersion;
                minor = rovi.dwMinorVersion;
                build = rovi.dwBuildNumber;
            }
        }
    }
    
    // 如果上面的方法失败，尝试下面的方法
    if (major == 0) {
        OSVERSIONINFOEXA osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEXA));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
        
        typedef LONG (WINAPI* PRTLGETVERSION)(OSVERSIONINFOEXW*);
        PRTLGETVERSION pRtlGetVersion;
        pRtlGetVersion = (PRTLGETVERSION)GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "RtlGetVersion");
        
        if (pRtlGetVersion) {
            pRtlGetVersion((OSVERSIONINFOEXW*)&osvi);
            major = osvi.dwMajorVersion;
            minor = osvi.dwMinorVersion;
            build = osvi.dwBuildNumber;
            isWorkstation = (osvi.wProductType == VER_NT_WORKSTATION);
            isServer = !isWorkstation;
        } else {
            // 最后尝试使用GetVersionExA，虽然可能不准确
            if (GetVersionExA((OSVERSIONINFOA*)&osvi)) {
                major = osvi.dwMajorVersion;
                minor = osvi.dwMinorVersion;
                build = osvi.dwBuildNumber;
                isWorkstation = (osvi.wProductType == VER_NT_WORKSTATION);
                isServer = !isWorkstation;
            } else {
                WriteLog(LOG_LEVEL_WARNING, "无法获取操作系统版本信息");
            }
        }
    }
    
    // 检测具体的Windows版本
    const char* windowsVersion = "未知版本";
    
    // 根据版本号确定具体版本
    if (major == 10) {
        if (build >= 22000) {
            windowsVersion = "Windows 11";
        } else {
            windowsVersion = "Windows 10";
        }
    } else if (major == 6) {
        if (minor == 3) {
            windowsVersion = "Windows 8.1";
        } else if (minor == 2) {
            windowsVersion = "Windows 8";
        } else if (minor == 1) {
            windowsVersion = "Windows 7";
        } else if (minor == 0) {
            windowsVersion = "Windows Vista";
        }
    } else if (major == 5) {
        if (minor == 2) {
            windowsVersion = "Windows Server 2003";
            if (isWorkstation && si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
                windowsVersion = "Windows XP Professional x64";
            }
        } else if (minor == 1) {
            windowsVersion = "Windows XP";
        } else if (minor == 0) {
            windowsVersion = "Windows 2000";
        }
    }
    
    WriteLog(LOG_LEVEL_INFO, "操作系统: %s (%d.%d) Build %d %s", 
        windowsVersion,
        major, minor, 
        build, 
        isWorkstation ? "工作站" : "服务器");
    
    // CPU架构
    const char* arch;
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "x64 (AMD64)";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "x86 (Intel)";
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = "ARM";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = "ARM64";
            break;
        default:
            arch = "未知";
            break;
    }
    WriteLog(LOG_LEVEL_INFO, "CPU架构: %s", arch);
    
    // 系统内存信息
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        WriteLog(LOG_LEVEL_INFO, "物理内存: %.2f GB / %.2f GB (已用 %d%%)", 
            (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024.0 * 1024 * 1024), 
            memInfo.ullTotalPhys / (1024.0 * 1024 * 1024),
            memInfo.dwMemoryLoad);
    }
    
    // 不获取屏幕分辨率信息，因为这些信息不准确且对于调试并非必要
    
    // 检查是否启用了UAC
    BOOL uacEnabled = FALSE;
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION_TYPE elevationType;
        DWORD dwSize;
        if (GetTokenInformation(hToken, TokenElevationType, &elevationType, sizeof(elevationType), &dwSize)) {
            uacEnabled = (elevationType != TokenElevationTypeDefault);
        }
        CloseHandle(hToken);
    }
    WriteLog(LOG_LEVEL_INFO, "UAC状态: %s", uacEnabled ? "已启用" : "未启用");
    
    // 检查是否以管理员运行
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup)) {
        if (CheckTokenMembership(NULL, AdministratorsGroup, &isAdmin)) {
            WriteLog(LOG_LEVEL_INFO, "管理员权限: %s", isAdmin ? "是" : "否");
        }
        FreeSid(AdministratorsGroup);
    }
}

/**
 * @brief 获取日志文件路径
 * 
 * 基于配置文件路径，构建日志文件名
 * 
 * @param logPath 日志路径缓冲区
 * @param size 缓冲区大小
 */
static void GetLogFilePath(char* logPath, size_t size) {
    char configPath[MAX_PATH] = {0};
    
    // 获取配置文件所在目录
    GetConfigPath(configPath, MAX_PATH);
    
    // 确定配置文件目录
    char* lastSeparator = strrchr(configPath, '\\');
    if (lastSeparator) {
        size_t dirLen = lastSeparator - configPath + 1;
        
        // 复制目录部分
        strncpy(logPath, configPath, dirLen);
        
        // 使用简单的日志文件名
        _snprintf_s(logPath + dirLen, size - dirLen, _TRUNCATE, "Catime_Logs.log");
    } else {
        // 如果无法确定配置目录，使用当前目录
        _snprintf_s(logPath, size, _TRUNCATE, "Catime_Logs.log");
    }
}

BOOL InitializeLogSystem(void) {
    InitializeCriticalSection(&logCS);
    
    GetLogFilePath(LOG_FILE_PATH, MAX_PATH);
    
    // 每次启动时以写入模式打开文件，这会清空原有内容
    logFile = fopen(LOG_FILE_PATH, "w");
    if (!logFile) {
        // 创建日志文件失败
        return FALSE;
    }
    
    // 记录日志系统初始化信息
    WriteLog(LOG_LEVEL_INFO, "==================================================");
    // 首先记录软件版本
    WriteLog(LOG_LEVEL_INFO, "Catime 版本: %s", CATIME_VERSION);
    // 然后记录系统环境信息(在任何可能的错误之前)
    WriteLog(LOG_LEVEL_INFO, "-----------------系统信息-----------------");
    LogSystemInformation();
    WriteLog(LOG_LEVEL_INFO, "-----------------应用信息-----------------");
    WriteLog(LOG_LEVEL_INFO, "日志系统初始化完成，Catime 启动");
    
    return TRUE;
}

void WriteLog(LogLevel level, const char* format, ...) {
    if (!logFile) {
        return;
    }
    
    EnterCriticalSection(&logCS);
    
    // 获取当前时间
    time_t now;
    struct tm local_time;
    char timeStr[32] = {0};
    
    time(&now);
    localtime_s(&local_time, &now);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &local_time);
    
    // 写入日志头
    fprintf(logFile, "[%s] [%s] ", timeStr, LOG_LEVEL_STRINGS[level]);
    
    // 格式化并写入日志内容
    va_list args;
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    
    // 换行
    fprintf(logFile, "\n");
    
    // 立即刷新缓冲区，确保即使程序崩溃也能保存日志
    fflush(logFile);
    
    LeaveCriticalSection(&logCS);
}

void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize) {
    if (!buffer || bufferSize <= 0) {
        return;
    }
    
    LPSTR messageBuffer = NULL;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0, NULL);
    
    if (size > 0) {
        // 移除末尾的换行符
        if (size >= 2 && messageBuffer[size-2] == '\r' && messageBuffer[size-1] == '\n') {
            messageBuffer[size-2] = '\0';
        }
        
        strncpy_s(buffer, bufferSize, messageBuffer, _TRUNCATE);
        LocalFree(messageBuffer);
    } else {
        _snprintf_s(buffer, bufferSize, _TRUNCATE, "未知错误 (代码: %lu)", errorCode);
    }
}

// 信号处理函数 - 用于处理各种C标准信号
void SignalHandler(int signal) {
    char errorMsg[256] = {0};
    
    switch (signal) {
        case SIGFPE:
            strcpy_s(errorMsg, sizeof(errorMsg), "浮点数异常");
            break;
        case SIGILL:
            strcpy_s(errorMsg, sizeof(errorMsg), "非法指令");
            break;
        case SIGSEGV:
            strcpy_s(errorMsg, sizeof(errorMsg), "段错误/内存访问异常");
            break;
        case SIGTERM:
            strcpy_s(errorMsg, sizeof(errorMsg), "终止信号");
            break;
        case SIGABRT:
            strcpy_s(errorMsg, sizeof(errorMsg), "异常终止/中止");
            break;
        case SIGINT:
            strcpy_s(errorMsg, sizeof(errorMsg), "用户中断");
            break;
        default:
            strcpy_s(errorMsg, sizeof(errorMsg), "未知信号");
            break;
    }
    
    // 记录异常信息
    if (logFile) {
        fprintf(logFile, "[FATAL] 发生致命信号: %s (信号编号: %d)\n", 
                errorMsg, signal);
        fflush(logFile);
        
        // 关闭日志文件
        fclose(logFile);
        logFile = NULL;
    }
    
    // 显示错误消息框
    MessageBox(NULL, "程序发生严重错误，请查看日志文件获取详细信息。", "致命错误", MB_ICONERROR | MB_OK);
    
    // 终止程序
    exit(signal);
}

void SetupExceptionHandler(void) {
    // 设置标准C信号处理器
    signal(SIGFPE, SignalHandler);   // 浮点数异常
    signal(SIGILL, SignalHandler);   // 非法指令
    signal(SIGSEGV, SignalHandler);  // 段错误
    signal(SIGTERM, SignalHandler);  // 终止信号
    signal(SIGABRT, SignalHandler);  // 异常终止
    signal(SIGINT, SignalHandler);   // 用户中断
}

// 程序退出时调用此函数清理日志资源
void CleanupLogSystem(void) {
    if (logFile) {
        WriteLog(LOG_LEVEL_INFO, "Catime 正常退出");
        WriteLog(LOG_LEVEL_INFO, "==================================================");
        fclose(logFile);
        logFile = NULL;
    }
    
    DeleteCriticalSection(&logCS);
}
