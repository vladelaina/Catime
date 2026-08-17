/**
 * @file log.c
 * @brief Logging implementation for the Catime Linux port.
 */
#include "log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "paths.h"

static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static CatimeLogLevel g_level = CATIME_LOG_INFO;

static const char *level_str(CatimeLogLevel level) {
    switch (level) {
        case CATIME_LOG_DEBUG:   return "DEBUG";
        case CATIME_LOG_INFO:    return "INFO";
        case CATIME_LOG_WARNING: return "WARN";
        case CATIME_LOG_ERROR:   return "ERROR";
    }
    return "?";
}

void catime_log_set_level(CatimeLogLevel level) { g_level = level; }

void catime_log_init(int enable_file) {
    if (!enable_file) return;
    char path[1024];
    if (paths_join(path, sizeof(path), paths_config_dir(), "catime.log") == 0) {
        paths_ensure_parent_dir(path);
        g_log_file = fopen(path, "a");
    }
}

void catime_log_shutdown(void) {
    pthread_mutex_lock(&g_log_mutex);
    if (g_log_file) {
        fflush(g_log_file);
        fclose(g_log_file);
        g_log_file = NULL;
    }
    pthread_mutex_unlock(&g_log_mutex);
}

void catime_log_write(CatimeLogLevel level, const char *file, int line,
                      const char *fmt, ...) {
    if (level < g_level) return;

    char body[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

    const char *base = strrchr(file, '/');
    base = base ? base + 1 : file;

    pthread_mutex_lock(&g_log_mutex);
    fprintf(stderr, "[%s] [%s] %s:%d: %s\n", ts, level_str(level), base, line, body);
    fflush(stderr);
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] %s:%d: %s\n", ts, level_str(level), base, line, body);
        fflush(g_log_file);
    }
    pthread_mutex_unlock(&g_log_mutex);
}
