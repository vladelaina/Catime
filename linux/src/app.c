/**
 * @file app.c
 * @brief Application controller: lifecycle, timer-event handling, user actions,
 *        CLI routing, and hotkey dispatch.
 */
#include "app.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "audio.h"
#include "config.h"
#include "hotkey.h"
#include "i18n.h"
#include "log.h"
#include "notify.h"
#include "paths.h"
#include "render.h"
#include "timer.h"
#include "tray.h"
#include "window.h"

static guint g_tick_id = 0;
static guint g_save_id = 0;

/* ---------- helpers ---------- */

static void spawn_async(const char *cmd) {
    if (!cmd || !*cmd) return;
    GError *err = NULL;
    if (!g_spawn_command_line_async(cmd, &err)) {
        LOG_WARNING("spawn '%s' failed: %s", cmd, err ? err->message : "?");
        if (err) g_error_free(err);
    }
}

static void notify_and_sound(const char *summary, const char *body) {
    CatimeConfig *c = config_get();
    if (c->notification_disabled) return;
    catime_notify_show(summary, body, c->notification_timeout_ms);
    if (c->notification_sound[0])
        audio_play_file(c->notification_sound, c->notification_volume);
    else
        audio_play_beep(c->notification_volume);
}

/* ---------- timer event handling ---------- */

static void handle_countdown_finished(void) {
    CatimeConfig *c = config_get();
    LOG_INFO("countdown finished (action=%d)", (int)c->timeout_action);
    switch (c->timeout_action) {
        case TIMEOUT_SHOW_TIME:
            audio_stop();
            timer_set_mode_show_time();
            break;
        case TIMEOUT_COUNT_UP:
            audio_stop();
            timer_start_countup();
            break;
        case TIMEOUT_OPEN_FILE:
            if (c->timeout_file[0]) {
                char cmd[700];
                snprintf(cmd, sizeof(cmd), "xdg-open '%s'", c->timeout_file);
                spawn_async(cmd);
            }
            notify_and_sound(tr("Time's up!"), c->timeout_message);
            timer_clear();
            break;
        case TIMEOUT_OPEN_WEBSITE:
            if (c->timeout_website[0]) {
                char cmd[700];
                snprintf(cmd, sizeof(cmd), "xdg-open '%s'", c->timeout_website);
                spawn_async(cmd);
            }
            notify_and_sound(tr("Time's up!"), c->timeout_message);
            timer_clear();
            break;
        case TIMEOUT_LOCK:
            spawn_async("loginctl lock-session 2>/dev/null || xdg-screensaver lock");
            notify_and_sound(tr("Time's up!"), c->timeout_message);
            timer_clear();
            break;
        case TIMEOUT_MESSAGE:
        default:
            notify_and_sound(tr("Time's up!"), c->timeout_message);
            timer_clear();
            break;
    }
}

static void handle_pomodoro_interval(void) {
    /* the interval that just finished: */
    int idx = timer_pomodoro_index();
    CatimeConfig *c = config_get();
    int dur = (idx >= 0 && idx < c->pomo_count) ? c->pomo_times[idx] : 0;
    char dbuf[32];
    timer_format_duration(dur, dbuf, sizeof(dbuf));
    char body[128];
    snprintf(body, sizeof(body), "%s", dbuf);
    notify_and_sound(tr("Pomodoro"), body);

    int all_done = timer_pomodoro_advance();
    if (all_done) {
        notify_and_sound(tr("Pomodoro"), tr("Pomodoro completed"));
        timer_clear();
    }
}

static gboolean on_tick(gpointer user) {
    (void)user;
    int64_t now = timer_now_ms();
    TimerEvent ev = timer_tick(now);
    if (ev == TIMER_EV_COUNTDOWN_FINISHED)
        handle_countdown_finished();
    else if (ev == TIMER_EV_POMODORO_INTERVAL_FINISHED)
        handle_pomodoro_interval();
    audio_poll();
    window_invalidate();
    return G_SOURCE_CONTINUE;
}

/* ---------- config save (debounced) ---------- */

static gboolean do_save(gpointer user) {
    (void)user;
    g_save_id = 0;
    config_save();
    return G_SOURCE_REMOVE;
}

void app_request_config_save(void) {
    if (g_save_id) g_source_remove(g_save_id);
    g_save_id = g_timeout_add_seconds(1, do_save, NULL);
}

void app_apply_config_changed(void) {
    render_load();
    window_apply_config();
    hotkey_reload();
    tray_update();
    window_invalidate();
}

void app_on_edit_mode_changed(void) {
    tray_set_edit_active(window_get_edit_mode());
}

