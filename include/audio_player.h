/**
 * @file audio_player.h
 * @brief 音频播放功能接口
 * 
 * 提供通知音频播放功能的接口，支持多种音频格式的后台播放。
 */

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <windows.h>

/**
 * @brief 音频播放完成回调函数类型
 * @param hwnd 窗口句柄
 * 
 * 当音频播放完成时，系统会调用此回调函数通知应用程序。
 */
typedef void (*AudioPlaybackCompleteCallback)(HWND hwnd);

/**
 * @brief 设置音频播放完成回调函数
 * @param hwnd 回调窗口句柄
 * @param callback 回调函数
 * 
 * 设置音频播放完成时的回调函数，当音频播放结束后，
 * 系统会调用该回调函数通知应用程序。
 */
void SetAudioPlaybackCompleteCallback(HWND hwnd, AudioPlaybackCompleteCallback callback);

/**
 * @brief 播放通知音频
 * @param hwnd 父窗口句柄
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 当配置了有效的NOTIFICATION_SOUND_FILE并且文件存在时，系统会播放该音频文件。
 * 如果文件不存在或播放失败，则会播放系统默认提示音。
 * 播放失败时会显示错误提示对话框。
 */
BOOL PlayNotificationSound(HWND hwnd);

/**
 * @brief 暂停当前播放的通知音频
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 暂停当前正在播放的通知音频，用于在用户暂停计时器时同步暂停音频。
 * 仅当音频处于播放状态且使用miniaudio播放时有效。
 */
BOOL PauseNotificationSound(void);

/**
 * @brief 继续播放已暂停的通知音频
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 继续播放之前被暂停的通知音频，用于在用户继续计时器时同步继续音频播放。
 * 仅当音频处于暂停状态时有效。
 */
BOOL ResumeNotificationSound(void);

/**
 * @brief 停止播放通知音频
 * 
 * 停止当前正在播放的任何通知音频
 */
void StopNotificationSound(void);

/**
 * @brief 设置音频播放音量
 * @param volume 音量百分比(0-100)
 * 
 * 设置音频播放的音量大小，影响所有通知音频的播放
 */
void SetAudioVolume(int volume);

#endif // AUDIO_PLAYER_H 