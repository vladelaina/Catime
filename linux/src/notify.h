/**
 * @file notify.h
 * @brief Desktop notifications via libnotify.
 *
 * Prefixed `catime_notify_*` to avoid colliding with libnotify's own
 * `notify_init`/`notify_uninit` symbols.
 */
#ifndef CATIME_LINUX_NOTIFY_H
#define CATIME_LINUX_NOTIFY_H

/** Initialize libnotify with the given app name. Returns 0 on success. */
int catime_notify_init(const char *app_name);

/** Show a notification (call from the main thread). */
void catime_notify_show(const char *summary, const char *body, int timeout_ms);

void catime_notify_uninit(void);

#endif /* CATIME_LINUX_NOTIFY_H */
