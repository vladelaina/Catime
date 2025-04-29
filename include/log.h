/**
 * @file log.h
 * @brief 日志记录功能头文件
 * 
 * 提供日志记录功能，记录应用程序错误和关键事件信息
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <windows.h>
#include <signal.h>

// 日志级别枚举
typedef enum {
    LOG_LEVEL_DEBUG,   // 调试信息
    LOG_LEVEL_INFO,    // 一般信息
    LOG_LEVEL_WARNING, // 警告信息
    LOG_LEVEL_ERROR,   // 错误信息
    LOG_LEVEL_FATAL    // 致命错误
} LogLevel;

/**
 * @brief 初始化日志系统
 * 
 * 设置日志文件路径，根据应用配置文件所在目录自动创建日志目录
 * 
 * @return BOOL 初始化是否成功
 */
BOOL InitializeLogSystem(void);

/**
 * @brief 写入日志
 * 
 * 根据指定的日志级别写入日志信息
 * 
 * @param level 日志级别
 * @param format 格式化字符串
 * @param ... 可变参数列表
 */
void WriteLog(LogLevel level, const char* format, ...);

/**
 * @brief 获取最后一个Windows错误的描述
 * 
 * @param errorCode Windows错误代码
 * @param buffer 用于存储错误描述的缓冲区
 * @param bufferSize 缓冲区大小
 */
void GetLastErrorDescription(DWORD errorCode, char* buffer, int bufferSize);

/**
 * @brief 设置全局异常处理器
 * 
 * 捕获标准C信号(SIGFPE, SIGILL, SIGSEGV等)并处理
 */
void SetupExceptionHandler(void);

/**
 * @brief 清理日志系统资源
 * 
 * 程序正常退出时关闭日志文件并释放相关资源
 */
void CleanupLogSystem(void);

// 宏定义，便于调用
#define LOG_DEBUG(format, ...) WriteLog(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) WriteLog(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define LOG_WARNING(format, ...) WriteLog(LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) WriteLog(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) WriteLog(LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

// 带错误代码的日志宏
#define LOG_WINDOWS_ERROR(format, ...) do { \
    DWORD errorCode = GetLastError(); \
    char errorMessage[256] = {0}; \
    GetLastErrorDescription(errorCode, errorMessage, sizeof(errorMessage)); \
    WriteLog(LOG_LEVEL_ERROR, format " (错误码: %lu, 描述: %s)", ##__VA_ARGS__, errorCode, errorMessage); \
} while(0)

#endif // LOG_H
