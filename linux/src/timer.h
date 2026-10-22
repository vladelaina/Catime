/**
 * @file timer.h
 * @brief Timer state machine, Pomodoro, time formatting, and duration parsing.
 *
 * Mirrors the Windows build's drift-free timing: countdowns use an absolute
 * deadline (target_end_ms) and count-ups use an absolute start (start_ms),
 * both on a monotonic clock. Ticks are only display/state pumps.
 */
#ifndef CATIME_LINUX_TIMER_H
#define CATIME_LINUX_TIMER_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TIMER_MODE_SHOW_TIME = 0,
    TIMER_MODE_COUNT_UP,
    TIMER_MODE_COUNTDOWN,
    TIMER_MODE_POMODORO
} TimerMode;

typedef enum {
    TIMER_EV_NONE = 0,
    TIMER_EV_COUNTDOWN_FINISHED,
    TIMER_EV_POMODORO_INTERVAL_FINISHED,
    TIMER_EV_POMODORO_ALL_FINISHED
} TimerEvent;

void timer_init(void);

/** Monotonic milliseconds. */
int64_t timer_now_ms(void);

TimerMode timer_mode(void);
int timer_is_paused(void);
int timer_total_seconds(void);
int timer_elapsed_seconds(void);
int timer_is_active(void);  /* count-up or countdown (not show-time) */

/* Mode transitions */
void timer_set_mode_show_time(void);
void timer_start_countup(void);
void timer_start_countdown(int seconds);
void timer_start_pomodoro(void);
void timer_toggle_pause(void);
void timer_restart(void);          /* restart current timer */
void timer_pomodoro_reset(void);
void timer_clear(void);            /* end countdown (total=0, elapsed=0) */

/** Suggested tick interval (ms) based on display needs. */
int timer_interval_ms(void);

/** Advance state; return any completion event that occurred. */
TimerEvent timer_tick(int64_t now_ms);

/** Format the current display text. Returns 1 if text is present, 0 if empty. */
int timer_format(char *buf, size_t size);

/** Format an arbitrary duration in seconds as a compact label ("25m"). */
void timer_format_duration(int seconds, char *buf, size_t size);

/** Parse a CLI duration expression to seconds, or -1 if invalid. */
long timer_parse_duration(const char *s);

/** Print duration-parser self-test results to stdout. */
void timer_self_test(void);

/* Pomodoro status */
int timer_pomodoro_index(void);
int timer_pomodoro_cycles(void);
int timer_pomodoro_loop(void);
int timer_pomodoro_count(void);
int timer_pomodoro_loop_done(void);

/** Advance to next pomodoro interval. Returns 1 if the whole session is done. */
int timer_pomodoro_advance(void);

#endif /* CATIME_LINUX_TIMER_H */
