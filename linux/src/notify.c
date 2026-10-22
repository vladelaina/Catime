/**
 * @file notify.c
 * @brief libnotify wrapper.
 */
#include "notify.h"

#include <libnotify/notify.h>

#include "log.h"

static gboolean g_init = FALSE;
static NotifyNotification *g_last = NULL;

int catime_notify_init(const char *app_name) {
    if (g_init) return 0;
    g_init = notify_init(app_name);
    if (!g_init) {
        LOG_WARNING("notify: libnotify init failed");
        return -1;
    }
    return 0;
}

void catime_notify_show(const char *summary, const char *body, int timeout_ms) {
    if (!g_init) return;
    if (g_last) {
        notify_notification_close(g_last, NULL);
        g_object_unref(g_last);
        g_last = NULL;
    }
    NotifyNotification *n = notify_notification_new(summary, body ? body : "",
                                                    "catime");
    if (timeout_ms > 0)
        notify_notification_set_timeout(n, timeout_ms);
    GError *err = NULL;
    if (!notify_notification_show(n, &err)) {
        LOG_WARNING("notify: show failed: %s", err ? err->message : "?");
        if (err) g_error_free(err);
    }
    g_last = n;
}

void catime_notify_uninit(void) {
    if (g_last) { g_object_unref(g_last); g_last = NULL; }
    /* notify_uninit() is safe to call when initialized */
}
