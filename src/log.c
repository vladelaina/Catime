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
#include "../include/log.h"
#include "../include/config.h"
#include "../resource/resource.h"

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
        _snprintf_s(logPath + dirLen, size - dirLen, _TRUNCATE, "catime.log");
    } else {
        // 如果无法确定配置目录，使用当前目录
        _snprintf_s(logPath, size, _TRUNCATE, "catime.log");
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
    WriteLog(LOG_LEVEL_INFO, "Catime 版本: %s", CATIME_VERSION);
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