/* ---------- user actions ---------- */

void app_action_show_time(void) {
    timer_set_mode_show_time();
    window_show();
    window_invalidate();
    tray_update();
}

void app_action_count_up(void) {
    timer_start_countup();
    audio_stop();
    window_show();
    window_invalidate();
    tray_update();
}

void app_action_countdown(int seconds) {
    audio_stop();
    timer_start_countdown(seconds);
    window_show();
    window_invalidate();
    tray_update();
}

void app_action_default_countdown(void) {
    CatimeConfig *c = config_get();
    app_action_countdown(c->default_start_time > 0 ? c->default_start_time : 1500);
}

void app_action_quick(int zero_based_index) {
    CatimeConfig *c = config_get();
    if (zero_based_index < 0 || zero_based_index >= c->time_options_count) {
        app_action_default_countdown();
        return;
    }
    app_action_countdown(c->time_options[zero_based_index]);
}

void app_action_pomodoro(void) {
    audio_stop();
    timer_start_pomodoro();
    window_show();
    window_invalidate();
    tray_update();
}

void app_action_pomodoro_reset(void) {
    timer_pomodoro_reset();
    audio_stop();
    window_invalidate();
    tray_update();
}

void app_action_pause_resume(void) {
    if (!timer_is_active()) return;
    timer_toggle_pause();
    if (timer_is_paused()) audio_stop();
    window_invalidate();
    tray_update();
}

void app_action_restart(void) {
    if (!timer_is_active() && timer_mode() == TIMER_MODE_SHOW_TIME) return;
    audio_stop();
    timer_restart();
    window_invalidate();
    tray_update();
}

void app_action_toggle_visibility(void) {
    window_toggle_visibility();
}

void app_action_toggle_edit_mode(void) {
    window_set_edit_mode(!window_get_edit_mode());
}

void app_action_toggle_ms(void) {
    CatimeConfig *c = config_get();
    c->show_milliseconds = !c->show_milliseconds;
    app_apply_config_changed();
    app_request_config_save();
}

void app_action_toggle_topmost(void) {
    CatimeConfig *c = config_get();
    c->window_topmost = !c->window_topmost;
    app_apply_config_changed();
    app_request_config_save();
}

void app_action_set_format(TimeFormat f) {
    CatimeConfig *c = config_get();
    c->time_format = f;
    app_apply_config_changed();
    app_request_config_save();
}

void app_action_set_24h(int on) {
    CatimeConfig *c = config_get();
    c->use_24hour = on;
    app_request_config_save();
    window_invalidate();
}

void app_action_set_show_seconds(int on) {
    CatimeConfig *c = config_get();
    c->show_seconds = on;
    app_request_config_save();
    window_invalidate();
}

void app_action_set_language(const char *config_key) {
    CatimeConfig *c = config_get();
    if (i18n_set_language(config_key) == 0) {
        snprintf(c->language, sizeof(c->language), "%s", i18n_current_config_key());
        app_request_config_save();
        tray_update();
        window_invalidate();
    }
}

/* ---------- hotkey dispatch ---------- */

static void on_hotkey(HotkeyAction action, void *user) {
    (void)user;
    switch (action) {
        case HK_SHOW_TIME:          app_action_show_time(); break;
        case HK_COUNT_UP:           app_action_count_up(); break;
        case HK_COUNTDOWN:          app_action_default_countdown(); break;
        case HK_QUICK1:             app_action_quick(0); break;
        case HK_QUICK2:             app_action_quick(1); break;
        case HK_QUICK3:             app_action_quick(2); break;
        case HK_POMODORO:           app_action_pomodoro(); break;
        case HK_TOGGLE_VISIBILITY:  app_action_toggle_visibility(); break;
        case HK_EDIT_MODE:          app_action_toggle_edit_mode(); break;
        case HK_PAUSE_RESUME:       app_action_pause_resume(); break;
        case HK_RESTART_TIMER:      app_action_restart(); break;
        case HK_CUSTOM_COUNTDOWN:   app_action_default_countdown(); break;
        case HK_TOGGLE_MS:          app_action_toggle_ms(); break;
        case HK_TOPMOST:            app_action_toggle_topmost(); break;
        case HK_COUNT:              break;
    }
}

/* ---------- CLI ---------- */

