/**
 * @file render.c
 * @brief Cairo/Pango text rendering with solid color or multi-stop gradient.
 */
#include "render.h"

#include <ctype.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define MAX_STOPS 8

static PangoFontDescription *g_font = NULL;
static double g_stops[MAX_STOPS][3];
static int g_stop_count = 1;
static int g_cached_w = 200, g_cached_h = 100;
static char g_color_key[128];
static char g_font_key[160];

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* parse "#RRGGBB" -> r,g,b in [0,1]; returns 0 on success */
static int parse_hex_color(const char *s, double *r, double *g, double *b) {
    if (*s == '#') s++;
    if (strlen(s) < 6) return -1;
    int rv = hex_digit(s[0]) * 16 + hex_digit(s[1]);
    int gv = hex_digit(s[2]) * 16 + hex_digit(s[3]);
    int bv = hex_digit(s[4]) * 16 + hex_digit(s[5]);
    if (rv < 0 || gv < 0 || bv < 0) return -1;
    *r = rv / 255.0; *g = gv / 255.0; *b = bv / 255.0;
    return 0;
}

static void parse_color_string(const char *spec) {
    g_stop_count = 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", spec);
    char *save = NULL;
    char *tok = strtok_r(buf, "_", &save);
    while (tok && g_stop_count < MAX_STOPS) {
        while (*tok == ' ' || *tok == ',') tok++;
        double r, g, b;
        if (parse_hex_color(tok, &r, &g, &b) == 0) {
            g_stops[g_stop_count][0] = r;
            g_stops[g_stop_count][1] = g;
            g_stops[g_stop_count][2] = b;
            g_stop_count++;
        }
        tok = strtok_r(NULL, "_", &save);
    }
    if (g_stop_count == 0) {
        /* fallback: white */
        g_stops[0][0] = g_stops[0][1] = g_stops[0][2] = 1.0;
        g_stop_count = 1;
    }
}

void render_load(void) {
    CatimeConfig *c = config_get();

    /* font */
    char fkey[160];
    int px = (int)(c->base_font_size * c->window_scale);
    if (px < 8) px = 8;
    snprintf(fkey, sizeof(fkey), "%s|%d", c->font_family[0] ? c->font_family : "monospace", px);
    if (g_font == NULL || strcmp(fkey, g_font_key) != 0) {
        snprintf(g_font_key, sizeof(g_font_key), "%s", fkey);
        if (g_font) pango_font_description_free(g_font);
        g_font = pango_font_description_from_string(
            c->font_family[0] ? c->font_family : "monospace");
        pango_font_description_set_absolute_size(g_font, px * PANGO_SCALE);
    }

    /* color */
    if (strcmp(c->text_color, g_color_key) != 0) {
        snprintf(g_color_key, sizeof(g_color_key), "%s", c->text_color);
        parse_color_string(c->text_color);
    }
}

static void measure_ref(PangoLayout *layout) {
    /* stable reference width: full hh:mm:ss(.cc) so resizing is rare */
    const char *ref = config_get()->show_milliseconds ? "00:00:00.00" : "00:00:00";
    pango_layout_set_text(layout, ref, -1);
    int w, h;
    pango_layout_get_pixel_size(layout, &w, &h);
    int margin = (int)(config_get()->base_font_size * 0.4 * config_get()->window_scale);
    if (margin < 6) margin = 6;
    g_cached_w = w + margin * 2;
    g_cached_h = h + margin;
}

void render_get_size(int *w, int *h) {
    render_load();
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t *cr = cairo_create(surf);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, g_font);
    measure_ref(layout);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    if (w) *w = g_cached_w;
    if (h) *h = g_cached_h;
}

void render_draw(cairo_t *cr, int w, int h, const char *text) {
    render_load();
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, g_font);
    pango_layout_set_text(layout, text ? text : "", -1);

    int tw, th;
    pango_layout_get_pixel_size(layout, &tw, &th);

    /* Per-pixel alpha = window opacity. Applied here (not via gtk opacity) so
     * it works regardless of compositor support for toplevel opacity. */
    double alpha = config_get()->window_opacity / 100.0;
    if (alpha > 1.0) alpha = 1.0;
    if (alpha < 0.0) alpha = 0.0;

    if (g_stop_count >= 2) {
        cairo_pattern_t *pat = cairo_pattern_create_linear(0, 0, tw, 0);
        for (int i = 0; i < g_stop_count; i++) {
            double off = g_stop_count == 1 ? 0.0 : (double)i / (g_stop_count - 1);
            cairo_pattern_add_color_stop_rgba(pat, off,
                g_stops[i][0], g_stops[i][1], g_stops[i][2], alpha);
        }
        cairo_set_source(cr, pat);
        cairo_move_to(cr, (w - tw) / 2.0, (h - th) / 2.0);
        pango_cairo_show_layout(cr, layout);
        cairo_pattern_destroy(pat);
    } else {
        cairo_set_source_rgba(cr, g_stops[0][0], g_stops[0][1], g_stops[0][2], alpha);
        cairo_move_to(cr, (w - tw) / 2.0, (h - th) / 2.0);
        pango_cairo_show_layout(cr, layout);
    }
    g_object_unref(layout);
}
