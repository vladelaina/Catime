/**
 * @file async_update_checker.c
 * @brief 简化的异步应用程序更新检查功能实现
 * 
 * 本文件实现了应用程序异步检查更新的功能，确保更新检查不会阻塞主线程。
 */

#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"
#include "../include/log.h"

// 线程参数结构体
typedef struct {
    HWND hwnd;
    BOOL silentCheck;
} UpdateThreadParams;

// 正在运行的更新检查线程句柄
static HANDLE g_hUpdateThread = NULL;
static BOOL g_bUpdateThreadRunning = FALSE;

/**
 * @brief 清理更新检查线程资源
 * 
 * 关闭线程句柄并释放相关资源。
 */
void CleanupUpdateThread() {
    LOG_INFO("清理更新检查线程资源");
    if (g_hUpdateThread != NULL) {
        LOG_INFO("等待更新检查线程结束，超时设为1秒");
        // 等待线程结束，但最多等待1秒
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
 * @brief 更新检查线程函数
 * @param param 线程参数（窗口句柄）
 * 
 * 在单独的线程中执行更新检查，不会阻塞主线程。
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    LOG_INFO("更新检查线程已启动");
    
    // 解析线程参数
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
    
    // 释放线程参数内存
    free(threadParams);
    LOG_INFO("释放线程参数内存");
    
    // 调用原始的更新检查函数，传入静默检查参数
    LOG_INFO("开始执行更新检查");
    CheckForUpdateSilent(hwnd, silentCheck);
    LOG_INFO("更新检查完成");
    
    // 标记线程已结束
    g_bUpdateThreadRunning = FALSE;
    
    // 线程结束
    _endthreadex(0);
    return 0;
}

/**
 * @brief 异步检查应用程序更新
 * @param hwnd 窗口句柄
 * @param silentCheck 是否为静默检查(仅在有更新时显示提示)
 * 
 * 在单独的线程中连接到GitHub检查是否有新版本。
 * 如果有，会提示用户是否在浏览器中下载。
 * 此函数立即返回，不会阻塞主线程。
 */
void CheckForUpdateAsync(HWND hwnd, BOOL silentCheck) {
    LOG_INFO("异步更新检查请求，窗口句柄：0x%p，静默模式：%s", 
             hwnd, silentCheck ? "是" : "否");
    
    // 如果已经有更新检查线程在运行，不要启动新线程
    if (g_bUpdateThreadRunning) {
        LOG_INFO("已有更新检查线程正在运行，跳过本次检查请求");
        return;
    }
    
    // 清理之前的线程句柄（如果有）
    if (g_hUpdateThread != NULL) {
        LOG_INFO("发现旧的线程句柄，清理中...");
        CloseHandle(g_hUpdateThread);
        g_hUpdateThread = NULL;
        LOG_INFO("旧线程句柄已关闭");
    }
    
    // 分配线程参数内存
    LOG_INFO("为线程参数分配内存");
    UpdateThreadParams* threadParams = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!threadParams) {
        // 内存分配失败
        LOG_ERROR("线程参数内存分配失败，无法启动更新检查线程");
        return;
    }
    
    // 设置线程参数
    threadParams->hwnd = hwnd;
    threadParams->silentCheck = silentCheck;
    LOG_INFO("线程参数设置完成");
    
    // 标记线程即将运行
    g_bUpdateThreadRunning = TRUE;
    
    LOG_INFO("准备创建更新检查线程");
    // 创建线程执行更新检查
    HANDLE hThread = (HANDLE)_beginthreadex(
        NULL,               // 默认安全属性
        0,                  // 默认栈大小
        UpdateCheckThreadProc, // 线程函数
        threadParams,       // 线程参数
        0,                  // 立即运行线程
        NULL                // 不需要线程ID
    );
    
    if (hThread) {
        // 保存线程句柄以便后续检查
        LOG_INFO("更新检查线程创建成功，线程句柄：0x%p", hThread);
        g_hUpdateThread = hThread;
    } else {
        // 线程创建失败，释放参数内存
        DWORD errorCode = GetLastError();
        char errorMsg[256] = {0};
        GetLastErrorDescription(errorCode, errorMsg, sizeof(errorMsg));
        LOG_ERROR("更新检查线程创建失败，错误码：%lu，错误信息：%s", errorCode, errorMsg);
        
        free(threadParams);
        g_bUpdateThreadRunning = FALSE;
    }
}