/**
 * @file config.c
 * @brief INI configuration load/save with defaults and validation.
 */
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "paths.h"

static CatimeConfig g_cfg;
static int g_loaded = 0;

static const char *kHotkeyNames[HK_COUNT] = {
    "Show Time", "Count Up", "Countdown", "Quick 1", "Quick 2", "Quick 3",
    "Pomodoro", "Toggle Visibility", "Edit Mode", "Pause/Resume",
    "Restart Timer", "Custom Countdown", "Toggle Milliseconds", "Always on Top"
};
const char *config_hotkey_action_name(HotkeyAction a) {
    if (a < 0 || a >= HK_COUNT) return "";
    return kHotkeyNames[a];
}

/* Hotkey config-key names used in [Hotkeys] (match Windows build layout). */
static const char *kHotkeyIniKeys[HK_COUNT] = {
    "SHOW_TIME", "COUNT_UP", "COUNTDOWN", "QUICK_COUNTDOWN1", "QUICK_COUNTDOWN2",
    "QUICK_COUNTDOWN3", "POMODORO", "TOGGLE_VISIBILITY", "EDIT_MODE",
    "PAUSE_RESUME", "RESTART_TIMER", "CUSTOM_COUNTDOWN", "TOGGLE_MILLISECONDS",
    "TOPMOST"
};

CatimeConfig *config_get(void) { return &g_cfg; }

/* ---------------- defaults ---------------- */

static int parse_int_list(const char *s, int *out, int max, int lo, int hi) {
    int n = 0;
    const char *p = s;
    while (*p && n < max) {
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (!*p) break;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p) { while (*p && *p != ',') p++; continue; }
        if (v < lo) v = lo;
        if (v > hi) v = hi;
        out[n++] = (int)v;
        p = end;
    }
    return n;
}

void config_reset_to_defaults(void) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    snprintf(g_cfg.config_version, sizeof(g_cfg.config_version), "%s", CATIME_VERSION_STRING);
    snprintf(g_cfg.language, sizeof(g_cfg.language), "English");
    g_cfg.first_run = 1;

    snprintf(g_cfg.text_color, sizeof(g_cfg.text_color), "#FFFFFF_#00FFFF");
    g_cfg.base_font_size = 20;
    g_cfg.font_family[0] = '\0';
    g_cfg.window_pos_x = -1;     /* centered */
    g_cfg.window_pos_y = 24;     /* near top */
    g_cfg.window_position_manual = 0;
    g_cfg.window_scale = 1.0;
    g_cfg.window_topmost = 1;
    g_cfg.window_opacity = 100;
    g_cfg.move_step_small = 10;
    g_cfg.move_step_large = 50;

    g_cfg.default_start_time = 1500;   /* 25 minutes */
    g_cfg.use_24hour = 1;
    g_cfg.show_seconds = 0;
    g_cfg.time_format = TIMEFMT_DEFAULT;
    g_cfg.show_milliseconds = 0;
    g_cfg.time_options_count = parse_int_list("1500,600,300",
        g_cfg.time_options, MAX_TIME_OPTIONS, 1, 86400);
    g_cfg.timeout_action = TIMEOUT_MESSAGE;
    g_cfg.timeout_file[0] = '\0';
    g_cfg.timeout_website[0] = '\0';
    snprintf(g_cfg.startup_mode, sizeof(g_cfg.startup_mode), "SHOW_TIME");

    g_cfg.pomo_count = parse_int_list("1500,300,1500,600",
        g_cfg.pomo_times, MAX_POMODORO_TIMES, 1, 86400);
    g_cfg.pomo_loop = 1;

    snprintf(g_cfg.timeout_message, sizeof(g_cfg.timeout_message), "Ding! Time's up~");
    g_cfg.notification_timeout_ms = 3000;
    g_cfg.notification_sound[0] = '\0';
    g_cfg.notification_volume = 100;
    g_cfg.notification_disabled = 0;

    for (int i = 0; i < HK_COUNT; i++)
        snprintf(g_cfg.hotkeys[i], MAX_HOTKEY_LEN, "None");

    snprintf(g_cfg.color_options, sizeof(g_cfg.color_options),
        "#FFFFFF,#FF5F5F,#F59E0B,#22C55E,#8771C6,#FFFFFF_#00FFFF,#FF5E96_#56C6FF,"
        "#FFA745_#FE869F_#EF7AC8_#A083ED_#43AEFF");
}

