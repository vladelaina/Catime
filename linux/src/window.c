/**
 * @file window.c
 * @brief GTK3 transparent, borderless, always-on-top countdown window with
 *        edit-mode interaction (drag to move, wheel to scale, Ctrl+wheel for
 *        opacity). Click-through when not in edit mode.
 */
#include "window.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "render.h"
#include "timer.h"

static GtkWidget *g_win = NULL;
static int g_edit_mode = 0;
static int g_visible = 1;
static int g_dragging = 0;
static int g_drag_root_x = 0, g_drag_root_y = 0;
static int g_win_x = 0, g_win_y = 0;
static int g_size_dirty = 1;
static char g_last_text[64] = {0};

/* ---- forward decls (controller hooks) ---- */
void app_request_config_save(void);
void app_on_edit_mode_changed(void);

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer user) {
    (void)w; (void)user;
    /* clear to fully transparent */
    cairo_save(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_restore(cr);

    char text[64];
    if (timer_format(text, sizeof(text))) {
        GtkAllocation alc;
        gtk_widget_get_allocation(g_win, &alc);
        render_draw(cr, alc.width, alc.height, text);
    }
    return FALSE;
}

static void apply_input_region(void) {
    if (!g_win || !gtk_widget_get_realized(g_win)) return;
    if (g_edit_mode) {
        /* interactive: full input region */
        gtk_widget_input_shape_combine_region(g_win, NULL);
    } else {
        /* click-through: empty input region */
        cairo_region_t *empty = cairo_region_create();
        gtk_widget_input_shape_combine_region(g_win, empty);
        cairo_region_destroy(empty);
    }
}

static gboolean on_configure(GtkWidget *w, GdkEventConfigure *e, gpointer user) {
    (void)w; (void)user;
    g_win_x = e->x;
    g_win_y = e->y;
    if (g_edit_mode) {
        CatimeConfig *c = config_get();
        c->window_pos_x = e->x;
        c->window_pos_y = e->y;
        c->window_position_manual = 1;
        app_request_config_save();
    }
    return FALSE;
}

static gboolean on_button_press(GtkWidget *w, GdkEventButton *e, gpointer user) {
    (void)w; (void)user;
    if (!g_edit_mode) return FALSE;
    if (e->button == 1) {
        g_dragging = 1;
        g_drag_root_x = (int)e->x_root;
        g_drag_root_y = (int)e->y_root;
        gtk_window_get_position(GTK_WINDOW(g_win), &g_win_x, &g_win_y);
    } else if (e->button == 3) {
        /* right-click exits edit mode */
        window_set_edit_mode(0);
    }
    return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer user) {
    (void)w; (void)user;
    if (!g_edit_mode || !g_dragging) return FALSE;
    int dx = (int)e->x_root - g_drag_root_x;
    int dy = (int)e->y_root - g_drag_root_y;
    gtk_window_move(GTK_WINDOW(g_win), g_win_x + dx, g_win_y + dy);
    return TRUE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *e, gpointer user) {
    (void)w; (void)user;
    if (e->button == 1) g_dragging = 0;
    return TRUE;
}

static gboolean on_scroll(GtkWidget *w, GdkEventScroll *e, gpointer user) {
    (void)w; (void)user;
    if (!g_edit_mode) return FALSE;
    CatimeConfig *c = config_get();
    int dir = (e->direction == GDK_SCROLL_UP) ? +1 :
              (e->direction == GDK_SCROLL_DOWN) ? -1 : 0;
    if (dir == 0) return FALSE;

    GdkModifierType state;
    guint evstate = e->state;
    state = (GdkModifierType)evstate;
    if (state & GDK_CONTROL_MASK) {
        c->window_opacity += dir * 5;
        if (c->window_opacity < 10) c->window_opacity = 10;
        if (c->window_opacity > 100) c->window_opacity = 100;
    } else {
        c->window_scale += dir * 0.1;
        if (c->window_scale < 0.5) c->window_scale = 0.5;
        if (c->window_scale > 20.0) c->window_scale = 20.0;
        g_size_dirty = 1;
    }
    window_invalidate();
    app_request_config_save();
    return TRUE;
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer user) {
    (void)w; (void)user;
    if (!g_edit_mode) return FALSE;
    CatimeConfig *c = config_get();
    int step = (e->state & GDK_CONTROL_MASK) ? c->move_step_large : c->move_step_small;
    int x = g_win_x, y = g_win_y;
    gtk_window_get_position(GTK_WINDOW(g_win), &x, &y);
    switch (e->keyval) {
        case GDK_KEY_Left:  gtk_window_move(GTK_WINDOW(g_win), x - step, y); break;
        case GDK_KEY_Right: gtk_window_move(GTK_WINDOW(g_win), x + step, y); break;
        case GDK_KEY_Up:    gtk_window_move(GTK_WINDOW(g_win), x, y - step); break;
        case GDK_KEY_Down:  gtk_window_move(GTK_WINDOW(g_win), x, y + step); break;
        case GDK_KEY_Escape: window_set_edit_mode(0); break;
        default: return FALSE;
    }
    return TRUE;
}

static void on_realize(GtkWidget *w, gpointer user) {
    (void)user;
    gdk_window_set_events(gtk_widget_get_window(w),
        gdk_window_get_events(gtk_widget_get_window(w)) |
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK);
    apply_input_region();
}

static gboolean on_delete(GtkWidget *w, GdkEvent *e, gpointer user) {
    (void)w; (void)e; (void)user;
    /* hide instead of destroy (keep running in tray) */
    window_hide();
    return TRUE;
}

void window_create(void) {
    if (g_win) return;
    g_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GdkScreen *screen = gtk_widget_get_screen(g_win);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(g_win, visual);

    gtk_widget_set_app_paintable(g_win, TRUE);
    gtk_window_set_decorated(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_win), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(g_win), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(g_win), config_get()->window_topmost);
    gtk_window_set_resizable(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_accept_focus(GTK_WINDOW(g_win), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(g_win), GDK_WINDOW_TYPE_HINT_UTILITY);
    gtk_widget_set_can_focus(g_win, TRUE);
    gtk_widget_add_events(g_win,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK);

    g_signal_connect(g_win, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(g_win, "configure-event", G_CALLBACK(on_configure), NULL);
    g_signal_connect(g_win, "realize", G_CALLBACK(on_realize), NULL);
    g_signal_connect(g_win, "button-press-event", G_CALLBACK(on_button_press), NULL);
    g_signal_connect(g_win, "motion-notify-event", G_CALLBACK(on_motion), NULL);
    g_signal_connect(g_win, "button-release-event", G_CALLBACK(on_button_release), NULL);
    g_signal_connect(g_win, "scroll-event", G_CALLBACK(on_scroll), NULL);
    g_signal_connect(g_win, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(g_win, "delete-event", G_CALLBACK(on_delete), NULL);
}

static void place_initial(void) {
    CatimeConfig *c = config_get();
    int x = c->window_pos_x, y = c->window_pos_y;
    if (!c->window_position_manual) {
        if (x < 0 || y < 0) {
            GdkRectangle geom;
            GdkMonitor *mon = gdk_display_get_monitor_at_point(
                gdk_display_get_default(), 0, 0);
            gdk_monitor_get_geometry(mon, &geom);
            int w, h;
            window_update_size();
            gtk_window_get_size(GTK_WINDOW(g_win), &w, &h);
            x = (x < 0) ? geom.x + (geom.width - w) / 2 : x;
            if (y < 0) y = geom.y + 24;
        }
    }
    gtk_window_move(GTK_WINDOW(g_win), x, y);
    g_win_x = x; g_win_y = y;
}

void window_apply_config(void) {
    if (!g_win) return;
    gtk_window_set_keep_above(GTK_WINDOW(g_win), config_get()->window_topmost);
    gtk_widget_set_opacity(g_win, 1.0); /* opacity via per-pixel alpha in render */
    apply_input_region();
    g_size_dirty = 1;
    window_invalidate();
}

void window_update_size(void) {
    if (!g_win) return;
    int w, h;
    render_get_size(&w, &h);
    /* set_size_request works for non-resizable windows (resize alone does
     * not) and guarantees the window exactly wraps the rendered text. */
    gtk_widget_set_size_request(g_win, w, h);
    gtk_window_resize(GTK_WINDOW(g_win), w, h);
    g_size_dirty = 0;
}

void window_invalidate(void) {
    if (!g_win) return;
    if (g_size_dirty) window_update_size();
    /* redraw only when the formatted text actually changes */
    char text[64];
    int has = timer_format(text, sizeof(text));
    if (!has) text[0] = '\0';
    if (strcmp(text, g_last_text) != 0) {
        snprintf(g_last_text, sizeof(g_last_text), "%s", text);
        gtk_widget_queue_draw(g_win);
    }
}

void window_show(void) {
    if (!g_win) return;
    if (g_size_dirty) window_update_size();
    if (!gtk_widget_get_realized(g_win)) {
        gtk_widget_realize(g_win);
        place_initial();
    }
    gtk_widget_show(g_win);
    g_visible = 1;
    window_apply_config();
}

void window_hide(void) {
    if (!g_win) return;
    gtk_widget_hide(g_win);
    g_visible = 0;
}

void window_toggle_visibility(void) {
    if (g_visible) window_hide();
    else window_show();
}

int window_get_visible(void) { return g_visible; }

void window_set_edit_mode(int on) {
    if (g_edit_mode == on) return;
    g_edit_mode = on;
    if (on) {
        gtk_window_set_accept_focus(GTK_WINDOW(g_win), TRUE);
        gtk_widget_grab_focus(g_win);
    } else {
        gtk_window_set_accept_focus(GTK_WINDOW(g_win), FALSE);
        g_dragging = 0;
    }
    apply_input_region();
    app_on_edit_mode_changed();
    LOG_INFO("edit mode %s", on ? "on" : "off");
}

int window_get_edit_mode(void) { return g_edit_mode; }

void window_set_topmost(int on) {
    if (!g_win) return;
    gtk_window_set_keep_above(GTK_WINDOW(g_win), on);
}

void window_save_position(void) {
    if (!g_win || !gtk_widget_get_realized(g_win)) return;
    int x, y;
    gtk_window_get_position(GTK_WINDOW(g_win), &x, &y);
    CatimeConfig *c = config_get();
    c->window_pos_x = x;
    c->window_pos_y = y;
}
