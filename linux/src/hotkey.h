/**
 * @file hotkey.h
 * @brief Global hotkeys via X11 XGrabKey (gracefully unavailable on Wayland).
 */
#ifndef CATIME_LINUX_HOTKEY_H
#define CATIME_LINUX_HOTKEY_H

#include "config.h"

typedef void (*HotkeyCallback)(HotkeyAction action, void *user);

/**
 * Initialize global hotkeys from config. Integrates with the running GLib
 * main loop. Returns 0 if active, 1 if unavailable (Wayland / no X display),
 * -1 on error.
 */
int hotkey_init(HotkeyCallback cb, void *user);

/** Re-read config and re-grab all hotkeys. */
void hotkey_reload(void);

void hotkey_shutdown(void);

/** Whether X11 global hotkeys are currently active. */
int hotkey_available(void);

#endif /* CATIME_LINUX_HOTKEY_H */
