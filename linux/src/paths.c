/**
 * @file paths.c
 * @brief XDG-aware path helpers and data-file discovery.
 */
#include "paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"

static char g_config_dir[1024];
static char g_data_dir[1024];
static int g_initialized = 0;

static void init_dirs(void) {
    if (g_initialized) return;
    g_initialized = 1;

    const char *xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0] == '/') {
        snprintf(g_config_dir, sizeof(g_config_dir), "%s/catime", xdg_config);
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/tmp";
        snprintf(g_config_dir, sizeof(g_config_dir), "%s/.config/catime", home);
    }

    const char *xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data && xdg_data[0] == '/') {
        snprintf(g_data_dir, sizeof(g_data_dir), "%s/catime", xdg_data);
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/tmp";
        snprintf(g_data_dir, sizeof(g_data_dir), "%s/.local/share/catime", home);
    }
}

const char *paths_config_dir(void) {
    init_dirs();
    return g_config_dir;
}

const char *paths_data_dir(void) {
    init_dirs();
    return g_data_dir;
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                LOG_WARNING("mkdir(%s) failed: %s", tmp, strerror(errno));
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        LOG_WARNING("mkdir(%s) failed: %s", tmp, strerror(errno));
        return -1;
    }
    return 0;
}

void paths_ensure_dirs(void) {
    init_dirs();
    mkdir_p(g_config_dir);
    mkdir_p(g_data_dir);
}

int paths_join(char *out, size_t out_size, const char *a, const char *b) {
    int n = snprintf(out, out_size, "%s/%s", a, b);
    if (n < 0 || (size_t)n >= out_size) return -1;
    return 0;
}

int paths_ensure_parent_dir(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char *slash = strrchr(tmp, '/');
    if (!slash) return 0;
    *slash = '\0';
    return mkdir_p(tmp);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void exe_dir(char *out, size_t out_size) {
    ssize_t n = readlink("/proc/self/exe", out, out_size - 1);
    if (n <= 0) {
        snprintf(out, out_size, ".");
        return;
    }
    out[n] = '\0';
    char *slash = strrchr(out, '/');
    if (slash) *slash = '\0';
}

const char *paths_find_data(const char *rel) {
    static char buf[1200];
    char prefix[1100];
    init_dirs();

    char edir[1024];
    exe_dir(edir, sizeof(edir));

    /* Candidate base directories, in priority order. */
    const char *bases[6];
    char b2[1100], b5[1100], b6[1100];
    paths_join(b2, sizeof(b2), edir, "../share/catime");
    paths_join(b5, sizeof(b5), edir, "../..");
    paths_join(b6, sizeof(b6), edir, "../../resource");
    bases[0] = g_data_dir;
    bases[1] = b2;
    bases[2] = "/usr/local/share/catime";
    bases[3] = "/usr/share/catime";
    bases[4] = b5;
    bases[5] = b6;

    for (int i = 0; i < 6; i++) {
        if (!bases[i] || !bases[i][0]) continue;
        if (paths_join(prefix, sizeof(prefix), bases[i], rel) != 0) continue;
        if (file_exists(prefix)) {
            snprintf(buf, sizeof(buf), "%s", prefix);
            return buf;
        }
    }
    return NULL;
}

