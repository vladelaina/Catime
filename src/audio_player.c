/**
 * @file audio_player.c
 * @brief 处理音频播放功能
 * 
 * 实现通知音频播放功能，使用miniaudio库支持后台播放多种音频格式，优先播放配置的音频文件，
 * 若文件不存在或播放失败则播放系统默认提示音。
 */

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "../libs/miniaudio/miniaudio.h"

#include "config.h"  // 引入配置相关头文件

// 声明全局变量
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];  // 从config.c引用

// 定义回调函数类型，用于音频播放完成通知
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

// 声明全局miniaudio变量
static ma_engine g_audioEngine;
static ma_sound g_sound;
static ma_bool32 g_engineInitialized = MA_FALSE;
static ma_bool32 g_soundInitialized = MA_FALSE;

// 回调相关全局变量
static AudioPlaybackCompleteCallback g_audioCompleteCallback = NULL;
static HWND g_audioCallbackHwnd = NULL;
static UINT_PTR g_audioTimerId = 0;

// 播放状态跟踪
static ma_bool32 g_isPlaying = MA_FALSE;

// 前置声明
static void CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime);

/**
 * @brief 初始化音频引擎
 * @return BOOL 初始化成功返回TRUE，否则返回FALSE
 */
static BOOL InitializeAudioEngine() {
    if (g_engineInitialized) {
        return TRUE; // 已经初始化
    }
    
    // 初始化音频引擎
    ma_result result = ma_engine_init(NULL, &g_audioEngine);
    if (result != MA_SUCCESS) {
        return FALSE;
    }
    
    g_engineInitialized = MA_TRUE;
    return TRUE;
}

/**
 * @brief 清理音频引擎资源
 */
static void UninitializeAudioEngine() {
    if (g_engineInitialized) {
        // 先停止所有声音
        if (g_soundInitialized) {
            ma_sound_uninit(&g_sound);
            g_soundInitialized = MA_FALSE;
        }
        
        // 然后清理引擎
        ma_engine_uninit(&g_audioEngine);
        g_engineInitialized = MA_FALSE;
    }
}

/**
 * @brief 检查文件是否存在
 * @param filePath 文件路径
 * @return BOOL 文件存在返回TRUE，否则返回FALSE
 */
static BOOL FileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 转换为宽字符以支持Unicode路径
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2);
    
    // 获取文件属性
    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief 显示错误消息对话框
 * @param hwnd 父窗口句柄
 * @param errorMsg 错误消息
 */
static void ShowErrorMessage(HWND hwnd, const wchar_t* errorMsg) {
    MessageBoxW(hwnd, errorMsg, L"音频播放错误", MB_ICONERROR | MB_OK);
}

/**
 * @brief 检查音频播放是否完成的定时器回调
 * @param hwnd 窗口句柄
 * @param message 消息
 * @param idEvent 定时器ID
 * @param dwTime 时间
 */
static void CALLBACK CheckAudioPlaybackComplete(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    // 如果音频引擎和声音已初始化
    if (g_engineInitialized && g_soundInitialized) {
        // 检查播放状态
        if (!ma_sound_is_playing(&g_sound)) {
            // 停止并清理资源
            if (g_soundInitialized) {
                ma_sound_uninit(&g_sound);
                g_soundInitialized = MA_FALSE;
            }
            
            // 清理定时器
            KillTimer(hwnd, idEvent);
            g_audioTimerId = 0;
            g_isPlaying = MA_FALSE;
            
            // 调用回调函数
            if (g_audioCompleteCallback) {
                g_audioCompleteCallback(g_audioCallbackHwnd);
            }
        }
    } else {
        // 如果引擎或声音未初始化，视为播放完成
        KillTimer(hwnd, idEvent);
        g_audioTimerId = 0;
        g_isPlaying = MA_FALSE;
        
        if (g_audioCompleteCallback) {
            g_audioCompleteCallback(g_audioCallbackHwnd);
        }
    }
}

/**
 * @brief 系统提示音播放完成回调定时器函数
 * @param hwnd 窗口句柄
 * @param message 消息
 * @param idEvent 定时器ID
 * @param dwTime 时间
 */
