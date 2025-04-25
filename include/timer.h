/**
 * @file timer.h
 * @brief 计时器核心功能定义
 * 
 * 本头文件定义计时器相关数据结构、状态枚举及全局变量，
 * 包含倒计时/正计时模式切换、超时动作处理等核心功能声明。
 */

#ifndef TIMER_H
#define TIMER_H

#include <windows.h>
#include <time.h>

/// 最大预设时间选项数量
#define MAX_TIME_OPTIONS 10

/// 番茄钟时间变量（单位：秒）
extern int POMODORO_WORK_TIME;    ///< 番茄钟工作时间（默认25分钟）
extern int POMODORO_SHORT_BREAK;  ///< 番茄钟短休息时间（默认5分钟）
extern int POMODORO_LONG_BREAK;   ///< 番茄钟长休息时间（默认15分钟）
extern int POMODORO_LOOP_COUNT;   ///< 番茄钟循环次数（默认1次）

/**
 * @brief 超时动作类型枚举
 * 
 * 定义计时器超时后执行的不同操作类型：
 * - MESSAGE: 显示提示信息
 * - LOCK: 锁定计算机
 * - SHUTDOWN: 关闭系统
 * - RESTART: 重启系统 
 * - OPEN_FILE: 打开指定文件
 * - SHOW_TIME: 显示当前时间
 * - COUNT_UP: 正计时
 * - OPEN_WEBSITE: 打开网站
 * - SLEEP: 睡眠
 */
typedef enum {
    TIMEOUT_ACTION_MESSAGE = 0,
    TIMEOUT_ACTION_LOCK = 1,
    TIMEOUT_ACTION_SHUTDOWN = 2,
    TIMEOUT_ACTION_RESTART = 3,
    TIMEOUT_ACTION_OPEN_FILE = 4,
    TIMEOUT_ACTION_SHOW_TIME = 5,
    TIMEOUT_ACTION_COUNT_UP = 6,
    TIMEOUT_ACTION_OPEN_WEBSITE = 7,
    TIMEOUT_ACTION_SLEEP = 8
} TimeoutActionType;

// 计时器状态 --------------------------------------------------
extern BOOL CLOCK_IS_PAUSED;         ///< 暂停状态标识
extern BOOL CLOCK_SHOW_CURRENT_TIME; ///< 显示实时时钟模式
extern BOOL CLOCK_USE_24HOUR;        ///< 24小时制显示
extern BOOL CLOCK_SHOW_SECONDS;      ///< 显示秒数
extern BOOL CLOCK_COUNT_UP;          ///< 正计时模式开关
extern char CLOCK_STARTUP_MODE[20];  ///< 启动模式(COUNTDOWN/COUNTUP)

// 时间相关 ----------------------------------------------------
extern int CLOCK_TOTAL_TIME;         ///< 总计时时间(秒)
extern int countdown_elapsed_time;    ///< 倒计时已用时间
extern int countup_elapsed_time;     ///< 正计时累计时间
extern time_t CLOCK_LAST_TIME_UPDATE;///< 最后更新时间戳
extern int last_displayed_second;    ///< 上一次显示的秒数，用于同步时间显示

// 消息状态 ----------------------------------------------------
extern BOOL countdown_message_shown; ///< 倒计时提示显示状态
extern BOOL countup_message_shown;   ///< 正计时提示显示状态
extern int pomodoro_work_cycles;     ///< 番茄钟工作周期计数

// 超时动作配置 ------------------------------------------------
extern TimeoutActionType CLOCK_TIMEOUT_ACTION; ///< 当前超时动作类型
extern char CLOCK_TIMEOUT_TEXT[50];            ///< 超时提示文本内容
extern char CLOCK_TIMEOUT_FILE_PATH[MAX_PATH];  ///< 超时打开文件路径
extern char CLOCK_TIMEOUT_WEBSITE_URL[MAX_PATH]; ///< 超时打开网站URL

// 时间选项配置 ------------------------------------------------
extern int time_options[MAX_TIME_OPTIONS]; ///< 预设时间选项数组
extern int time_options_count;             ///< 有效选项数量

/**
 * @brief 格式化时间显示
 * @param remaining_time 剩余时间(秒)
 * @param time_text 输出缓冲区(至少9字节)
 * 
 * 根据当前计时模式(24小时制/显示秒数)将秒数转换为"HH:MM:SS"格式，
 * 自动处理倒计时/正计时的显示差异。
 */
void FormatTime(int remaining_time, char* time_text);

/**
 * @brief 解析用户输入时间
 * @param input 用户输入字符串
 * @param total_seconds 输出解析后的总秒数
 * @return int 解析成功返回1，失败返回0
 * 
 * 支持"MM:SS"、"HH:MM:SS"及纯数字秒数格式，
 * 最大支持99小时59分59秒(359999秒)。
 */
int ParseInput(const char* input, int* total_seconds);

/**
 * @brief 验证时间输入格式
 * @param input 待验证字符串
 * @return int 有效返回1，无效返回0
 * 
 * 检查输入是否符合以下格式：
 * - 纯数字(秒数)
 * - MM:SS (1-59分 或 00-59秒)
 * - HH:MM:SS (00-99小时 00-59分 00-59秒)
 */
int isValidInput(const char* input);

/**
 * @brief 写入默认启动时间配置
 * @param seconds 默认启动时间(秒)
 * 
 * 更新配置文件中的CLOCK_DEFAULT_START_TIME项，
 * 影响计时器初始值及重置操作。
 */
void WriteConfigDefaultStartTime(int seconds);

/**
 * @brief 写入启动模式配置
 * @param mode 启动模式字符串("COUNTDOWN"/"COUNTUP")
 * 
 * 修改配置文件中的STARTUP_MODE项，
 * 控制程序启动时的默认计时模式。
 */
void WriteConfigStartupMode(const char* mode);

#endif // TIMER_H