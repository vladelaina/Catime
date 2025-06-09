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

// INI配置文件节定义
#define INI_SECTION_GENERAL       "General"        // 一般设置（包括版本、语言等）
#define INI_SECTION_DISPLAY       "Display"        // 显示设置（字体、颜色、窗口位置等）
#define INI_SECTION_TIMER         "Timer"          // 计时器设置（默认时间等）
#define INI_SECTION_POMODORO      "Pomodoro"       // 番茄钟设置
#define INI_SECTION_NOTIFICATION  "Notification"   // 通知设置
#define INI_SECTION_HOTKEYS       "Hotkeys"        // 热键设置
#define INI_SECTION_RECENTFILES   "RecentFiles"    // 最近使用的文件
#define INI_SECTION_COLORS        "Colors"         // 颜色选项
#define INI_SECTION_OPTIONS       "Options"        // 其他选项

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

// 新增：通知类型枚举
typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,      // Catime通知窗口
    NOTIFICATION_TYPE_SYSTEM_MODAL,    // 系统模态窗口
    NOTIFICATION_TYPE_OS               // 操作系统通知
} NotificationType;

// 通知类型全局变量声明
extern NotificationType NOTIFICATION_TYPE;

// 新增：是否禁用通知窗口
extern BOOL NOTIFICATION_DISABLED;  ///< 是否禁用通知窗口

// 新增：通知音频相关配置
extern char NOTIFICATION_SOUND_FILE[MAX_PATH];  ///< 通知音频文件路径

// 新增：通知音频音量
extern int NOTIFICATION_SOUND_VOLUME;  ///< 通知音频音量 (0-100)

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
 * @brief 检查并创建音频文件夹
 * 
 * 检查配置文件同目录下是否存在audio文件夹，如果不存在则创建
 */
void CheckAndCreateAudioFolder();

/**
 * @brief 将超时动作写入配置文件
 * @param action 要写入的超时动作
 */
void WriteConfigTimeoutAction(const char* action);

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

/**
 * @brief 从配置文件中读取通知类型
 * 
 * 专门读取 NOTIFICATION_TYPE 配置项并更新相应的全局变量。
 */
void ReadNotificationTypeConfig(void);

/**
 * @brief 写入通知类型配置
 * @param type 通知类型枚举值
 * 
 * 更新配置文件中的通知类型设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationType(NotificationType type);

/**
 * @brief 从配置文件中读取是否禁用通知设置
 * 
 * 专门读取 NOTIFICATION_DISABLED 配置项并更新相应的全局变量。
 */
void ReadNotificationDisabledConfig(void);

/**
 * @brief 写入是否禁用通知配置
 * @param disabled 是否禁用通知 (TRUE/FALSE)
 * 
 * 更新配置文件中的是否禁用通知设置，
 * 采用临时文件方式确保配置更新安全。
 */
void WriteConfigNotificationDisabled(BOOL disabled);

/**
 * @brief 写入语言设置
 * @param language 语言ID
 * 
 * 将当前的语言设置写入配置文件
 */
void WriteConfigLanguage(int language);

/**
 * @brief 获取音频文件夹路径
 * @param path 存储路径的缓冲区
 * @param size 缓冲区大小
 */
void GetAudioFolderPath(char* path, size_t size);

/**
 * @brief 从配置文件中读取通知音频设置
 */
void ReadNotificationSoundConfig(void);

/**
 * @brief 写入通知音频配置
 * @param sound_file 音频文件路径
 */
void WriteConfigNotificationSound(const char* sound_file);

/**
 * @brief 从配置文件中读取通知音频音量
 */
void ReadNotificationVolumeConfig(void);

/**
 * @brief 写入通知音频音量配置
 * @param volume 音量百分比 (0-100)
 */
void WriteConfigNotificationVolume(int volume);

/**
 * @brief 将热键值转换为可读字符串
 * @param hotkey 热键值
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 */
void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

/**
 * @brief 将字符串转换为热键值
 * @param str 热键字符串
 * @return WORD 热键值
 */
WORD StringToHotkey(const char* str);

/**
 * @brief 从配置文件中读取热键设置
 * @param showTimeHotkey 存储显示时间热键的指针
 * @param countUpHotkey 存储正计时热键的指针
 * @param countdownHotkey 存储倒计时热键的指针
 * @param quickCountdown1Hotkey 存储快捷倒计时1热键的指针
 * @param quickCountdown2Hotkey 存储快捷倒计时2热键的指针
 * @param quickCountdown3Hotkey 存储快捷倒计时3热键的指针
 * @param pomodoroHotkey 存储番茄钟热键的指针
 * @param toggleVisibilityHotkey 存储隐藏/显示热键的指针
 * @param editModeHotkey 存储编辑模式热键的指针
 * @param pauseResumeHotkey 存储暂停/继续热键的指针
 * @param restartTimerHotkey 存储重新开始热键的指针
 */
void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey);

/**
 * @brief 从配置文件中读取自定义倒计时热键
 * @param hotkey 存储热键的指针
 */
void ReadCustomCountdownHotkey(WORD* hotkey);

/**
 * @brief 写入热键配置
 * @param showTimeHotkey 显示时间热键值
 * @param countUpHotkey 正计时热键值
 * @param countdownHotkey 倒计时热键值
 * @param quickCountdown1Hotkey 快捷倒计时1热键值
 * @param quickCountdown2Hotkey 快捷倒计时2热键值
 * @param quickCountdown3Hotkey 快捷倒计时3热键值
 * @param pomodoroHotkey 番茄钟热键值
 * @param toggleVisibilityHotkey 隐藏/显示热键值
 * @param editModeHotkey 编辑模式热键值
 * @param pauseResumeHotkey 暂停/继续热键值
 * @param restartTimerHotkey 重新开始热键值
 */
void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey);

/**
 * @brief 写入配置项
 * @param key 配置项键名
 * @param value 配置项值
 */
void WriteConfigKeyValue(const char* key, const char* value);

/**
 * @brief 判断是否已经执行过快捷方式检查
 * @return bool true表示已检查过，false表示未检查过
 */
bool IsShortcutCheckDone(void);

/**
 * @brief 设置快捷方式检查状态
 * @param done 是否已检查完成
 */
void SetShortcutCheckDone(bool done);

/**
 * @brief 从INI文件读取字符串值
 * @param section 节名
 * @param key 键名
 * @param defaultValue 默认值
 * @param returnValue 返回值缓冲区
 * @param returnSize 缓冲区大小
 * @param filePath 文件路径
 * @return 实际读取的字符数
 */
DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

/**
 * @brief 写入字符串值到INI文件
 * @param section 节名
 * @param key 键名
 * @param value 值
 * @param filePath 文件路径
 * @return 是否成功
 */
BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

/**
 * @brief 读取INI整数值
 * @param section 节名
 * @param key 键名
 * @param defaultValue 默认值
 * @param filePath 文件路径
 * @return 读取的整数值
 */
int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

/**
 * @brief 写入整数值到INI文件
 * @param section 节名
 * @param key 键名
 * @param value 值
 * @param filePath 文件路径
 * @return 是否成功
 */
BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

/// @}

#endif // CONFIG_H
