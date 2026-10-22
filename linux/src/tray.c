/**
 * @file tray.c
 * @brief System tray via Ayatana AppIndicator with a runtime-generated icon.
 */
#include "tray.h"

#include <cairo.h>
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "config.h"
#include "i18n.h"
#include "notify.h"
#include "paths.h"
#include "timer.h"

static AppIndicator *g_ind = NULL;
static GtkWidget *g_menu = NULL;

/* toggle widgets we refresh on tray_update */
static GtkCheckMenuItem *g_mi_show_time;
static GtkCheckMenuItem *g_mi_24h;
static GtkCheckMenuItem *g_mi_seconds;
static GtkCheckMenuItem *g_mi_countup;
static GtkCheckMenuItem *g_mi_ms;
static GtkCheckMenuItem *g_mi_topmost;
static GtkCheckMenuItem *g_mi_edit;
static GPtrArray *g_lang_items = NULL;

/* ---- generate a tray icon at runtime (no build-time image tools needed) ---- */
static void ensure_icon(void) {
    static char icon_path[1100];
    snprintf(icon_path, sizeof(icon_path), "%s/tray-icon.png", paths_data_dir());
    FILE *f = fopen(icon_path, "rb");
    if (f) { fclose(f); return; } /* already exists */

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t *cr = cairo_create(surf);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    /* rounded gradient disc */
    double x = 6, y = 6, r = 26;
    cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 64, 64);
    cairo_pattern_add_color_stop_rgb(p, 0, 1.0, 0.75, 0.27);   /* #FFA745-ish */
    cairo_pattern_add_color_stop_rgb(p, 1, 0.26, 0.72, 1.0);   /* #43AEFF-ish */
    cairo_set_source(cr, p);
    cairo_arc(cr, 32, 32, r, 0, 2 * 3.14159265);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    /* clock hands */
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 3.5);
    cairo_move_to(cr, 32, 32);
    cairo_line_to(cr, 32, 18);
    cairo_move_to(cr, 32, 32);
    cairo_line_to(cr, 44, 32);
    cairo_stroke(cr);
    cairo_destroy(cr);
    cairo_surface_write_to_png(surf, icon_path);
    cairo_surface_destroy(surf);
}

/* ---- helpers ---- */
static void set_label(GtkMenuItem *mi, const char *key) {
    gtk_menu_item_set_label(mi, tr(key));
}

static void on_activate(GtkMenuItem *mi, gpointer user) {
    (void)mi;
    void (*fn)(void) = (void (*)(void))user;
    if (fn) fn();
}

static GtkWidget *item(const char *label, void (*fn)(void)) {
    GtkWidget *mi = gtk_menu_item_new_with_label(label);
    g_signal_connect(mi, "activate", G_CALLBACK(on_activate), (gpointer)fn);
    return mi;
}

static GtkWidget *check_item(const char *label, void (*fn)(void), GtkCheckMenuItem **out) {
    GtkWidget *mi = gtk_check_menu_item_new_with_label(label);
    g_signal_connect(mi, "activate", G_CALLBACK(on_activate), (gpointer)fn);
    if (out) *out = GTK_CHECK_MENU_ITEM(mi);
    return mi;
}

static void on_pause(void)   { app_action_pause_resume(); }
static void on_restart(void) { app_action_restart(); }
static void on_vis(void)     { app_action_toggle_visibility(); }
static void on_show_time(void){ app_action_show_time(); }
static void on_countup(void) { app_action_count_up(); }
static void on_pomodoro(void){ app_action_pomodoro(); }
static void on_pomo_reset(void){ app_action_pomodoro_reset(); }
static void on_edit(void)    { app_action_toggle_edit_mode(); }
static void on_topmost(void) { app_action_toggle_topmost(); }
static void on_ms(void)      { app_action_toggle_ms(); }
static void on_quit(void)    { app_quit(); }
static void on_about(void)   { catime_notify_show("Catime", CATIME_VERSION_STRING, 0); }

static void on_fmt_default(GtkMenuItem *mi, gpointer u){ (void)mi;(void)u; app_action_set_format(TIMEFMT_DEFAULT); }
static void on_fmt_zero(GtkMenuItem *mi, gpointer u){ (void)mi;(void)u; app_action_set_format(TIMEFMT_ZERO_PADDED); }
static void on_fmt_full(GtkMenuItem *mi, gpointer u){ (void)mi;(void)u; app_action_set_format(TIMEFMT_FULL_PADDED); }