/* ---------------- INI model ---------------- */

#define MAX_ENTRIES 512
typedef struct { char section[64]; char key[128]; char value[768]; } IniEntry;
static IniEntry g_model[MAX_ENTRIES];
static int g_model_count = 0;

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static void parse_ini_buffer(const char *buf, size_t len) {
    char section[64] = "";
    const char *line = buf;
    const char *end = buf + len;
    while (line < end) {
        const char *eol = memchr(line, '\n', (size_t)(end - line));
        const char *next = eol ? eol + 1 : end;
        size_t llen = eol ? (size_t)(eol - line) : (size_t)(end - line);
        if (llen >= 2048) llen = 2047;
        char raw[2048];
        memcpy(raw, line, llen);
        raw[llen] = '\0';
        char *p = trim(raw);
        if (*p == '\0' || *p == '#' || *p == ';') { line = next; continue; }
        if (*p == '[') {
            char *cl = strchr(p, ']');
            if (cl) {
                *cl = '\0';
                snprintf(section, sizeof(section), "%s", p + 1);
            }
            line = next; continue;
        }
        char *eq = strchr(p, '=');
        if (!eq) { line = next; continue; }
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);
        if (g_model_count < MAX_ENTRIES) {
            snprintf(g_model[g_model_count].section, 64, "%s", section);
            snprintf(g_model[g_model_count].key, 128, "%s", key);
            snprintf(g_model[g_model_count].value, 768, "%s", val);
            g_model_count++;
        }
        line = next;
    }
}

static const char *model_get(const char *section, const char *key) {
    for (int i = 0; i < g_model_count; i++)
        if (strcasecmp(g_model[i].section, section) == 0 &&
            strcasecmp(g_model[i].key, key) == 0)
            return g_model[i].value;
    return NULL;
}

static int model_get_int(const char *sec, const char *key, int def, int lo, int hi) {
    const char *v = model_get(sec, key);
    if (!v || !*v) return def;
    char *end = NULL;
    long n = strtol(v, &end, 10);
    if (end == v || *end != '\0') return def;
    if (n < lo) n = lo;
    if (n > hi) n = hi;
    return (int)n;
}

static int model_get_bool(const char *sec, const char *key, int def) {
    const char *v = model_get(sec, key);
    if (!v || !*v) return def;
    return (strcasecmp(v, "true") == 0 || strcasecmp(v, "1") == 0 ||
            strcasecmp(v, "yes") == 0);
}

static double model_get_float(const char *sec, const char *key, double def, double lo) {
    const char *v = model_get(sec, key);
    if (!v || !*v) return def;
    char *end = NULL;
    double d = strtod(v, &end);
    if (end == v || *end != '\0') return def;
    if (d < lo) d = lo;
    return d;
}

static void model_get_str(const char *sec, const char *key, char *out, size_t n, const char *def) {
    const char *v = model_get(sec, key);
    snprintf(out, n, "%s", v ? v : def);
}

/* ---------------- load ---------------- */

static TimeFormat parse_time_format(const char *v) {
    if (!v) return TIMEFMT_DEFAULT;
    if (strcasecmp(v, "ZERO_PADDED") == 0) return TIMEFMT_ZERO_PADDED;
    if (strcasecmp(v, "FULL_PADDED") == 0) return TIMEFMT_FULL_PADDED;
    return TIMEFMT_DEFAULT;
}

static TimeoutAction parse_timeout_action(const char *v) {
    if (!v) return TIMEOUT_MESSAGE;
    if (strcasecmp(v, "SHOW_TIME") == 0) return TIMEOUT_SHOW_TIME;
    if (strcasecmp(v, "COUNT_UP") == 0) return TIMEOUT_COUNT_UP;
    if (strcasecmp(v, "LOCK") == 0) return TIMEOUT_LOCK;
    if (strcasecmp(v, "OPEN_FILE") == 0) return TIMEOUT_OPEN_FILE;
    if (strcasecmp(v, "OPEN_WEBSITE") == 0) return TIMEOUT_OPEN_WEBSITE;
    /* SHUTDOWN/RESTART/SLEEP reset to MESSAGE (one-shot, not persisted). */
    return TIMEOUT_MESSAGE;
}