static int print_cli_help_impl(void) {
    printf(
        "Catime " CATIME_VERSION_STRING "\n\n"
        "Usage:\n"
        "  catime                 Run with the configured startup mode\n"
        "  catime <time>          Start a countdown\n"
        "  catime <command>       Send a command to the running instance\n\n"
        "Time formats (a countdown is started):\n"
        "  catime 25              25 minutes\n"
        "  catime 25h / 25s       25 hours / 25 seconds\n"
        "  catime 2h3m            2 hours 3 minutes\n"
        "  catime 1 30            1 minute 30 seconds\n"
        "  catime 1 30 20         1 hour 30 minutes 20 seconds\n"
        "  catime 17 20t          countdown to 17:20 today (tomorrow if past)\n"
        "  catime 9 9 9t          countdown to 09:09:09\n\n"
        "Commands:\n"
        "  s    toggle Show Current Time\n"
        "  u    start Count Up (stopwatch)\n"
        "  q1 q2 q3   start quick countdown preset 1/2/3\n"
        "  p    start Pomodoro\n"
        "  p4   start quick countdown preset #4 (p<number>)\n"
        "  v    show/hide window\n"
        "  e    toggle Edit Mode\n"
        "  pr   pause/resume timer\n"
        "  r    restart current timer\n"
        "  h    show this help\n\n"
        "Edit Mode: drag to move, wheel to scale, Ctrl+wheel for opacity,\n"
        "           arrow keys to nudge, right-click or Esc to exit.\n"
    );
    return 0;
}

void app_print_help(void) { print_cli_help_impl(); }

static int cli_one(const char *arg) {
    if (!arg || !*arg) return 0;
    if (arg[0] == '-' && arg[1] == '-') return 0; /* ignore --flags */

    if (strcasecmp(arg, "s") == 0) { app_action_show_time(); return 1; }
    if (strcasecmp(arg, "u") == 0) { app_action_count_up(); return 1; }
    if (strcasecmp(arg, "p") == 0) { app_action_pomodoro(); return 1; }
    if (strcasecmp(arg, "r") == 0) { app_action_restart(); return 1; }
    if (strcasecmp(arg, "pr") == 0) { app_action_pause_resume(); return 1; }
    if (strcasecmp(arg, "q1") == 0) { app_action_quick(0); return 1; }
    if (strcasecmp(arg, "q2") == 0) { app_action_quick(1); return 1; }
    if (strcasecmp(arg, "q3") == 0) { app_action_quick(2); return 1; }
    if (strcasecmp(arg, "v") == 0) { app_action_toggle_visibility(); return 1; }
    if (strcasecmp(arg, "e") == 0) { app_action_toggle_edit_mode(); return 1; }
    if (strcasecmp(arg, "h") == 0) { app_print_help(); return 1; }

    /* p<number> */
    if ((arg[0] == 'p' || arg[0] == 'P') && arg[1] >= '1' && arg[1] <= '9') {
        int idx = atoi(arg + 1) - 1;
        app_action_quick(idx);
        return 1;
    }

    /* time input */
    long sec = timer_parse_duration(arg);
    if (sec > 0) {
        app_action_countdown((int)sec);
        return 1;
    }
    LOG_WARNING("ignoring unknown CLI argument: %s", arg);
    return 0;
}

void app_run_cli_tokens(char **tokens, int n) {
    for (int i = 0; i < n; i++) {
        LOG_INFO("cli token: '%s'", tokens[i]);
        cli_one(tokens[i]);
    }
}

/* ---------- lifecycle ---------- */

static gboolean is_flag_arg(const char *a) {
    return a && a[0] == '-' && a[1] == '-';
}

void app_bootstrap(int argc, char **argv) {
    paths_ensure_dirs();
    catime_log_init(1);

    config_load();
    i18n_set_language(config_get()->language); /* honor saved language, else en */
    render_load();

    audio_init();
    catime_notify_init("Catime");

    timer_init();
    window_create();
    tray_create();
    hotkey_init(on_hotkey, NULL);

    /* initial visibility per startup mode */
    if (strcasecmp(config_get()->startup_mode, "NO_DISPLAY") == 0)
        window_hide();
    else
        window_show();

    /* apply any CLI arguments that started the app */
    int had_cli = 0;
    for (int i = 1; i < argc; i++) {
        if (is_flag_arg(argv[i])) continue;
        had_cli = 1;
        break;
    }
    if (had_cli) {
        char *tokens[128];
        int n = 0;
        for (int i = 1; i < argc && n < 128; i++)
            if (!is_flag_arg(argv[i])) tokens[n++] = argv[i];
        app_run_cli_tokens(tokens, n);
    }

    g_tick_id = g_timeout_add(50, on_tick, NULL);
}

void app_run(void) {
    gtk_main();
}

void app_quit(void) {
    LOG_INFO("shutting down");
    if (g_save_id) { g_source_remove(g_save_id); g_save_id = 0; config_save(); }
    window_save_position();
    config_save();
    gtk_main_quit();
}
