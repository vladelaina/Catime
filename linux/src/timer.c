/**
 * @file timer.c
 * @brief Timer state machine, Pomodoro, formatting, and duration parsing.
 */
#include "timer.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"

#define DEFAULT_FALLBACK_TIME 60
#define MAX_POMODORO_TIMES 10

/* ---- monotonic clock ---- */
static int64_t g_clock_base_real_ms = 0;
static int64_t g_clock_base_mono_ns = 0;

static void init_clock_base(void) {
    struct timespec tm = {0, 0}, tr = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tm);
    clock_gettime(CLOCK_REALTIME, &tr);
    g_clock_base_mono_ns = (int64_t)tm.tv_sec * 1000000000LL + tm.tv_nsec;
    g_clock_base_real_ms = (int64_t)tr.tv_sec * 1000LL + tr.tv_nsec / 1000000LL;
}

int64_t timer_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t ns = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return (ns - g_clock_base_mono_ns) / 1000000LL;
}

/* ---- state ---- */
static TimerMode g_mode = TIMER_MODE_SHOW_TIME;
static int g_paused = 0;

static int g_total_time = 0;        /* countdown total (s) */
static int g_countdown_elapsed = 0; /* countdown elapsed (s) */
static int g_countup_elapsed = 0;   /* count-up elapsed (s) */

static int64_t g_target_end_ms = 0; /* countdown deadline */
static int64_t g_start_ms = 0;      /* count-up start */
static int64_t g_pause_start_ms = 0;

static int g_message_shown = 0;

/* Pomodoro session (snapshot) */
static int g_pomo_times[MAX_POMODORO_TIMES];
static int g_pomo_count = 0;
static int g_pomo_loop = 1;
static int g_pomo_index = 0;
static int g_pomo_cycles = 0;
static int g_pomo_active = 0;

void timer_init(void) {
    init_clock_base();
    CatimeConfig *c = config_get();
    const char *m = c->startup_mode;
    if (strcmp(m, "COUNT_UP") == 0) {
        g_mode = TIMER_MODE_COUNT_UP;
        g_start_ms = timer_now_ms();
    } else if (strcmp(m, "POMODORO") == 0) {
        timer_start_pomodoro();
        return;
    } else if (strcmp(m, "NO_DISPLAY") == 0) {
        g_mode = TIMER_MODE_SHOW_TIME;
        g_paused = 1;
        g_message_shown = 1;
    } else if (strcmp(m, "COUNTDOWN") == 0 || strcmp(m, "DEFAULT") == 0) {
        int s = c->default_start_time > 0 ? c->default_start_time : DEFAULT_FALLBACK_TIME;
        timer_start_countdown(s);
        return;
    } else {
        g_mode = TIMER_MODE_SHOW_TIME; /* SHOW_TIME */
    }
}

TimerMode timer_mode(void) { return g_mode; }
int timer_is_paused(void) { return g_paused; }
int timer_total_seconds(void) { return g_total_time; }

int timer_elapsed_seconds(void) {
    if (g_mode == TIMER_MODE_COUNT_UP) return g_countup_elapsed;
    return g_countdown_elapsed;
}

int timer_is_active(void) {
    if (g_mode == TIMER_MODE_SHOW_TIME) return 0;
    if (g_mode == TIMER_MODE_COUNT_UP) return 1;
    return g_total_time > 0;
}

/* ---- mode transitions ---- */

static void start_timer_fresh(int total_seconds) {
    int64_t now = timer_now_ms();
    g_total_time = total_seconds > 0 ? total_seconds : DEFAULT_FALLBACK_TIME;
    g_countdown_elapsed = 0;
    g_target_end_ms = now + (int64_t)g_total_time * 1000;
    g_message_shown = 0;
    g_paused = 0;
    g_pause_start_ms = 0;
}

