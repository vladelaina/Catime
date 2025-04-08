/**
 * @file pomodoro.h
 * @brief 番茄钟相关定义
 * 
 * 定义番茄钟的阶段和共享变量
 */

#ifndef POMODORO_H
#define POMODORO_H

// 番茄钟阶段枚举
typedef enum {
    POMODORO_PHASE_IDLE = 0,  // 空闲状态
    POMODORO_PHASE_WORK,      // 工作阶段
    POMODORO_PHASE_BREAK,     // 休息阶段
    POMODORO_PHASE_LONG_BREAK // 长休息阶段
} POMODORO_PHASE;

// 当前番茄钟阶段
extern POMODORO_PHASE current_pomodoro_phase;

// 当前正在执行的番茄钟时间索引
extern int current_pomodoro_time_index;

// 完成的番茄钟循环次数
extern int complete_pomodoro_cycles;

// 初始化番茄钟状态为工作阶段
void InitializePomodoro(void);

#endif // POMODORO_H
