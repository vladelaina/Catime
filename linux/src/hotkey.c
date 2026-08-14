/**
 * @file hotkey.c
 * @brief X11 global hotkeys via XGrabKey, driven by the GLib main loop.
 *
 * On a pure Wayland session (no X display) global hotkeys are unavailable;
 * the module degrades gracefully and logs a notice.
 */
#include "hotkey.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <glib.h>
#include <glib-unix.h>

#include "log.h"

#define MAX_GRABS 64

typedef struct {
    KeyCode keycode;
    unsigned int modifiers;
    HotkeyAction action;
} Grab;

static Display *g_dpy = NULL;
static int g_fd_source = 0;   /* GLib source id, 0 = none */
static HotkeyCallback g_cb = NULL;
static void *g_user = NULL;
static Grab g_grabs[MAX_GRABS];
static int g_grab_count = 0;

static unsigned int mod_for_token(const char *tok) {
    if (strcasecmp(tok, "ctrl") == 0 || strcasecmp(tok, "control") == 0)
        return ControlMask;
    if (strcasecmp(tok, "shift") == 0) return ShiftMask;
    if (strcasecmp(tok, "alt") == 0) return Mod1Mask;
    if (strcasecmp(tok, "super") == 0 || strcasecmp(tok, "win") == 0)
        return Mod4Mask;
    return 0;
}

static KeySym keysym_for_token(const char *tok) {
    if (!tok || !*tok) return NoSymbol;
    if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
        /* Windows VK hex code: not portable to X11. Skip. */
        return NoSymbol;
    }
    if (strlen(tok) == 1) {
        if (isdigit((unsigned char)tok[0]))
            return XStringToKeysym(tok);                 /* "1".."9" */
        char lower[2] = {(char)tolower((unsigned char)tok[0]), '\0'};
        return XStringToKeysym(lower);                   /* "k" */
    }
    /* F1..F24 */
    if ((tok[0] == 'f' || tok[0] == 'F') && isdigit((unsigned char)tok[1]))
        return XStringToKeysym(tok);
    /* named keys */
    struct { const char *name; KeySym sym; } named[] = {
        {"space", XK_space}, {"enter", XK_Return}, {"return", XK_Return},
        {"esc", XK_Escape}, {"escape", XK_Escape}, {"tab", XK_Tab},
        {"backspace", XK_BackSpace}, {"insert", XK_Insert}, {"delete", XK_Delete},
        {"home", XK_Home}, {"end", XK_End},
        {"pageup", XK_Page_Up}, {"page_up", XK_Page_Up},
        {"pagedown", XK_Page_Down}, {"page_down", XK_Page_Down},
        {"left", XK_Left}, {"right", XK_Right}, {"up", XK_Up}, {"down", XK_Down},
        {NULL, NoSymbol}
    };
    char buf[24];
    snprintf(buf, sizeof(buf), "%s", tok);
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (int i = 0; named[i].name; i++)
        if (strcmp(buf, named[i].name) == 0) return named[i].sym;
    return XStringToKeysym(tok);
}

static void ungrab_all(void) {
    if (!g_dpy) return;
    Window root = DefaultRootWindow(g_dpy);
    for (int i = 0; i < g_grab_count; i++)
        XUngrabKey(g_dpy, g_grabs[i].keycode, g_grabs[i].modifiers, root);
    g_grab_count = 0;
}

static void apply_grab(unsigned int base_mod, KeyCode kc, HotkeyAction action) {
    if (!kc || g_grab_count >= MAX_GRABS) return;
    Window root = DefaultRootWindow(g_dpy);
    /* grab with the common lock-key combinations so NumLock/CapsLock don't
     * silently break the hotkey. */
    unsigned int variants[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
    for (size_t v = 0; v < sizeof(variants) / sizeof(variants[0]); v++) {
        unsigned int mod = base_mod | variants[v];
        XGrabKey(g_dpy, kc, mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    g_grabs[g_grab_count].keycode = kc;
    g_grabs[g_grab_count].modifiers = base_mod;
    g_grabs[g_grab_count].action = action;
    g_grab_count++;
}

static void load_and_grab(void) {
    ungrab_all();
    CatimeConfig *c = config_get();
    for (int a = 0; a < HK_COUNT; a++) {
        const char *spec = c->hotkeys[a];
        if (!spec || !*spec || strcasecmp(spec, "none") == 0) continue;
        /* split on '+' */
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", spec);
        unsigned int mods = 0;
        char *save = NULL;
        char *tok = strtok_r(buf, "+", &save);
        KeySym ks = NoSymbol;
        while (tok) {
            /* trim spaces */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && end[-1] == ' ') *--end = '\0';
            unsigned int m = mod_for_token(tok);
            if (m) {
                mods |= m;
                tok = strtok_r(NULL, "+", &save);
                continue;
            }
            /* last token is the key */
            ks = keysym_for_token(tok);
            tok = strtok_r(NULL, "+", &save);
        }
        if (ks == NoSymbol) continue;
        KeyCode kc = XKeysymToKeycode(g_dpy, ks);
        if (!kc) continue;
        apply_grab(mods, kc, (HotkeyAction)a);
    }
    XSync(g_dpy, False);
    LOG_INFO("hotkey: registered %d global hotkey(s)", g_grab_count);
}

static gboolean on_x_ready(gint fd, GIOCondition cond, gpointer user) {
    (void)fd; (void)cond; (void)user;
    if (!g_dpy) return G_SOURCE_REMOVE;
    while (XPending(g_dpy)) {
        XEvent ev;
        XNextEvent(g_dpy, &ev);
        if (ev.type != KeyPress) continue;
        unsigned int state = ev.xkey.state &
            (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
        KeyCode kc = ev.xkey.keycode;
        for (int i = 0; i < g_grab_count; i++) {
            if (g_grabs[i].keycode == kc && g_grabs[i].modifiers == state) {
                if (g_cb) g_cb(g_grabs[i].action, g_user);
                break;
            }
        }
    }
    return G_SOURCE_CONTINUE;
}

int hotkey_init(HotkeyCallback cb, void *user) {
    g_cb = cb;
    g_user = user;

    if (getenv("WAYLAND_DISPLAY") && !getenv("DISPLAY")) {
        LOG_INFO("hotkey: Wayland session without X; global hotkeys disabled");
        return 1;
    }
    g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) {
        LOG_INFO("hotkey: no X display; global hotkeys disabled");
        return 1;
    }
    if (getenv("WAYLAND_DISPLAY")) {
        LOG_INFO("hotkey: Wayland session detected; global hotkeys may be limited");
    }

    Window root = DefaultRootWindow(g_dpy);
    XSelectInput(g_dpy, root, KeyPressMask);

    load_and_grab();

    g_fd_source = g_unix_fd_add(ConnectionNumber(g_dpy), G_IO_IN, on_x_ready, NULL);
    return 0;
}

void hotkey_reload(void) {
    if (!g_dpy) return;
    load_and_grab();
}

void hotkey_shutdown(void) {
    if (g_fd_source) {
        g_source_remove(g_fd_source);
        g_fd_source = 0;
    }
    ungrab_all();
    if (g_dpy) {
        XCloseDisplay(g_dpy);
        g_dpy = NULL;
    }
}

int hotkey_available(void) { return g_dpy != NULL; }