void timer_set_mode_show_time(void) {
    g_mode = TIMER_MODE_SHOW_TIME;
    g_countup_elapsed = 0;
    g_total_time = 0;
    g_countdown_elapsed = 0;
    g_paused = 0;
    g_message_shown = 1;
    g_pomo_active = 0;
}

void timer_start_countup(void) {
    /* re-invoking while counting-up toggles pause */
    if (g_mode == TIMER_MODE_COUNT_UP && !g_paused) {
        timer_toggle_pause();
        return;
    }
    if (g_mode == TIMER_MODE_COUNT_UP && g_paused) {
        timer_toggle_pause();
        return;
    }
    g_mode = TIMER_MODE_COUNT_UP;
    g_countup_elapsed = 0;
    g_start_ms = timer_now_ms();
    g_paused = 0;
    g_pause_start_ms = 0;
    g_pomo_active = 0;
}

void timer_start_countdown(int seconds) {
    g_mode = TIMER_MODE_COUNTDOWN;
    g_pomo_active = 0;
    start_timer_fresh(seconds);
}

void timer_start_pomodoro(void) {
    CatimeConfig *c = config_get();
    g_pomo_count = c->pomo_count;
    if (g_pomo_count > MAX_POMODORO_TIMES) g_pomo_count = MAX_POMODORO_TIMES;
    if (g_pomo_count <= 0) {
        g_pomo_times[0] = 1500;
        g_pomo_count = 1;
    }
    for (int i = 0; i < g_pomo_count; i++) {
        g_pomo_times[i] = c->pomo_times[i] > 0 ? c->pomo_times[i] : 1500;
    }
    g_pomo_loop = c->pomo_loop;
    if (g_pomo_loop < 1) g_pomo_loop = 1;
    g_pomo_index = 0;
    g_pomo_cycles = 0;
    g_pomo_active = 1;

    g_mode = TIMER_MODE_POMODORO;
    start_timer_fresh(g_pomo_times[0]);
}

void timer_toggle_pause(void) {
    if (g_mode == TIMER_MODE_SHOW_TIME) return;
    if (!timer_is_active() && g_mode != TIMER_MODE_POMODORO) return;
    int64_t now = timer_now_ms();
    if (!g_paused) {
        g_paused = 1;
        g_pause_start_ms = now;
    } else {
        int64_t dur = now - g_pause_start_ms;
        g_target_end_ms += dur;
        g_start_ms += dur;
        g_pause_start_ms = 0;
        g_paused = 0;
    }
}

void timer_restart(void) {
    if (g_mode == TIMER_MODE_COUNT_UP) {
        g_start_ms = timer_now_ms();
        g_countup_elapsed = 0;
        g_paused = 0;
        g_pause_start_ms = 0;
    } else if (g_mode == TIMER_MODE_POMODORO) {
        /* restart current interval */
        start_timer_fresh(g_pomo_times[g_pomo_index]);
    } else if (g_mode == TIMER_MODE_COUNTDOWN) {
        start_timer_fresh(g_total_time);
    }
}

void timer_pomodoro_reset(void) {
    g_pomo_index = 0;
    g_pomo_cycles = 0;
    g_pomo_active = 0;
    g_mode = TIMER_MODE_SHOW_TIME;
    g_total_time = 0;
    g_countdown_elapsed = 0;
    g_paused = 0;
    g_message_shown = 1;
}

void timer_clear(void) {
    g_total_time = 0;
    g_countdown_elapsed = 0;
    g_message_shown = 1;
}

/* ---- tick ---- */

int timer_interval_ms(void) {
    CatimeConfig *c = config_get();
    if (c->show_milliseconds) return 20;
    if (g_mode == TIMER_MODE_SHOW_TIME) return c->show_seconds ? 250 : 1000;
    return 100;
}

