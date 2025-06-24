/**
 * @file async_update_checker.c
 * @brief Implementation of simplified asynchronous application update checking functionality
 * 
 * This file implements the functionality for asynchronously checking for application updates,
 * ensuring that update checks do not block the main thread.
 */

#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"
#include "../include/log.h"

// Thread parameter structure
typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

// Handle for the currently running update check thread
static HANDLE g_hUpdateThread = NULL;
static BOOL g_bUpdateThreadRunning = FALSE;

/**
 * @brief Clean up update check thread resources
 * 
 * Close thread handle and release related resources.
 */
void CleanupUpdateThread() {
    LOG_INFO("清理更新检查线程资源");
    if (g_hUpdateThread != NULL) {
        LOG_INFO("等待更新检查线程结束，超时设为1秒");
        // Wait for thread to end, but no more than 1 second
        DWORD waitResult = WaitForSingleObject(g_hUpdateThread, 1000);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARNING("等待线程结束超时，强制关闭线程句柄");
        } else if (waitResult == WAIT_OBJECT_0) {
            LOG_INFO("线程已正常结束");
        } else {
            LOG_WARNING("等待线程返回意外结果：%lu", waitResult);
        }
        
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        g_bUpdateThreadRunning = FALSE;
        LOG_INFO("线程资源已清理完毕");
    } else {
        LOG_INFO("更新检查线程未运行，无需清理");
    }
}

/**
 * @brief Update check thread function
 * @param param Thread parameters (window handle)
 * 
 * Performs update check in a separate thread, without blocking the main thread.
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    LOG_INFO("更新检查线程已启动");
    
    // Parse thread parameters
    UpdateThreadParams* threadParams = (UpdateThreadParams*)param;
    if (!threadParams) {
        LOG_ERROR("线程参数为空，无法执行更新检查");
        g_bUpdateThreadRunning = FALSE;
        _endthreadex(1);
        return 1;
    }
    
    HWND hwnd = threadParams->hwnd;
    BOOL silentCheck = threadParams->silentCheck;
    
    LOG_INFO("解析线程参数成功，窗口句柄：0x%p，静默检查模式：%s", 
             hwnd, silentCheck ? "是" : "否");
    
    // Free thread parameter memory
    free(threadParams);
    LOG_INFO("释放线程参数内存");
    
    // Call the original update check function, passing the silent check parameter
    LOG_INFO("开始执行更新检查");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("更新检查完成");
    
    // Mark thread as ended
    g_bUpdateThreadRunning = FALSE;
    
    // End thread
    _endthreadex(0);
    return 0;
}

/**
 * @brief Check for application updates asynchronously
 * @param hwnd Window handle
 * @param silentCheck Whether to perform a silent check (only show prompt if updates are available)
 * 
 * Connects to GitHub in a separate thread to check for new versions.
 * If available, prompts the user whether to download in the browser.
 * This function returns immediately, without blocking the main thread.
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("异步更新检查请求，窗口句柄：0x%p，静默模式：%s", 
             hwnd, silentCheck ? "是" : "否");
    
    // If an update check thread is already running, don't start a new one
    if (g_bUpdateThreadRunning) {
        LOG_INFO("已有更新检查线程正在运行，跳过本次检查请求");
        return;
    }
    
    // Clean up previous thread handle (if any)
    if (g_hUpdateThread != NULL) {
        LOG_INFO("发现旧的线程句柄，清理中...");
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        LOG_INFO("旧线程句柄已关闭");
    }
    
    // Allocate memory for thread parameters
    LOG_INFO("为线程参数分配内存");
    UpdateThreadParams* threadParams = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!threadParams) {
        // Memory allocation failed
        LOG_ERROR("线程参数内存分配失败，无法启动更新检查线程");
        return;
    }
    
    // Set thread parameters
    threadParams->hwnd = hwnd;
    threadParams->silentCheck = silentCheck;
    LOG_INFO("线程参数设置完成");
    
    // Mark thread as about to run
    g_bUpdateThreadRunning = TRUE;
    
    LOG_INFO("准备创建更新检查线程");
    // Create thread to perform update check
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,               // Default security attributes
        0,                  // Default stack size
        UpdateCheckThreadProc, // Thread function
        threadParams,       // Thread parameters
        0,                  // Run thread immediately
        NULL                // Don't need thread ID
    );
    
    if (hThread) {
        // Save thread handle for later checking
        LOG_INFO("更新检查线程创建成功，线程句柄：0x%p", hThread);
        g_hUpdateThread = hThread;
    } else {
        // Thread creation failed, free parameter memory
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("更新检查线程创建失败，错误码：%lu，错误信息：%s", errorCode, errorMsg);
        
        free(threadParams);
        g_bUpdateThreadRunning = FALSE;
    }
}