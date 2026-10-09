/**
 * @file main.c
 * @brief Entry point for the Catime Linux port.
 */
#include <cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app.h"
#include "config.h"
#include "i18n.h"
#include "log.h"
#include "paths.h"
#include "render.h"
#include "single.h"
#include "timer.h"

static void on_forwarded(char **tokens, int n) {
    app_run_cli_tokens(tokens, n);
}

int main(int argc, char **argv) {
    /* Handle global help/version first. */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            app_print_help();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            printf("Catime %s (Linux)\n", CATIME_VERSION_STRING);
            return 0;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            timer_self_test();
            return 0;
        }
        if (strcmp(argv[i], "--render-png") == 0 && i + 1 < argc) {
            /* Headless render using the production Cairo/Pango path. */
            paths_ensure_dirs();
            config_load();
            i18n_set_language(config_get()->language);
            render_load();
            timer_init();
            timer_start_countdown(1500); /* "25:00" */
            int w, h;
            render_get_size(&w, &h);
            cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_t *cr = cairo_create(surf);
            /* dark background so the (transparent-on-desktop) text is visible */
            cairo_set_source_rgb(cr, 0.05, 0.05, 0.08);
            cairo_paint(cr);
            char text[64];
            timer_format(text, sizeof(text));
            render_draw(cr, w, h, text);
            cairo_status_t st = cairo_surface_write_to_png(surf, argv[i + 1]);
            cairo_destroy(cr);
            cairo_surface_destroy(surf);
            printf("rendered '%s' (%dx%d) to %s [%s]\n", text, w, h, argv[i + 1],
                   st == CAIRO_STATUS_SUCCESS ? "ok" : cairo_status_to_string(st));
            return st == CAIRO_STATUS_SUCCESS ? 0 : 1;
        }
    }

    /* Become (or forward to) the single instance BEFORE gtk_init, so that CLI
     * commands can be sent from a terminal that has no display server. */
    int r = single_instance_init(on_forwarded, argc, argv);
    if (r == 1) return 0;   /* forwarded CLI to the running instance */
    /* r == 0: we are the single instance; r < 0: proceed without it */

    gtk_init(&argc, &argv);

    app_bootstrap(argc, argv);
    app_run();

    single_instance_shutdown();
    catime_log_shutdown();
    return 0;
}
