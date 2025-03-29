/**
 * @file config.h
 * @brief 配置管理模块头文件
 *
 * 本文件定义了应用程序配置的接口，
 * 包括读取、写入和管理与窗口、字体、颜色及其他可定制选项相关的设置。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <time.h>
#include "../resource/resource.h"
#include "language.h"
#include "font.h"
#include "color.h"
#include "tray.h"
#include "tray_menu.h"
#include "timer.h"
#include "window.h"
#include "startup.h"

// 定义最多保存5个最近文件
#define MAX_RECENT_FILES 5

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
extern int CLOCK_RECENT_FILES_COUNT;
extern int CLOCK_DEFAULT_START_TIME;
extern time_t last_config_time;
extern int POMODORO_WORK_TIME;      // 工作时间(分钟)
extern int POMODORO_SHORT_BREAK;    // 短休息时间(分钟) 
extern int POMODORO_LONG_BREAK;     // 长休息时间(分钟)

// 新增：用于存储自定义通知消息的变量
extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];       ///< 倒计时结束时的通知消息
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];    ///< 番茄钟时间段结束时的通知消息
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];     ///< 番茄钟所有循环完成时的通知消息

// 新增：用于存储通知显示时间的变量
extern int NOTIFICATION_TIMEOUT_MS;  ///< 通知显示持续时间(毫秒)

/// @name 配置相关函数声明
/// @{

/**
 * @brief 获取配置文件路径
 * @param path 存储配置文件路径的缓冲区
 * @param size 缓冲区大小
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief 从文件读取配置
 */
void ReadConfig();

/**
 * @brief 将超时动作写入配置文件
 * @param action 要写入的超时动作
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief 将编辑模式写入配置文件
 * @param mode 要写入的编辑模式
 */
void WriteConfigEditMode(const char* mode);

/**
 * @brief 将时间选项写入配置文件
 * @param options 要写入的时间选项
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief 从配置中加载最近使用的文件
 */
void LoadRecentFiles(void);

/**
 * @brief 将最近使用的文件保存到配置中
 * @param filePath 要保存的文件路径
 */
void SaveRecentFile(const char* filePath);

/**
 * @brief 将UTF-8字符串转换为ANSI编码
 * @param utf8Str 要转换的UTF-8字符串
 * @return 转换后的ANSI字符串
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief 创建默认配置文件
 * @param config_path 配置文件的路径
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief 将所有配置设置写入文件
 * @param config_path 配置文件的路径
 */
void WriteConfig(const char* config_path);

/**
 * @brief 将番茄钟时间写入配置文件
 * @param work 工作时间（分钟）
 * @param short_break 短休息时间（分钟）
 * @param long_break 长休息时间（分钟）
 */
void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

/**
 * @brief 写入番茄钟时间设置
 * @param work_time 工作时间(秒)
 * @param short_break 短休息时间(秒)
 * @param long_break 长休息时间(秒)
 * 
 * 更新配置文件中的番茄钟时间设置，包括工作、短休息和长休息时间
 */
void WriteConfigPomodoroSettings(int work_time, int short_break, int long_break);

/**
 * @brief 写入番茄钟循环次数设置
 * @param loop_count 循环次数
 * 
 * 更新配置文件中的番茄钟循环次数设置
 */
void WriteConfigPomodoroLoopCount(int loop_count);

/**
 * @brief 写入超时打开文件路径
 * @param filePath 文件路径
 * 
 * 更新配置文件中的超时打开文件路径，同时设置超时动作为打开文件
 */
void WriteConfigTimeoutFile(const char* filePath);

/**
 * @brief 写入窗口置顶状态到配置文件
 * @param topmost 置顶状态字符串("TRUE"/"FALSE")
 */
void WriteConfigTopmost(const char* topmost);

/**
 * @brief 写入超时打开网站的URL
 * @param url 网站URL
 * 
 * 更新配置文件中的超时打开网站URL，同时设置超时动作为打开网站
 */
void WriteConfigTimeoutWebsite(const char* url);

/**
 * @brief 写入番茄钟时间选项
 * @param times 时间数组（秒）
 * @param count 时间数组长度
 * 
 * 将番茄钟时间选项写入配置文件
 */
void WriteConfigPomodoroTimeOptions(int* times, int count);

/**
 * @brief 从配置文件中读取通知消息文本
 * 
 * 专门读取 CLOCK_TIMEOUT_MESSAGE_TEXT、POMODORO_TIMEOUT_MESSAGE_TEXT 和 POMODORO_CYCLE_COMPLETE_TEXT
 * 并更新相应的全局变量。
 */
void ReadNotificationMessagesConfig(void);

/**
 * @brief 写入通知显示时间配置
 * @param timeout_ms 通知显示时间(毫秒)
 * 
 * 更新配置文件中的通知显示时间，并更新全局变量。
 */
void WriteConfigNotificationTimeout(int timeout_ms);

/**
 * @brief 从配置文件中读取通知显示时间
 * 
 * 专门读取 NOTIFICATION_TIMEOUT_MS 配置项并更新相应的全局变量。
 */
void ReadNotificationTimeoutConfig(void);

/**
 * @brief 从配置文件中读取通知最大透明度
 * 
 * 专门读取 NOTIFICATION_MAX_OPACITY 配置项
 * 并更新相应的全局变量。若配置不存在则保持默认值不变。
 */
void ReadNotificationOpacityConfig(void);

/**
 * @brief 写入通知最大透明度配置
 * @param opacity 透明度百分比值(1-100)
 * 
 * 更新配置文件中的通知最大透明度设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationOpacity(int opacity);

/**
 * @brief 写入通知消息配置
 * @param timeout_msg 倒计时超时提示文本
 * @param pomodoro_msg 番茄钟超时提示文本
 * @param cycle_complete_msg 番茄钟循环完成提示文本
 * 
 * 更新配置文件中的通知消息设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg);

/// @}

#endif // CONFIG_H