static void format_components(int total_seconds, int ms100, char *buf, size_t size) {
    CatimeConfig *c = config_get();
    int h = total_seconds / 3600;
    int m = (total_seconds % 3600) / 60;
    int s = total_seconds % 60;
    char head[64];
    if (h > 0) {
        switch (c->time_format) {
            case TIMEFMT_DEFAULT: case TIMEFMT_ZERO_PADDED: case TIMEFMT_FULL_PADDED:
                snprintf(head, sizeof(head), "%02d:%02d:%02d", h, m, s);
                if (c->time_format == TIMEFMT_DEFAULT)
                    snprintf(head, sizeof(head), "%d:%02d:%02d", h, m, s);
                break;
        }
    } else if (m > 0) {
        if (c->time_format == TIMEFMT_FULL_PADDED)
            snprintf(head, sizeof(head), "00:%02d:%02d", m, s);
        else if (c->time_format == TIMEFMT_ZERO_PADDED)
            snprintf(head, sizeof(head), "%02d:%02d", m, s);
        else
            snprintf(head, sizeof(head), "%d:%02d", m, s);
    } else {
        if (c->time_format == TIMEFMT_FULL_PADDED)
            snprintf(head, sizeof(head), "00:00:%02d", s);
        else if (c->time_format == TIMEFMT_ZERO_PADDED)
            snprintf(head, sizeof(head), "00:%02d", s);
        else
            snprintf(head, sizeof(head), "%d", s);
    }
    if (c->show_milliseconds)
        snprintf(buf, size, "%s.%02d", head, ms100);
    else
        snprintf(buf, size, "%s", head);
}

static int format_countdown(char *buf, size_t size) {
    int64_t base = g_paused ? g_pause_start_ms : timer_now_ms();
    int64_t remaining_ms = g_target_end_ms - base;
    if (remaining_ms < 0) remaining_ms = 0;
    CatimeConfig *c = config_get();
    int sec;
    int ms100 = 0;
    if (c->show_milliseconds) {
        sec = (int)(remaining_ms / 1000);
        ms100 = (int)((remaining_ms % 1000) / 10);
    } else {
        sec = (int)((remaining_ms + 999) / 1000);
    }
    if (sec <= 0 && g_total_time > 0) sec = 0;
    format_components(sec, ms100, buf, size);
    return 1;
}

static int format_countup(char *buf, size_t size) {
    int64_t base = g_paused ? g_pause_start_ms : timer_now_ms();
    int64_t elapsed_ms = base - g_start_ms;
    if (elapsed_ms < 0) elapsed_ms = 0;
    CatimeConfig *c = config_get();
    int sec = (int)(elapsed_ms / 1000);
    int ms100 = c->show_milliseconds ? (int)((elapsed_ms % 1000) / 10) : 0;
    format_components(sec, ms100, buf, size);
    return 1;
}

static int format_clock(char *buf, size_t size) {
    CatimeConfig *c = config_get();
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    localtime_r(&ts.tv_sec, &tmv);
    int h = tmv.tm_hour;
    int m = tmv.tm_min;
    int s = tmv.tm_sec;
    if (!c->use_24hour) {
        h = tmv.tm_hour % 12;
        if (h == 0) h = 12;
    }
    char head[64];
    if (c->show_seconds)
        snprintf(head, sizeof(head), "%d:%02d:%02d", h, m, s);
    else
        snprintf(head, sizeof(head), "%d:%02d", h, m);
    if (c->show_milliseconds) {
        int ms100 = (int)((ts.tv_nsec / 1000000) % 1000) / 10;
        snprintf(buf, size, "%s.%02d", head, ms100);
    } else {
        snprintf(buf, size, "%s", head);
    }
    return 1;
}

int timer_format(char *buf, size_t size) {
    if (size == 0) return 0;
    buf[0] = '\0';
    if (g_mode == TIMER_MODE_SHOW_TIME) return format_clock(buf, size);
    if (g_mode == TIMER_MODE_COUNT_UP) return format_countup(buf, size);
    /* countdown / pomodoro */
    int64_t base = g_paused ? g_pause_start_ms : timer_now_ms();
    int64_t remaining_ms = g_target_end_ms - base;
    if (remaining_ms <= 0) {
        /* finished: show empty unless still mid-tick */
        return 0;
    }
    return format_countdown(buf, size);
}

