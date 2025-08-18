#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <time.h>
#include "../resource/resource.h"
#include "language.h"
#include "font.h"
#include "color.h"
#include "tray.h"
#include "tray_menu.h"
#include "timer.h"
#include "window.h"
#include "startup.h"

#define MAX_RECENT_FILES 5

#define INI_SECTION_GENERAL       "General"
#define INI_SECTION_DISPLAY       "Display"
#define INI_SECTION_TIMER         "Timer"
#define INI_SECTION_POMODORO      "Pomodoro"
#define INI_SECTION_NOTIFICATION  "Notification"
#define INI_SECTION_HOTKEYS       "Hotkeys"
#define INI_SECTION_RECENTFILES   "RecentFiles"
#define INI_SECTION_COLORS        "Colors"
#define INI_SECTION_OPTIONS       "Options"

typedef struct {
    char path[MAX_PATH];
    char name[MAX_PATH];
} RecentFile;

extern RecentFile CLOCK_RECENT_FILES[MAX_RECENT_FILES];
extern int CLOCK_RECENT_FILES_COUNT;
extern int CLOCK_DEFAULT_START_TIME;
extern time_t last_config_time;
extern int POMODORO_WORK_TIME;
extern int POMODORO_SHORT_BREAK;
extern int POMODORO_LONG_BREAK;

extern char CLOCK_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_TIMEOUT_MESSAGE_TEXT[100];
extern char POMODORO_CYCLE_COMPLETE_TEXT[100];

extern int NOTIFICATION_TIMEOUT_MS;

typedef enum {
    NOTIFICATION_TYPE_CATIME = 0,
    NOTIFICATION_TYPE_SYSTEM_MODAL,
    NOTIFICATION_TYPE_OS
} NotificationType;

extern NotificationType NOTIFICATION_TYPE;

extern BOOL NOTIFICATION_DISABLED;

extern char NOTIFICATION_SOUND_FILE[MAX_PATH];

extern int NOTIFICATION_SOUND_VOLUME;

void GetConfigPath(char* path, size_t size);

void ReadConfig();

void CheckAndCreateAudioFolder();

void WriteConfigTimeoutAction(const char* action);

void WriteConfigTimeOptions(const char* options);

void LoadRecentFiles(void);

void SaveRecentFile(const char* filePath);

char* UTF8ToANSI(const char* utf8Str);

void CreateDefaultConfig(const char* config_path);

void WriteConfig(const char* config_path);

void WriteConfigPomodoroTimes(int work, int short_break, int long_break);

void WriteConfigPomodoroSettings(int work_time, int short_break, int long_break);

void WriteConfigPomodoroLoopCount(int loop_count);

void WriteConfigTimeoutFile(const char* filePath);

void WriteConfigTopmost(const char* topmost);

void WriteConfigTimeoutWebsite(const char* url);

void WriteConfigPomodoroTimeOptions(int* times, int count);

void ReadNotificationMessagesConfig(void);

void WriteConfigNotificationTimeout(int timeout_ms);

void ReadNotificationTimeoutConfig(void);

void ReadNotificationOpacityConfig(void);

void WriteConfigNotificationOpacity(int opacity);

void WriteConfigNotificationMessages(const char* timeout_msg, const char* pomodoro_msg, const char* cycle_complete_msg);

void ReadNotificationTypeConfig(void);

void WriteConfigNotificationType(NotificationType type);

void ReadNotificationDisabledConfig(void);

void WriteConfigNotificationDisabled(BOOL disabled);

void WriteConfigLanguage(int language);

void GetAudioFolderPath(char* path, size_t size);

void ReadNotificationSoundConfig(void);

void WriteConfigNotificationSound(const char* sound_file);

void ReadNotificationVolumeConfig(void);

void WriteConfigNotificationVolume(int volume);

void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);

WORD StringToHotkey(const char* str);

void ReadConfigHotkeys(WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
                      WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey, WORD* quickCountdown3Hotkey,
                      WORD* pomodoroHotkey, WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
                      WORD* pauseResumeHotkey, WORD* restartTimerHotkey);

void ReadCustomCountdownHotkey(WORD* hotkey);

void WriteConfigHotkeys(WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
                        WORD quickCountdown1Hotkey, WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
                        WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
                        WORD pauseResumeHotkey, WORD restartTimerHotkey);

void WriteConfigKeyValue(const char* key, const char* value);

bool IsShortcutCheckDone(void);

void SetShortcutCheckDone(bool done);

DWORD ReadIniString(const char* section, const char* key, const char* defaultValue,
                  char* returnValue, DWORD returnSize, const char* filePath);

BOOL WriteIniString(const char* section, const char* key, const char* value,
                  const char* filePath);

int ReadIniInt(const char* section, const char* key, int defaultValue, 
             const char* filePath);

BOOL WriteIniInt(const char* section, const char* key, int value,
               const char* filePath);

#endif