int config_load(void) {
    config_reset_to_defaults();
    g_model_count = 0;

    char path[1024];
    paths_join(path, sizeof(path), paths_config_dir(), "config.ini");

    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 1024 * 1024) {
            char *buf = malloc((size_t)sz + 1);
            if (buf) {
                size_t rd = fread(buf, 1, (size_t)sz, f);
                buf[rd] = '\0';
                char *start = buf;
                if (rd >= 3 && (unsigned char)start[0] == 0xEF &&
                    (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF)
                    start += 3;
                parse_ini_buffer(start, rd - (size_t)(start - buf));
                free(buf);
            }
        }
        fclose(f);
    }

    /* [General] */
    model_get_str("General", "CONFIG_VERSION", g_cfg.config_version,
                  sizeof(g_cfg.config_version), CATIME_VERSION_STRING);
    model_get_str("General", "LANGUAGE", g_cfg.language, sizeof(g_cfg.language), "English");
    g_cfg.first_run = model_get_bool("General", "FIRST_RUN", 1);

    /* [Display] */
    model_get_str("Display", "CLOCK_TEXT_COLOR", g_cfg.text_color,
                  sizeof(g_cfg.text_color), "#FFFFFF_#00FFFF");
    g_cfg.base_font_size = model_get_int("Display", "CLOCK_BASE_FONT_SIZE", 20, 8, 500);
    model_get_str("Display", "FONT_FAMILY", g_cfg.font_family,
                  sizeof(g_cfg.font_family), "");
    g_cfg.window_pos_x = model_get_int("Display", "CLOCK_WINDOW_POS_X", -1, -100000, 100000);
    g_cfg.window_pos_y = model_get_int("Display", "CLOCK_WINDOW_POS_Y", 24, -100000, 100000);
    g_cfg.window_position_manual = model_get_bool("Display", "WINDOW_POSITION_MANUAL", 0);
    g_cfg.window_scale = model_get_float("Display", "WINDOW_SCALE", 1.0, 0.5);
    g_cfg.window_topmost = model_get_bool("Display", "WINDOW_TOPMOST", 1);
    g_cfg.window_opacity = model_get_int("Display", "WINDOW_OPACITY", 100, 10, 100);
    g_cfg.move_step_small = model_get_int("Display", "MOVE_STEP_SMALL", 10, 1, 500);
    g_cfg.move_step_large = model_get_int("Display", "MOVE_STEP_LARGE", 50, 1, 500);

    /* [Timer] */
    g_cfg.default_start_time = model_get_int("Timer", "CLOCK_DEFAULT_START_TIME", 1500, 1, 86400);
    g_cfg.use_24hour = model_get_bool("Timer", "CLOCK_USE_24HOUR", 1);
    g_cfg.show_seconds = model_get_bool("Timer", "CLOCK_SHOW_SECONDS", 0);
    {
        const char *tf = model_get("Timer", "CLOCK_TIME_FORMAT");
        g_cfg.time_format = parse_time_format(tf);
    }
    g_cfg.show_milliseconds = model_get_bool("Timer", "CLOCK_SHOW_MILLISECONDS", 0);
    {
        const char *opts = model_get("Timer", "CLOCK_TIME_OPTIONS");
        g_cfg.time_options_count = parse_int_list(opts ? opts : "1500,600,300",
            g_cfg.time_options, MAX_TIME_OPTIONS, 1, 86400);
        if (g_cfg.time_options_count == 0) {
            g_cfg.time_options_count = parse_int_list("1500,600,300",
                g_cfg.time_options, MAX_TIME_OPTIONS, 1, 86400);
        }
    }
    {
        const char *ta = model_get("Timer", "CLOCK_TIMEOUT_ACTION");
        g_cfg.timeout_action = parse_timeout_action(ta);
    }
    model_get_str("Timer", "CLOCK_TIMEOUT_FILE", g_cfg.timeout_file,
                  sizeof(g_cfg.timeout_file), "");
    model_get_str("Timer", "CLOCK_TIMEOUT_WEBSITE", g_cfg.timeout_website,
                  sizeof(g_cfg.timeout_website), "");
    model_get_str("Timer", "STARTUP_MODE", g_cfg.startup_mode,
                  sizeof(g_cfg.startup_mode), "SHOW_TIME");

    /* [Pomodoro] */
    {
        const char *opts = model_get("Pomodoro", "POMODORO_TIME_OPTIONS");
        g_cfg.pomo_count = parse_int_list(opts ? opts : "1500,300,1500,600",
            g_cfg.pomo_times, MAX_POMODORO_TIMES, 1, 86400);
        if (g_cfg.pomo_count == 0) {
            g_cfg.pomo_count = parse_int_list("1500,300,1500,600",
                g_cfg.pomo_times, MAX_POMODORO_TIMES, 1, 86400);
        }
    }
    g_cfg.pomo_loop = model_get_int("Pomodoro", "POMODORO_LOOP_COUNT", 1, 1, 100);

    /* [Notification] */
    model_get_str("Notification", "CLOCK_TIMEOUT_MESSAGE_TEXT", g_cfg.timeout_message,
                  sizeof(g_cfg.timeout_message), "Ding! Time's up~");
    g_cfg.notification_timeout_ms = model_get_int("Notification", "NOTIFICATION_TIMEOUT_MS",
                                                  3000, 0, 60000);
    model_get_str("Notification", "NOTIFICATION_SOUND_FILE", g_cfg.notification_sound,
                  sizeof(g_cfg.notification_sound), "");
    g_cfg.notification_volume = model_get_int("Notification", "NOTIFICATION_SOUND_VOLUME",
                                              100, 0, 100);
    g_cfg.notification_disabled = model_get_bool("Notification", "NOTIFICATION_DISABLED", 0);

    /* [Hotkeys] */
    for (int i = 0; i < HK_COUNT; i++) {
        char def[8] = "None";
        model_get_str("Hotkeys", kHotkeyIniKeys[i], g_cfg.hotkeys[i], MAX_HOTKEY_LEN, def);
    }

    /* [Colors] */
    model_get_str("Colors", "COLOR_OPTIONS", g_cfg.color_options,
                  sizeof(g_cfg.color_options), g_cfg.color_options);

    g_loaded = 1;

    /* first run: write defaults (with detected language) */
    if (g_cfg.first_run) {
        g_cfg.first_run = 0;
        config_save();
    }
    return 0;
}

