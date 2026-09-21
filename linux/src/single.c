/**
 * @file single.c
 * @brief Single-instance + CLI forwarding via a Unix domain socket.
 */
#include "single.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <glib.h>
#include <glib-unix.h>

#include "log.h"

static int g_listen_fd = -1;
static guint g_watch = 0;
static SingleMessageCb g_cb = NULL;

static const char *socket_path(void) {
    static char path[256];
    const char *rtd = getenv("XDG_RUNTIME_DIR");
    if (rtd && rtd[0]) {
        snprintf(path, sizeof(path), "%s/catime.sock", rtd);
    } else {
        snprintf(path, sizeof(path), "/tmp/catime-%d.sock", getuid());
    }
    return path;
}

static gboolean on_incoming(gint fd, GIOCondition cond, gpointer user) {
    (void)cond; (void)user;
    int c = accept(fd, NULL, NULL);
    if (c < 0) return G_SOURCE_CONTINUE;

    /* read up to 8KB */
    char buf[8192];
    ssize_t n = read(c, buf, sizeof(buf) - 1);
    close(c);
    if (n <= 0) return G_SOURCE_CONTINUE;
    buf[n] = '\0';

    /* split on whitespace into tokens */
    char *tokens[128];
    int ntok = 0;
    char *save = NULL;
    char *tok = strtok_r(buf, " \t\r\n", &save);
    while (tok && ntok < 128) {
        tokens[ntok++] = tok;
        tok = strtok_r(NULL, " \t\r\n", &save);
    }
    if (ntok > 0 && g_cb) g_cb(tokens, ntok);
    return G_SOURCE_CONTINUE;
}

int single_instance_init(SingleMessageCb cb, int argc, char **argv) {
    g_cb = cb;
    const char *path = socket_path();

    if (strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path)) {
        LOG_ERROR("single: socket path too long: %s", path);
        return -1;
    }

    /* try to connect as a client */
    int c = socket(AF_UNIX, SOCK_STREAM, 0);
    if (c >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, path, strlen(path) + 1); /* length guarded above */
        if (connect(c, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            /* forward argv */
            char buf[4096];
            size_t off = 0;
            for (int i = 1; i < argc && off + 2 < sizeof(buf); i++) {
                if (argv[i][0] == '-' && argv[i][1] == '-') continue; /* skip --flags */
                int w = snprintf(buf + off, sizeof(buf) - off, "%s%s",
                                 off ? " " : "", argv[i]);
                if (w < 0 || off + (size_t)w >= sizeof(buf)) break;
                off += (size_t)w;
            }
            snprintf(buf + off, sizeof(buf) - off, "\n");
            ssize_t wr = write(c, buf, strlen(buf)); (void)wr;
            close(c);
            return 1; /* forwarded, caller should exit */
        }
        close(c);
    }

    /* become the server */
    unlink(path);
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) { LOG_ERROR("single: socket: %m"); return -1; }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1); /* length guarded above */
    mode_t old = umask(0077);
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        umask(old);
        LOG_ERROR("single: bind %s: %m", path);
        close(s);
        return -1;
    }
    umask(old);
    if (listen(s, 4) != 0) {
        LOG_ERROR("single: listen: %m");
        close(s);
        return -1;
    }
    g_listen_fd = s;
    g_watch = g_unix_fd_add(s, G_IO_IN, on_incoming, NULL);
    LOG_INFO("single: listening on %s", path);
    return 0;
}

void single_instance_shutdown(void) {
    if (g_watch) { g_source_remove(g_watch); g_watch = 0; }
    if (g_listen_fd >= 0) { close(g_listen_fd); g_listen_fd = -1; }
    const char *path = socket_path();
    unlink(path);
}
