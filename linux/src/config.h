/**
 * @file config.h
 * @brief Configuration (INI) for the Catime Linux port.
 *
 * Uses the same section/key names and value formats as the Windows build so a
 * config.ini is interoperable. Only the schema subset needed by the core
 * feature port is implemented; unknown keys are preserved verbatim on rewrite.
 */
#ifndef CATIME_LINUX_CONFIG_H
#define CATIME_LINUX_CONFIG_H

#include <stddef.h>

#define CATIME_VERSION_STRING "1.6.0-alpha2"

#define MAX_TIME_OPTIONS 50
#define MAX_POMODORO_TIMES 10
#define MAX_HOTKEY_LEN 64

typedef enum {
    HK_SHOW_TIME = 0,
    HK_COUNT_UP,
    HK_COUNTDOWN,
    HK_QUICK1,
    HK_QUICK2,
    HK_QUICK3,
    HK_POMODORO,
    HK_TOGGLE_VISIBILITY,
    HK_EDIT_MODE,
    HK_PAUSE_RESUME,
    HK_RESTART_TIMER,
    HK_CUSTOM_COUNTDOWN,
    HK_TOGGLE_MS,
    HK_TOPMOST,
    HK_COUNT
} HotkeyAction;

typedef enum {
    TIMEFMT_DEFAULT = 0,
    TIMEFMT_ZERO_PADDED,
    TIMEFMT_FULL_PADDED
} TimeFormat;

typedef enum {
    TIMEOUT_MESSAGE = 0,
    TIMEOUT_SHOW_TIME,
    TIMEOUT_COUNT_UP,
    TIMEOUT_LOCK,
    TIMEOUT_OPEN_FILE,
    TIMEOUT_OPEN_WEBSITE
} TimeoutAction;

typedef struct {
    /* [General] */
    char     config_version[32];
    char     language[32];
    int      first_run;

    /* [Display] */
    char     text_color[128];          /* "#RRGGBB" or "#RRGGBB_#RRGGBB[_...] */
    int      base_font_size;           /* 8..500 */
    char     font_family[128];         /* Pango font family ("" = default) */
    int      window_pos_x;             /* -1 = centered */
    int      window_pos_y;             /* -1 = top */
    int      window_position_manual;   /* bool */
    double   window_scale;             /* >= 0.5 */
    int      window_topmost;           /* bool */
    int      window_opacity;           /* 10..100 */
    int      move_step_small;          /* 1..500 */
    int      move_step_large;          /* 1..500 */

    /* [Timer] */
    int      default_start_time;       /* 1..86400 */
    int      use_24hour;               /* bool */
    int      show_seconds;             /* bool (clock mode) */
    TimeFormat time_format;
    int      show_milliseconds;        /* bool */
    int      time_options[MAX_TIME_OPTIONS];
    int      time_options_count;
    TimeoutAction timeout_action;
    char     timeout_file[512];
    char     timeout_website[512];
    char     startup_mode[24];         /* SHOW_TIME/COUNTDOWN/COUNT_UP/NO_DISPLAY/POMODORO */

    /* [Pomodoro] */
    int      pomo_times[MAX_POMODORO_TIMES];
    int      pomo_count;
    int      pomo_loop;                /* 1..100 */

    /* [Notification] */
    char     timeout_message[256];
    int      notification_timeout_ms;  /* 0..60000 */
    char     notification_sound[512];
    int      notification_volume;      /* 0..100 */
    int      notification_disabled;    /* bool */

    /* [Hotkeys] */
    char     hotkeys[HK_COUNT][MAX_HOTKEY_LEN];

    /* [Colors] */
    char     color_options[1024];
} CatimeConfig;

/** Global config singleton (populated by config_load). */
CatimeConfig *config_get(void);

/** Default values of the hotkey action labels (for menu building). */
const char *config_hotkey_action_name(HotkeyAction a);

/** Load config.ini (creating defaults if absent). Returns 0 on success. */
int config_load(void);

/** Persist config.ini atomically. Returns 0 on success. */
int config_save(void);

/** Reset to built-in defaults (does not touch disk). */
void config_reset_to_defaults(void);

#endif /* CATIME_LINUX_CONFIG_H */
