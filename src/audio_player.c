/**
 * @file audio_player.c
 * @brief 处理音频播放功能
 * 
 * 实现通知音频播放功能，支持后台播放多种音频格式，优先播放配置的音频文件，
 * 若文件不存在或播放失败则播放系统默认提示音。
 */

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>

#include "config.h"  // 引入配置相关头文件

// 声明全局变量
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];  // 从config.c引用

/**
 * @brief 检查文件是否存在
 * @param filePath 文件路径
 * @return BOOL 文件存在返回TRUE，否则返回FALSE
 */
static BOOL FileExists(const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 转换为宽字符以支持Unicode路径
    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);
    
    // 获取文件属性
    DWORD dwAttrib = GetFileAttributesW(wFilePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief 获取文件扩展名
 * @param filePath 文件路径
 * @param extension 存储扩展名的缓冲区
 * @param maxSize 缓冲区大小
 */
static void GetFileExtension(const char* filePath, char* extension, size_t maxSize) {
    if (!filePath || !extension || maxSize == 0) return;
    
    // 查找最后一个点
    const char* dot = strrchr(filePath, '.');
    if (dot && dot != filePath) {
        // 复制扩展名（包括点）
        strncpy(extension, dot, maxSize - 1);
        extension[maxSize - 1] = '\0';
    } else {
        extension[0] = '\0';
    }
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
 * @brief 使用mciSendString播放音频
 * @param hwnd 父窗口句柄
 * @param filePath 音频文件路径
 * @return BOOL 成功返回TRUE，失败返回FALSE
 */
static BOOL PlayAudioWithMCI(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 转换为宽字符以支持Unicode路径
    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);
    
    // 构建MCI命令
    wchar_t openCommand[MAX_PATH + 32];
    wchar_t playCommand[] = L"play catime_notify";
    wchar_t closeCommand[] = L"close catime_notify";
    wchar_t errorMsg[256] = {0};
    MCIERROR error;
    
    // 先尝试关闭可能存在的实例
    mciSendStringW(closeCommand, NULL, 0, NULL);
    
    // 构建打开命令
    StringCchPrintfW(openCommand, MAX_PATH + 32, L"open \"%s\" type mpegvideo alias catime_notify", wFilePath);
    
    // 尝试打开文件
    error = mciSendStringW(openCommand, NULL, 0, NULL);
    if (error) {
        mciGetErrorStringW(error, errorMsg, 256);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    // 尝试播放
    error = mciSendStringW(playCommand, NULL, 0, hwnd);
    if (error) {
        mciGetErrorStringW(error, errorMsg, 256);
        mciSendStringW(closeCommand, NULL, 0, NULL);
        ShowErrorMessage(hwnd, errorMsg);
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief 播放WAV格式音频
 * @param hwnd 父窗口句柄
 * @param filePath 音频文件路径
 * @return BOOL 成功返回TRUE，失败返回FALSE
 */
static BOOL PlayWavFile(HWND hwnd, const char* filePath) {
    if (!filePath || filePath[0] == '\0') return FALSE;
    
    // 转换为宽字符以支持Unicode路径
    wchar_t wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, filePath, -1, wFilePath, MAX_PATH);
    
    // 使用PlaySound API播放WAV文件
    if (!PlaySoundW(wFilePath, NULL, SND_FILENAME | SND_ASYNC)) {
        ShowErrorMessage(hwnd, L"无法播放WAV音频文件");
        return FALSE;
    }
    
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
 * @brief 播放通知音频
 * @param hwnd 父窗口句柄
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 如果配置了有效的NOTIFICATION_SOUND_FILE并且文件存在，
 * 系统将优先使用配置的音频文件，只有在文件不存在或播放失败时
 * 才会播放系统默认提示音。
 */
BOOL PlayNotificationSound(HWND hwnd) {
    // 检查是否配置了音频文件
    if (NOTIFICATION_SOUND_FILE[0] != '\0') {
        // 验证文件路径是否合法
        if (!IsValidFilePath(NOTIFICATION_SOUND_FILE)) {
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"音频文件路径无效:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
            // 播放系统默认提示音作为备选
            MessageBeep(MB_OK);
            return TRUE;
        }
        
        // 检查文件是否存在
        if (FileExists(NOTIFICATION_SOUND_FILE)) {
            char extension[16] = {0};
            GetFileExtension(NOTIFICATION_SOUND_FILE, extension, sizeof(extension));
            
            // 根据文件扩展名选择适当的播放方法
            if (_stricmp(extension, ".wav") == 0) {
                // 播放WAV文件
                if (PlayWavFile(hwnd, NOTIFICATION_SOUND_FILE)) {
                    return TRUE;
                }
            } else if (_stricmp(extension, ".mp3") == 0 ||
                      _stricmp(extension, ".wma") == 0 ||
                      _stricmp(extension, ".m4a") == 0 ||
                      _stricmp(extension, ".ogg") == 0) {
                // 使用MCI播放其他格式
                if (PlayAudioWithMCI(hwnd, NOTIFICATION_SOUND_FILE)) {
                    return TRUE;
                }
            } else {
                // 不支持的文件格式
                wchar_t errorMsg[256];
                StringCbPrintfW(errorMsg, sizeof(errorMsg), L"不支持的音频格式: %hs", extension);
                ShowErrorMessage(hwnd, errorMsg);
                
                // 尝试使用MCI播放，有时候即使扩展名不常见，仍然可以播放
                return PlayAudioWithMCI(hwnd, NOTIFICATION_SOUND_FILE);
            }
        } else {
            // 文件不存在
            wchar_t errorMsg[MAX_PATH + 64];
            StringCbPrintfW(errorMsg, sizeof(errorMsg), L"找不到配置的音频文件:\n%hs", NOTIFICATION_SOUND_FILE);
            ShowErrorMessage(hwnd, errorMsg);
        }
    }
    
    // 如果配置的音频文件不存在或播放失败，播放系统默认提示音
    MessageBeep(MB_OK);
    return TRUE;
} 