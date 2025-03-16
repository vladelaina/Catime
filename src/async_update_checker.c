/**
 * @file async_update_checker.c
 * @brief 异步应用程序更新检查功能实现
 * 
 * 本文件实现了应用程序异步检查更新的功能，确保更新检查不会阻塞主线程。
 */

#include <windows.h>
#include <process.h>
#include "../include/async_update_checker.h"
#include "../include/update_checker.h"

// 线程参数结构体
typedef struct {
    HWND hwnd;
} UpdateThreadParams;

/**
 * @brief 更新检查线程函数
 * @param param 线程参数（窗口句柄）
 * 
 * 在单独的线程中执行更新检查，不会阻塞主线程。
 */
unsigned __stdcall UpdateCheckThreadProc(void* param) {
    UpdateThreadParams* threadParams = (UpdateThreadParams*)param;
    HWND hwnd = threadParams->hwnd;
    
    // 释放线程参数内存
    free(threadParams);
    
    // 调用原始的更新检查函数
    CheckForUpdate(hwnd);
    
    // 线程结束
    _endthreadex(0);
    return 0;
}

/**
 * @brief 异步检查应用程序更新
 * @param hwnd 窗口句柄
 * 
 * 在单独的线程中连接到GitHub/Gitee检查是否有新版本。
 * 如果有，会提示用户是否在浏览器中下载。
 * 此函数立即返回，不会阻塞主线程。
 */
void CheckForUpdateAsync(HWND hwnd) {
    // 分配线程参数内存
    UpdateThreadParams* threadParams = (UpdateThreadParams*)malloc(sizeof(UpdateThreadParams));
    if (!threadParams) {
        // 内存分配失败
        return;
    }
    
    // 设置线程参数
    threadParams->hwnd = hwnd;
    
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
        // 关闭线程句柄（线程会继续运行）
        CloseHandle(hThread);
    } else {
        // 线程创建失败，释放参数内存
        free(threadParams);
    }
}