static void on_24h(GtkCheckMenuItem *mi, gpointer u){ (void)u; app_action_set_24h(gtk_check_menu_item_get_active(mi)); }
static void on_seconds(GtkCheckMenuItem *mi, gpointer u){ (void)u; app_action_set_show_seconds(gtk_check_menu_item_get_active(mi)); }

static GtkWidget *submenu(const char *label) {
    GtkWidget *root = gtk_menu_item_new_with_label(label);
    GtkWidget *sub = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(root), sub);
    return root;
}

static void on_quick(GtkMenuItem *mi, gpointer u) {
    (void)mi;
    app_action_quick(GPOINTER_TO_INT(u));
}

static void on_lang(GtkMenuItem *mi, gpointer u) {
    (void)mi;
    const CatimeLanguageInfo *info = i18n_language_info(GPOINTER_TO_INT(u));
    if (info) app_action_set_language(info->config_key);
}

void tray_create(void) {
    ensure_icon();
    g_menu = gtk_menu_new();

    /* Timer Control submenu */
    {
        GtkWidget *root = submenu(tr("Timer Control"));
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(root));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), item(tr("Pause"), on_pause));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), item(tr("Start Over"), on_restart));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), gtk_separator_menu_item_new());
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), item(tr("Show Window"), on_vis));
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), root);
    }

    /* Time Display submenu */
    {
        GtkWidget *root = submenu(tr("Time Display"));
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(root));
        g_mi_show_time = GTK_CHECK_MENU_ITEM(check_item(tr("Show Current Time"), on_show_time, NULL));
        g_mi_24h = GTK_CHECK_MENU_ITEM(check_item(tr("24-Hour Format"), NULL, NULL));
        g_mi_seconds = GTK_CHECK_MENU_ITEM(check_item(tr("Show Seconds"), NULL, NULL));
        g_signal_connect(g_mi_24h, "toggled", G_CALLBACK(on_24h), NULL);
        g_signal_connect(g_mi_seconds, "toggled", G_CALLBACK(on_seconds), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), GTK_WIDGET(g_mi_show_time));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), GTK_WIDGET(g_mi_24h));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), GTK_WIDGET(g_mi_seconds));
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), root);
    }

    /* Pomodoro submenu */
    {
        GtkWidget *root = submenu(tr("Pomodoro"));
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(root));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), item(tr("Start"), on_pomodoro));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), item(tr("Reset"), on_pomo_reset));
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), root);
    }

    /* Count Up + Countdown */
    g_mi_countup = GTK_CHECK_MENU_ITEM(check_item(tr("Count Up"), on_countup, NULL));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), GTK_WIDGET(g_mi_countup));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), item(tr("Countdown"), app_action_default_countdown));

    /* Quick presets */
    {
        CatimeConfig *c = config_get();
        if (c->time_options_count > 0) {
            gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
            for (int i = 0; i < c->time_options_count; i++) {
                char buf[32];
                timer_format_duration(c->time_options[i], buf, sizeof(buf));
                GtkWidget *mi = gtk_menu_item_new_with_label(buf);
                g_signal_connect(mi, "activate", G_CALLBACK(on_quick), GINT_TO_POINTER(i));
                gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), mi);
            }
        }
    }

    /* Format submenu */
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    {
        GtkWidget *root = submenu(tr("Format"));
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(root));
        GtkWidget *d = gtk_menu_item_new_with_label(tr("Default Format"));
        GtkWidget *z = gtk_menu_item_new_with_label(tr("09:59 Format"));
        GtkWidget *f = gtk_menu_item_new_with_label(tr("00:09:59 Format"));
        g_signal_connect(d, "activate", G_CALLBACK(on_fmt_default), NULL);
        g_signal_connect(z, "activate", G_CALLBACK(on_fmt_zero), NULL);
        g_signal_connect(f, "activate", G_CALLBACK(on_fmt_full), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), d);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), z);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), f);
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), gtk_separator_menu_item_new());
        g_mi_ms = GTK_CHECK_MENU_ITEM(check_item(tr("Show Milliseconds"), on_ms, NULL));
        gtk_menu_shell_append(GTK_MENU_SHELL(sub), GTK_WIDGET(g_mi_ms));
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), root);
    }

    /* toggles */
    g_mi_topmost = GTK_CHECK_MENU_ITEM(check_item(tr("Always on Top"), on_topmost, NULL));
    g_mi_edit = GTK_CHECK_MENU_ITEM(check_item(tr("Edit Mode"), on_edit, NULL));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), GTK_WIDGET(g_mi_topmost));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), GTK_WIDGET(g_mi_edit));

    /* Language submenu */
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    {
        GtkWidget *root = submenu(tr("Language"));
        GtkWidget *sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(root));
        g_lang_items = g_ptr_array_new();
        for (int i = 0; i < i18n_language_count(); i++) {
            const CatimeLanguageInfo *info = i18n_language_info(i);
            GtkWidget *mi = gtk_check_menu_item_new_with_label(info->native_name);
            g_signal_connect(mi, "activate", G_CALLBACK(on_lang), GINT_TO_POINTER(i));
            gtk_menu_shell_append(GTK_MENU_SHELL(sub), mi);
            g_ptr_array_add(g_lang_items, mi);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), root);
    }

    /* About / Exit */
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), item(tr("About"), on_about));
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), item(tr("Exit"), on_quit));

    gtk_widget_show_all(g_menu);

    g_ind = app_indicator_new("catime", "catime",
                              APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(g_ind, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(g_ind, "Catime");
    {
        char icon_path[1100];
        snprintf(icon_path, sizeof(icon_path), "%s/tray-icon.png", paths_data_dir());
        FILE *f = fopen(icon_path, "rb");
        if (f) { fclose(f); app_indicator_set_icon_full(g_ind, icon_path, "Catime"); }
        else   { app_indicator_set_icon_full(g_ind, "dialog-information", "Catime"); }
    }
    app_indicator_set_menu(g_ind, GTK_MENU(g_menu));

    tray_update();
}

void tray_update(void) {
    CatimeConfig *c = config_get();

    /* Block toggle handlers while we set state programmatically, otherwise
     * set_active() emits the signal -> action -> tray_update() -> recursion. */
#define BLOCK(mi, fn, data) g_signal_handlers_block_by_func((mi), (fn), (data))
#define UNBLOCK(mi, fn, data) g_signal_handlers_unblock_by_func((mi), (fn), (data))

    if (g_mi_show_time) { BLOCK(g_mi_show_time, on_activate, on_show_time);
        gtk_check_menu_item_set_active(g_mi_show_time, timer_mode() == TIMER_MODE_SHOW_TIME);
        UNBLOCK(g_mi_show_time, on_activate, on_show_time); }
    if (g_mi_24h)      { BLOCK(g_mi_24h, on_24h, NULL);
        gtk_check_menu_item_set_active(g_mi_24h, c->use_24hour);
        UNBLOCK(g_mi_24h, on_24h, NULL); }
    if (g_mi_seconds)  { BLOCK(g_mi_seconds, on_seconds, NULL);
        gtk_check_menu_item_set_active(g_mi_seconds, c->show_seconds);
        UNBLOCK(g_mi_seconds, on_seconds, NULL); }
    if (g_mi_countup)  { BLOCK(g_mi_countup, on_activate, on_countup);
        gtk_check_menu_item_set_active(g_mi_countup, timer_mode() == TIMER_MODE_COUNT_UP);
        UNBLOCK(g_mi_countup, on_activate, on_countup); }
    if (g_mi_ms)       { BLOCK(g_mi_ms, on_activate, on_ms);
        gtk_check_menu_item_set_active(g_mi_ms, c->show_milliseconds);
        UNBLOCK(g_mi_ms, on_activate, on_ms); }
    if (g_mi_topmost)  { BLOCK(g_mi_topmost, on_activate, on_topmost);
        gtk_check_menu_item_set_active(g_mi_topmost, c->window_topmost);
        UNBLOCK(g_mi_topmost, on_activate, on_topmost); }
    if (g_mi_edit)     { BLOCK(g_mi_edit, on_activate, on_edit);
        gtk_check_menu_item_set_active(g_mi_edit, 0);
        UNBLOCK(g_mi_edit, on_activate, on_edit); }

    if (g_lang_items) {
        for (guint i = 0; i < g_lang_items->len; i++) {
            const CatimeLanguageInfo *info = i18n_language_info((int)i);
            GtkCheckMenuItem *mi = GTK_CHECK_MENU_ITEM(g_ptr_array_index(g_lang_items, i));
            BLOCK(mi, on_lang, GINT_TO_POINTER((gint)i));
            gtk_check_menu_item_set_active(mi, strcmp(info->config_key, c->language) == 0);
            UNBLOCK(mi, on_lang, GINT_TO_POINTER((gint)i));
        }
    }
#undef BLOCK
#undef UNBLOCK
}

void tray_set_edit_active(int on) {
    if (g_mi_edit) {
        g_signal_handlers_block_by_func(g_mi_edit, on_edit, NULL);
        gtk_check_menu_item_set_active(g_mi_edit, on);
        g_signal_handlers_unblock_by_func(g_mi_edit, on_edit, NULL);
    }
}
