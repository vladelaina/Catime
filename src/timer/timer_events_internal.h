/**
 * @file timer_events_internal.h
 * @brief Shared implementation details for timer event modules.
 */

#ifndef CATIME_TIMER_EVENTS_INTERNAL_H
#define CATIME_TIMER_EVENTS_INTERNAL_H

#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#include "timer/timer_events.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "timer/timer_render_cache.h"
#include "language.h"
#include "notification.h"
#include "pomodoro.h"
#include "config.h"
#include "config/config_defaults.h"
#include "window.h"
#include "drawing.h"
#include "menu_preview.h"
#include "audio_player.h"
#include "drag_scale.h"
#include "font/font_manager.h"
#include "tray/tray_animation_core.h"
#include "utils/string_convert.h"
#include "utils/url_safety.h"
#include "log.h"
#include "window/window_desktop_integration.h"

#define DEFAULT_POMODORO_DURATION 1500
#define MAX_RETRY_ATTEMPTS 3
#define RETRY_INTERVAL_MS 1500
#define FONT_CHECK_INTERVAL_MS 2000
#define MESSAGE_BUFFER_SIZE 256

/* Monotonic time source is implemented by timer.c. */
int64_t GetAbsoluteTimeMs(void);

/* Runtime state shared by the timer event modules. */
extern int pomodoro_initial_times_count;
extern int pomodoro_initial_loop_count;
extern int pomodoro_initial_times[MAX_POMODORO_TIMES];
extern DWORD last_timer_tick;
extern int ms_accumulator;
extern wchar_t g_visibleTimerCurrentText[TIME_TEXT_MAX_LEN];
extern wchar_t g_lastPaintedTimerText[TIME_TEXT_MAX_LEN];
extern BOOL g_hasLastPaintedTimerText;
extern TimeoutActionType g_armedTimeoutSystemAction;

/* Common timer/event helpers. */
void TimerEvents_ForceWindowRedraw(HWND hwnd);
void TimerEvents_RequestWindowRepaint(HWND hwnd);
BOOL TimerEvents_StartMainTimerForTimeoutAction(HWND hwnd,
                                                const char* actionName);
void TimerEvents_ShowTimeoutNotification(HWND hwnd,
                                         const char* messageUtf8,
                                         BOOL playSound);
void TimerEvents_ResetTimerState(int newTotalTime);
void TimerEvents_ResetMillisecondAccumulator(void);

/* Timeout system-action helpers. */
BOOL TimerEvents_IsSystemTimeoutAction(TimeoutActionType action);
BOOL TimerEvents_IsSystemTimeoutActionArmed(TimeoutActionType action);
BOOL TimerEvents_IsSystemTimeoutExecutionContextSafe(void);
void TimerEvents_ConsumeBlockedSystemTimeoutAction(HWND hwnd);
const char* TimerEvents_GetSystemActionName(TimeoutActionType action);
BOOL TimerEvents_ExecuteSystemAction(HWND hwnd, TimeoutActionType action);
void TimerEvents_HandleTimeoutActions(HWND hwnd);

/* Retry and window maintenance helpers. */
typedef void (*TimerEvents_RetrySetupCallback)(HWND hwnd);
BOOL TimerEvents_HandleRetryTimer(HWND hwnd, UINT timerId, int* retryCount,
                                  TimerEvents_RetrySetupCallback callback);
void TimerEvents_SetupTopmostWindow(HWND hwnd);
void TimerEvents_SetupVisibilityWindow(HWND hwnd);
BOOL TimerEvents_HandleFontValidation(HWND hwnd);
BOOL TimerEvents_HandleForceRedraw(HWND hwnd);

/* Pomodoro and completion helpers. */
BOOL TimerEvents_AdvancePomodoroState(void);
BOOL TimerEvents_IsActivePomodoroTimer(void);
void TimerEvents_FormatPomodoroTime(int seconds, wchar_t* buffer,
                                    size_t bufferSize);
BOOL TimerEvents_HandlePomodoroCompletion(HWND hwnd);
void TimerEvents_HandleCountdownCompletion(HWND hwnd);

/* Render/cache helpers and main timer tick. */
BOOL TimerEvents_ShouldRenderMainTimer(void);
BOOL TimerEvents_ShouldCheckActiveTimerRender(int currentElapsedSecond,
                                              int* lastCheckedSecond,
                                              BOOL* hasLastCheckedSecond);
BOOL TimerEvents_HandleMainTimer(HWND hwnd);

#endif /* CATIME_TIMER_EVENTS_INTERNAL_H */