static void CALLBACK SystemBeepDoneCallback(HWND hwnd, UINT message, UINT_PTR idEvent, DWORD dwTime) {
    // 清理定时器
    KillTimer(hwnd, idEvent);
    g_audioTimerId = 0;
    g_isPlaying = MA_FALSE;
    
    // 执行回调
    if (g_audioCompleteCallback) {
        g_audioCompleteCallback(g_audioCallbackHwnd);
    }
}

/**
 * @brief 使用miniaudio播放音频文件
 * @param hwnd 父窗口句柄
 * @param filePath 音频文件路径
 * @return BOOL 成功返回TRUE，失败返回FALSE
 */
static BOOL PlayAudioWithMiniaudio(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 确保音频引擎已初始化
    if (!InitializeAudioEngine()) {
        ShowErrorMessage(hwnd, L"无法初始化音频引擎");
        return FALSE;
    }
    
    // 如果已经有声音在播放，先停止并清理
    if (g_soundInitialized) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    // 步骤1: 首先将UTF-8路径转换为Windows宽字符格式
    wchar_t wFilePath[MAX_PATH * 2] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH * 2) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"路径转换错误 (UTF-8->Unicode): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    // 步骤2: 获取文件的短路径名 (8.3格式)，避免中文路径问题
    wchar_t shortPath[MAX_PATH] = {0};
    DWORD shortPathLen = GetShortPathNameW(wFilePath, shortPath, MAX_PATH);
    if (shortPathLen == 0 || shortPathLen >= MAX_PATH) {
        DWORD error = GetLastError();
        
        // 如果获取短路径失败，尝试使用Windows原生PlaySound API
        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            // 设置一个定时器来模拟播放完成回调
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }
        
        // 如果备用方法也失败，显示错误信息
        wchar_t errorMsg[512];
        swprintf(errorMsg, 512, L"获取短路径失败: %ls\n错误代码: %lu", wFilePath, error);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    // 步骤3: 将短路径名转换为ASCII编码（对短路径名可行）
    char asciiPath[MAX_PATH] = {0};
    if (WideCharToMultiByte(CP_ACP, 0, shortPath, -1, asciiPath, MAX_PATH, NULL, NULL) == 0) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"路径转换错误 (Short Path->ASCII): %lu", error);
        ShowErrorMessage(hwnd, errorMsg);
        
        // 尝试使用Windows原生API播放
        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }
        
        return FALSE;
    }
    
    // 步骤4: 使用ASCII短路径初始化miniaudio
    ma_result result = ma_sound_init_from_file(&g_audioEngine, asciiPath, 0, NULL, NULL, &g_sound);
    
    if (result != MA_SUCCESS) {
        // 尝试回退到宽字符路径（很可能会失败，但值得一试）
        char utf8Path[MAX_PATH * 4] = {0};
        WideCharToMultiByte(CP_UTF8, 0, wFilePath, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
        
        result = ma_sound_init_from_file(&g_audioEngine, utf8Path, 0, NULL, NULL, &g_sound);
        
        if (result != MA_SUCCESS) {
            // 如果还是失败，尝试使用Windows原生API播放
            if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
                // 设置一个定时器来模拟播放完成回调
                if (g_audioTimerId != 0) {
                    KillTimer(hwnd, g_audioTimerId);
                }
                g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
                g_isPlaying = MA_TRUE;
                return TRUE;
            }
            
            // 如果所有方法都失败，显示错误信息
            wchar_t errorMsg[512];
            swprintf(errorMsg, 512, L"无法加载音频文件: %ls\n错误代码: %d", wFilePath, result);
            ShowErrorMessage(hwnd, errorMsg);
            return FALSE;
        }
    }
    
    g_soundInitialized = MA_TRUE;
    
    // 开始播放
    if (ma_sound_start(&g_sound) != MA_SUCCESS) {
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
        
        // 再次尝试使用Windows原生API
        if (PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1002, 3000, (TIMERPROC)SystemBeepDoneCallback);
            g_isPlaying = MA_TRUE;
            return TRUE;
        }
        
        ShowErrorMessage(hwnd, L"无法开始播放音频");
        return FALSE;
    }
    
    g_isPlaying = MA_TRUE;
    
    // 设置定时器检查播放状态，每500毫秒检查一次
    if (g_audioTimerId != 0) {
        KillTimer(hwnd, g_audioTimerId);
    }
    g_audioTimerId = SetTimer(hwnd, 1001, 500, (TIMERPROC)CheckAudioPlaybackComplete);
    
    return TRUE;
}