/* ---------------- save ---------------- */

static void w(FILE *f, const char *section) { fprintf(f, "[%s]\n", section); }

int config_save(void) {
    char path[1024], tmp[1100];
    paths_join(path, sizeof(path), paths_config_dir(), "config.ini");
    paths_ensure_parent_dir(path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *f = fopen(tmp, "wb");
    if (!f) { LOG_ERROR("config: cannot write %s: %s", tmp, strerror(errno)); return -1; }

    fprintf(f, "[General]\n");
    fprintf(f, "CONFIG_VERSION=%s\n", g_cfg.config_version);
    fprintf(f, "LANGUAGE=%s\n", g_cfg.language);
    fprintf(f, "FIRST_RUN=FALSE\n");
    fprintf(f, "\n");

    fprintf(f, "[Display]\n");
    fprintf(f, "CLOCK_TEXT_COLOR=%s\n", g_cfg.text_color);
    fprintf(f, "CLOCK_BASE_FONT_SIZE=%d\n", g_cfg.base_font_size);
    fprintf(f, "FONT_FAMILY=%s\n", g_cfg.font_family);
    fprintf(f, "CLOCK_WINDOW_POS_X=%d\n", g_cfg.window_pos_x);
    fprintf(f, "CLOCK_WINDOW_POS_Y=%d\n", g_cfg.window_pos_y);
    fprintf(f, "WINDOW_POSITION_MANUAL=%s\n", g_cfg.window_position_manual ? "TRUE" : "FALSE");
    fprintf(f, "WINDOW_SCALE=%.9g\n", g_cfg.window_scale);
    fprintf(f, "WINDOW_TOPMOST=%s\n", g_cfg.window_topmost ? "TRUE" : "FALSE");
    fprintf(f, "WINDOW_OPACITY=%d\n", g_cfg.window_opacity);
    fprintf(f, "MOVE_STEP_SMALL=%d\n", g_cfg.move_step_small);
    fprintf(f, "MOVE_STEP_LARGE=%d\n", g_cfg.move_step_large);
    fprintf(f, "\n");

    fprintf(f, "[Timer]\n");
    fprintf(f, "CLOCK_DEFAULT_START_TIME=%d\n", g_cfg.default_start_time);
    fprintf(f, "CLOCK_USE_24HOUR=%s\n", g_cfg.use_24hour ? "TRUE" : "FALSE");
    fprintf(f, "CLOCK_SHOW_SECONDS=%s\n", g_cfg.show_seconds ? "TRUE" : "FALSE");
    fprintf(f, "CLOCK_TIME_FORMAT=%s\n",
            g_cfg.time_format == TIMEFMT_ZERO_PADDED ? "ZERO_PADDED" :
            g_cfg.time_format == TIMEFMT_FULL_PADDED ? "FULL_PADDED" : "DEFAULT");
    fprintf(f, "CLOCK_SHOW_MILLISECONDS=%s\n", g_cfg.show_milliseconds ? "TRUE" : "FALSE");
    fprintf(f, "CLOCK_TIME_OPTIONS=");
    for (int i = 0; i < g_cfg.time_options_count; i++)
        fprintf(f, "%s%d", i ? "," : "", g_cfg.time_options[i]);
    fprintf(f, "\n");
    const char *ta = g_cfg.timeout_action == TIMEOUT_SHOW_TIME ? "SHOW_TIME" :
                     g_cfg.timeout_action == TIMEOUT_COUNT_UP ? "COUNT_UP" :
                     g_cfg.timeout_action == TIMEOUT_LOCK ? "LOCK" :
                     g_cfg.timeout_action == TIMEOUT_OPEN_FILE ? "OPEN_FILE" :
                     g_cfg.timeout_action == TIMEOUT_OPEN_WEBSITE ? "OPEN_WEBSITE" : "MESSAGE";
    fprintf(f, "CLOCK_TIMEOUT_ACTION=%s\n", ta);
    fprintf(f, "CLOCK_TIMEOUT_FILE=%s\n", g_cfg.timeout_file);
    fprintf(f, "CLOCK_TIMEOUT_WEBSITE=%s\n", g_cfg.timeout_website);
    fprintf(f, "STARTUP_MODE=%s\n", g_cfg.startup_mode);
    fprintf(f, "\n");

    fprintf(f, "[Pomodoro]\n");
    fprintf(f, "POMODORO_TIME_OPTIONS=");
    for (int i = 0; i < g_cfg.pomo_count; i++)
        fprintf(f, "%s%d", i ? "," : "", g_cfg.pomo_times[i]);
    fprintf(f, "\n");
    fprintf(f, "POMODORO_LOOP_COUNT=%d\n", g_cfg.pomo_loop);
    fprintf(f, "\n");

    fprintf(f, "[Notification]\n");
    fprintf(f, "CLOCK_TIMEOUT_MESSAGE_TEXT=%s\n", g_cfg.timeout_message);
    fprintf(f, "NOTIFICATION_TIMEOUT_MS=%d\n", g_cfg.notification_timeout_ms);
    fprintf(f, "NOTIFICATION_SOUND_FILE=%s\n", g_cfg.notification_sound);
    fprintf(f, "NOTIFICATION_SOUND_VOLUME=%d\n", g_cfg.notification_volume);
    fprintf(f, "NOTIFICATION_DISABLED=%s\n", g_cfg.notification_disabled ? "TRUE" : "FALSE");
    fprintf(f, "\n");

    fprintf(f, "[Hotkeys]\n");
    for (int i = 0; i < HK_COUNT; i++)
        fprintf(f, "%s=%s\n", kHotkeyIniKeys[i], g_cfg.hotkeys[i]);
    fprintf(f, "\n");

    fprintf(f, "[Colors]\n");
    fprintf(f, "COLOR_OPTIONS=%s\n", g_cfg.color_options);
    fprintf(f, "\n");

    fflush(f);
    fclose(f);

    if (rename(tmp, path) != 0) {
        LOG_ERROR("config: rename %s -> %s failed: %s", tmp, path, strerror(errno));
        return -1;
    }
    (void)g_loaded;
    return 0;
}
