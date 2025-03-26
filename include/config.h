/**
 * @file config.h
 * @brief Configuration management module header file.
 *
 * This file defines the interfaces for application configuration,
 * including reading, writing, and managing settings related to window,
 * fonts, colors, and other customizable options.
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
extern char POMODORO_CYCLE_COMPLETE_TEXT[100]; ///< 番茄钟所有循环完成时的通知消息

/// @name Configuration-related Function Declarations
/// @{

/**
 * @brief Retrieves the configuration file path.
 * @param path Buffer to store the configuration file path.
 * @param size Size of the buffer.
 */
void GetConfigPath(char* path, size_t size);

/**
 * @brief Reads the configuration from the file.
 */
void ReadConfig();

/**
 * @brief Writes the timeout action to the configuration file.
 * @param action The timeout action to write.
 */
void WriteConfigTimeoutAction(const char* action);

/**
 * @brief Writes the edit mode to the configuration file.
 * @param mode The edit mode to write.
 */
void WriteConfigEditMode(const char* mode);

/**
 * @brief Writes the time options to the configuration file.
 * @param options The time options to write.
 */
void WriteConfigTimeOptions(const char* options);

/**
 * @brief Loads recent files from the configuration.
 */
void LoadRecentFiles(void);

/**
 * @brief Saves a recent file to the configuration.
 * @param filePath The path of the file to save.
 */
void SaveRecentFile(const char* filePath);

/**
 * @brief Converts a UTF-8 string to ANSI.
 * @param utf8Str The UTF-8 string to convert.
 * @return The converted ANSI string.
 */
char* UTF8ToANSI(const char* utf8Str);

/**
 * @brief Creates the default configuration file.
 * @param config_path The path to the configuration file.
 */
void CreateDefaultConfig(const char* config_path);

/**
 * @brief Writes all configuration settings to the file.
 * @param config_path The path to the configuration file.
 */
void WriteConfig(const char* config_path);

/**
 * @brief Writes the Pomodoro times to the configuration file.
 * @param work The work time in minutes.
 * @param short_break The short break time in minutes.
 * @param long_break The long break time in minutes.
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

/// @}

#endif // CONFIG_H
