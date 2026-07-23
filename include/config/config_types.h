/**
 * @file config_types.h
 * @brief Public configuration enums and in-memory state model.
 */

#ifndef CATIME_CONFIG_TYPES_H
#define CATIME_CONFIG_TYPES_H

#include <windows.h>
#include <time.h>
#include "config/config_constants.h"

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,
    NOTIFICATION_TYPE_SYSTEM_MODAL,
    NOTIFICATION_TYPE_OS
} NotificationType;

typedef enum {
    ANIMATION_SPEED_ORIGINAL = 0,
    ANIMATION_SPEED_MEMORY = 1,
    ANIMATION_SPEED_CPU = 2,
    ANIMATION_SPEED_TIMER = 3,
    ANIMATION_SPEED_FIXED = 4
} AnimationSpeedMetric;

#ifndef TIME_FORMAT_TYPE_DEFINED
typedef enum {
    TIME_FORMAT_DEFAULT = 0,
    TIME_FORMAT_ZERO_PADDED = 1,
    TIME_FORMAT_FULL_PADDED = 2
} TimeFormatType;
#define TIME_FORMAT_TYPE_DEFINED
#endif

typedef struct {
    RecentFile files[MAX_RECENT_FILES];
    int count;
} RecentFilesState;

typedef struct {
    int work_time;
    int short_break;
    int long_break;
    int times[10];
    int times_count;
    int loop_count;
} PomodoroConfig;

typedef struct {
    char timeout_message[NOTIFICATION_MESSAGE_BUFFER_SIZE];
} NotificationMessages;

typedef struct {
    int timeout_ms;
    int max_opacity;
    int corner_radius;
    int font_size;
    NotificationType type;
    BOOL disabled;
    int window_x;
    int window_y;
    int window_width;
    int window_height;
} NotificationDisplay;

typedef struct {
    char sound_file[MAX_PATH];
    int volume;
} NotificationSound;

typedef struct {
    NotificationMessages messages;
    NotificationDisplay display;
    NotificationSound sound;
} NotificationConfig;

typedef struct {
    BOOL accepted;
    char version_accepted[16];
} FontLicenseState;

typedef struct {
    char path[MAX_PATH];
    char sha256[65];
} PluginTrustEntry;

typedef struct {
    PluginTrustEntry entries[MAX_TRUSTED_PLUGINS];
    int count;
} PluginTrustState;

typedef struct {
    TimeFormatType format;
    BOOL show_milliseconds;
} TimeFormatConfig;

typedef struct {
    TimeFormatConfig time_format;
    int move_step_small;
    int move_step_large;
    int opacity_step_normal;
    int opacity_step_fast;
    int scale_step_normal;
    int scale_step_fast;
    int text_effect;
} DisplayConfig;

typedef struct {
    int default_start_time;
} TimerState;

typedef struct {
    RecentFilesState recent_files;
    PomodoroConfig pomodoro;
    NotificationConfig notification;
    FontLicenseState font_license;
    PluginTrustState plugin_trust;
    DisplayConfig display;
    TimerState timer;
    time_t last_config_time;
} AppConfig;

extern AppConfig g_AppConfig;
extern BOOL g_PerformFactoryReset;

void InitializeAppConfigDefaults(void);

#endif /* CATIME_CONFIG_TYPES_H */