void timer_format_duration(int seconds, char *buf, size_t size) {
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (h > 0)
        snprintf(buf, size, "%dh", h);
    else if (m > 0)
        snprintf(buf, size, "%dm", m);
    else
        snprintf(buf, size, "%ds", s);
}

TimerEvent timer_tick(int64_t now_ms) {
    (void)now_ms;
    if (g_mode == TIMER_MODE_SHOW_TIME) return TIMER_EV_NONE;

    if (g_mode == TIMER_MODE_COUNT_UP) {
        if (!g_paused)
            g_countup_elapsed = (int)((timer_now_ms() - g_start_ms) / 1000);
        return TIMER_EV_NONE;
    }

    /* countdown / pomodoro */
    if (g_paused) return TIMER_EV_NONE;

    int64_t base = timer_now_ms();
    int64_t remaining_ms = g_target_end_ms - base;
    if (remaining_ms < 0) remaining_ms = 0;
    int remaining_sec = (int)((remaining_ms + 999) / 1000);
    if (remaining_sec < 0) remaining_sec = 0;
    g_countdown_elapsed = g_total_time - remaining_sec;
    if (g_countdown_elapsed < 0) g_countdown_elapsed = 0;
    if (g_countdown_elapsed > g_total_time) g_countdown_elapsed = g_total_time;

    if (g_total_time > 0 && remaining_ms <= 0 && !g_message_shown) {
        g_message_shown = 1;
        g_countdown_elapsed = g_total_time;
        if (g_mode == TIMER_MODE_POMODORO && g_pomo_active) {
            return TIMER_EV_POMODORO_INTERVAL_FINISHED;
        }
        return TIMER_EV_COUNTDOWN_FINISHED;
    }
    return TIMER_EV_NONE;
}

/* Pomodoro: advance to next interval. Returns 1 if session fully done. */
int timer_pomodoro_advance(void) {
    g_pomo_index++;
    if (g_pomo_index >= g_pomo_count) {
        g_pomo_index = 0;
        g_pomo_cycles++;
    }
    if (g_pomo_cycles >= g_pomo_loop) {
        return 1; /* all done */
    }
    int sec = g_pomo_times[g_pomo_index];
    g_total_time = sec;
    g_countdown_elapsed = 0;
    g_target_end_ms = timer_now_ms() + (int64_t)sec * 1000;
    g_message_shown = 0;
    return 0;
}

int timer_pomodoro_index(void) { return g_pomo_index; }
int timer_pomodoro_cycles(void) { return g_pomo_cycles; }
int timer_pomodoro_loop(void) { return g_pomo_loop; }
int timer_pomodoro_count(void) { return g_pomo_count; }
int timer_pomodoro_loop_done(void) { return g_pomo_cycles >= g_pomo_loop; }

/* ---- duration parser (CLI) ---- */

