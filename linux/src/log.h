/**
 * @file log.h
 * @brief Minimal logging for the Catime Linux port.
 *
 * Mirrors the spirit of the Windows build's log.h API (LOG_INFO/LOG_WARNING/...)
 * but writes to stderr and an optional rotating log file under the config dir.
 */
#ifndef CATIME_LINUX_LOG_H
#define CATIME_LINUX_LOG_H

#include <stdarg.h>

typedef enum {
    CATIME_LOG_DEBUG = 0,
    CATIME_LOG_INFO,
    CATIME_LOG_WARNING,
    CATIME_LOG_ERROR
} CatimeLogLevel;

void catime_log_init(int enable_file);
void catime_log_shutdown(void);
void catime_log_set_level(CatimeLogLevel level);

void catime_log_write(CatimeLogLevel level, const char *file, int line,
                      const char *fmt, ...);

#define LOG_DEBUG(fmt, ...) \
    catime_log_write(CATIME_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    catime_log_write(CATIME_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) \
    catime_log_write(CATIME_LOG_WARNING, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
    catime_log_write(CATIME_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif /* CATIME_LINUX_LOG_H */