/**
 * @brief 验证文件路径是否合法
 * @param filePath 要验证的文件路径
 * @return BOOL 路径合法返回TRUE，否则返回FALSE
 */
static BOOL IsValidFilePath(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 检查路径中是否包含等号或其他非法字符
    if (strchr(filePath, '=') != NULL) return FALSE;
    
    // 检查路径长度
    if (strlen(filePath) >= MAX_PATH) return FALSE;
    
    return TRUE;
}

/**
 * @brief 清理音频资源
 * 
 * 停止任何正在播放的音频并释放相关资源，
 * 确保新音频播放前不会有资源冲突。
 */
void CleanupAudioResources(void) {
    // 停止任何可能正在播放的WAV音频
    PlaySound(NULL, NULL, SND_PURGE);
    
    // 停止miniaudio播放
    if (g_engineInitialized && g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = MA_FALSE;
    }
    
    // 取消定时器
    if (g_audioTimerId != 0 && g_audioCallbackHwnd != NULL) {
        KillTimer(g_audioCallbackHwnd, g_audioTimerId);
        g_audioTimerId = 0;
    }
    
    g_isPlaying = MA_FALSE;
}

/**
 * @brief 设置音频播放完成回调函数
 * @param hwnd 回调窗口句柄
 * @param callback 回调函数
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback) {
    g_audioCallbackHwnd = hwnd;
    g_audioCompleteCallback = callback;
}

/**
 * @brief 播放通知音频
 * @param hwnd 父窗口句柄
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 如果配置了有效的NOTIFICATION_SOUND_FILE并且文件存在，
 * 系统将优先使用配置的音频文件，只有在文件不存在或播放失败时
 * 才会播放系统默认提示音。如果没有配置音频文件，不播放任何声音。
 */
BOOL PlayNotificationSound(HWND hwnd) {
    // 首先清理之前的音频资源，确保播放质量
    CleanupAudioResources();
    
    // 记录回调窗口句柄
    g_audioCallbackHwnd = hwnd;
    
    // 检查是否配置了音频文件
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // 检查是否是系统提示音特殊标记
        if (strcmp(NOTIFICATION_SOUND_FILE, "SYSTEM_BEEP") == 0) {
            // 直接播放系统提示音
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // 对于系统提示音，设置一个较短的定时器（500毫秒）以模拟完成回调
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
        
        // 验证文件路径是否合法
        if (!IsValidFilePath(NOTIFICATION_SOUND_FILE)) {
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"音频文件路径无效:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
            
            // 播放系统默认提示音作为备选
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // 同样设置短定时器
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
        
        // 检查文件是否存在
        if (FileExists(NOTIFICATION_SOUND_FILE)) {
            // 使用miniaudio播放所有类型的音频文件
            if (PlayAudioWithMiniaudio(hwnd, NOTIFICATION_SOUND_FILE)) {
                return TRUE;
            }
            
            // 如果播放失败，回退到系统提示音
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // 设置短定时器
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        } else {
            // 文件不存在
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"找不到配置的音频文件:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
            
            // 播放系统默认提示音作为备选
            MessageBeep(MB_OK);
            g_isPlaying = MA_TRUE;
            
            // 设置短定时器
            if (g_audioTimerId != 0) {
                KillTimer(hwnd, g_audioTimerId);
            }
            g_audioTimerId = SetTimer(hwnd, 1003, 500, (TIMERPROC)SystemBeepDoneCallback);
            
            return TRUE;
        }
    }
    
    // 如果没有配置音频文件，不播放任何声音
    return TRUE;
}

/**
 * @brief 停止播放通知音频
 * 
 * 停止当前正在播放的任何通知音频
 */
void StopNotificationSound(void) {
    CleanupAudioResources();
} 