long timer_parse_duration(const char *input) {
    if (!input || !*input) return -1;

    /* tokenize into number+optional unit; detect trailing 't' (absolute) */
    typedef struct { long val; int unit; } Tok; /* unit: 0 none,'h','m','s' */
    Tok toks[8];
    int ntok = 0;
    int absolute = 0;
    const char *p = input;
    while (*p && ntok < 8) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (!isdigit((unsigned char)*p)) return -1;
        long val = 0;
        while (isdigit((unsigned char)*p)) { val = val * 10 + (*p - '0'); p++; }
        int unit = 0;
        if (*p == 'h' || *p == 'H') { unit = 'h'; p++; }
        else if (*p == 'm' || *p == 'M') { unit = 'm'; p++; }
        else if (*p == 's' || *p == 'S') { unit = 's'; p++; }
        else if (*p == 't' || *p == 'T') { unit = 't'; absolute = 1; p++; }
        toks[ntok].val = val; toks[ntok].unit = unit; ntok++;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (*p) return -1;
    if (ntok == 0) return -1;

    /* absolute time: H [M [S]] */
    if (absolute) {
        if (ntok > 3) return -1;
        int hh = (int)toks[0].val;
        int mm = ntok > 1 ? (int)toks[1].val : 0;
        int ss = ntok > 2 ? (int)toks[2].val : 0;
        if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) return -1;
        time_t now = time(NULL);
        struct tm tmv;
        localtime_r(&now, &tmv);
        tmv.tm_hour = hh; tmv.tm_min = mm; tmv.tm_sec = ss;
        tmv.tm_isdst = -1;
        time_t target = mktime(&tmv);
        long diff = (long)(target - now);
        if (diff <= 0) diff += 24 * 3600;
        return diff;
    }

    /* relative: single token, no unit -> minutes */
    if (ntok == 1 && toks[0].unit == 0) {
        return toks[0].val * 60;
    }

    /* assign levels: h=3, m=2, s=1 */
    int level[8];
    int all_no_unit = 1;
    for (int i = 0; i < ntok; i++) {
        if (toks[i].unit == 'h') level[i] = 3;
        else if (toks[i].unit == 'm') level[i] = 2;
        else if (toks[i].unit == 's') level[i] = 1;
        else level[i] = 0;
        if (toks[i].unit != 0) all_no_unit = 0;
    }

    if (all_no_unit) {
        /* positional descending: start level depends on count */
        int start = ntok >= 3 ? 3 : 2;
        for (int i = 0; i < ntok; i++) level[i] = start - i;
    } else {
        for (int i = 0; i < ntok; i++) {
            if (level[i] != 0) continue;
            int lv = 0;
            if (i > 0 && level[i - 1] != 0) lv = level[i - 1] - 1;
            else if (i < ntok - 1 && toks[i + 1].unit != 0) {
                int nxt = toks[i + 1].unit == 'h' ? 3 : toks[i + 1].unit == 'm' ? 2 : 1;
                lv = nxt + 1;
            }
            if (lv < 1 || lv > 3) lv = 2;
            level[i] = lv;
        }
    }

    long total = 0;
    for (int i = 0; i < ntok; i++) {
        if (level[i] == 3) total += toks[i].val * 3600;
        else if (level[i] == 2) total += toks[i].val * 60;
        else total += toks[i].val;
    }
    if (total <= 0) return -1;
    return total;
}

#include <stdio.h>
void timer_self_test(void) {
    struct { const char *in; long want; } cases[] = {
        {"25", 25 * 60},
        {"25h", 25 * 3600},
        {"25s", 25},
        {"2h3m", 2 * 3600 + 3 * 60},
        {"1 30", 1 * 60 + 30},
        {"1 30 20", 3600 + 30 * 60 + 20},
        {"23m3", 23 * 60 + 3},
        {"1 30m", 3600 + 30 * 60},
        {"90s", 90},
        {"2h3", 2 * 3600 + 3 * 60},
        {"", -1},
        {"abc", -1},
        {"0", 0},
    };
    int pass = 0, fail = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        long got = timer_parse_duration(cases[i].in);
        int ok = (got == cases[i].want);
        printf("  parse(%-10s) = %7ld  (want %7ld)  %s\n",
               cases[i].in, got, cases[i].want, ok ? "OK" : "FAIL");
        ok ? pass++ : fail++;
    }
    /* absolute time 't': must be >0, < 86400 */
    long t = timer_parse_duration("23 59 59t");
    printf("  parse(23 59 59t) = %ld  (0<x<86400)  %s\n", t,
           (t > 0 && t < 86400) ? "OK" : "FAIL");
    printf("self-test: %d passed, %d failed\n", pass, fail);
}
