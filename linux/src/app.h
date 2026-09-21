/**
 * @file app.h
 * @brief Application controller: ties timer, window, tray, audio, notify,
 *        hotkeys, and CLI together.
 */
#ifndef CATIME_LINUX_APP_H
#define CATIME_LINUX_APP_H

#include "config.h"

/* lifecycle */
void app_bootstrap(int argc, char **argv);  /* gtk_init + create everything */
void app_run(void);                         /* gtk_main */
void app_quit(void);

/* config change propagation */
void app_request_config_save(void);         /* debounced */
void app_on_edit_mode_changed(void);
void app_apply_config_changed(void);        /* after config edits: re-render/re-grab/resize */

/* user actions (menu / hotkey / CLI) */
void app_action_show_time(void);
void app_action_count_up(void);
void app_action_countdown(int seconds);
void app_action_default_countdown(void);
void app_action_quick(int zero_based_index);
void app_action_pomodoro(void);
void app_action_pomodoro_reset(void);
void app_action_pause_resume(void);
void app_action_restart(void);
void app_action_toggle_visibility(void);
void app_action_toggle_edit_mode(void);
void app_action_toggle_ms(void);
void app_action_toggle_topmost(void);
void app_action_set_format(TimeFormat f);
void app_action_set_24h(int on);
void app_action_set_show_seconds(int on);
void app_action_set_language(const char *config_key);

/* CLI: route one or more command tokens (used by single-instance forwarding) */
void app_run_cli_tokens(char **tokens, int n);

/** Print CLI help text to stdout. */
void app_print_help(void);

#endif /* CATIME_LINUX_APP_H */
