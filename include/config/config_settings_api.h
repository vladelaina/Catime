/**
 * @file config_settings_api.h
 * @brief Pomodoro, notification, language, hotkey, and onboarding settings.
 */

#ifndef CATIME_CONFIG_SETTINGS_API_H
#define CATIME_CONFIG_SETTINGS_API_H

#include <stddef.h>
#include <windows.h>
#include "config/config_types.h"

BOOL WriteConfigPomodoroTimes(int work, int shortBreak, int longBreak);
BOOL WriteConfigPomodoroSettings(int work, int shortBreak,
                                 int longBreak, int longBreak2);
BOOL WriteConfigPomodoroLoopCount(int loopCount);
BOOL WriteConfigTimeoutFile(const char* filePath);
BOOL WriteConfigTopmost(const char* topmost);
BOOL WriteConfigTimeoutWebsite(const char* url);
BOOL WriteConfigPomodoroTimeOptions(const int* times, int count);

BOOL WriteConfigNotificationTimeout(int timeoutMs);
BOOL WriteConfigNotificationOpacity(int opacity);
BOOL WriteConfigNotificationMessages(const char* timeoutMessage);
void WriteConfigNotificationType(NotificationType type);
void WriteConfigNotificationDisabled(BOOL disabled);
BOOL WriteConfigLanguage(int language);
void GetAudioFolderPath(char* path, size_t size);
void WriteConfigNotificationSound(const char* soundFile);
void WriteConfigNotificationVolume(int volume);
BOOL WriteConfigNotificationSettings(
    const char* timeoutMessage, int timeoutMs, int opacity,
    NotificationType type, int cornerRadius, int fontPercent, BOOL disabled,
    const char* soundFile, int volume, BOOL useForPomodoro);
BOOL WriteConfigNotificationWindow(int x, int y, int width, int height);

void HotkeyToString(WORD hotkey, char* buffer, size_t bufferSize);
WORD StringToHotkey(const char* value);
WORD NormalizeHotkeyValue(WORD hotkey);
BOOL IsHotkeyValueAllowed(WORD hotkey);
void ReadConfigHotkeys(
    WORD* showTimeHotkey, WORD* countUpHotkey, WORD* countdownHotkey,
    WORD* quickCountdown1Hotkey, WORD* quickCountdown2Hotkey,
    WORD* quickCountdown3Hotkey, WORD* pomodoroHotkey,
    WORD* toggleVisibilityHotkey, WORD* editModeHotkey,
    WORD* pauseResumeHotkey, WORD* restartTimerHotkey,
    WORD* toggleMillisecondsHotkey, WORD* toggleTopmostHotkey);
void ReadCustomCountdownHotkey(WORD* hotkey);
BOOL WriteConfigHotkeys(
    WORD showTimeHotkey, WORD countUpHotkey, WORD countdownHotkey,
    WORD customCountdownHotkey, WORD quickCountdown1Hotkey,
    WORD quickCountdown2Hotkey, WORD quickCountdown3Hotkey,
    WORD pomodoroHotkey, WORD toggleVisibilityHotkey, WORD editModeHotkey,
    WORD pauseResumeHotkey, WORD restartTimerHotkey,
    WORD toggleMillisecondsHotkey, WORD toggleTopmostHotkey);
BOOL WriteConfigKeyValue(const char* key, const char* value);
BOOL IsShortcutCheckDone(void);
BOOL SetShortcutCheckDone(BOOL done);

BOOL IsFirstRun(void);
BOOL SetFirstRunCompleted(void);
void SetFontLicenseAccepted(BOOL accepted);
void SetFontLicenseVersionAccepted(const char* version);
BOOL NeedsFontLicenseVersionAcceptance(void);
const char* GetCurrentFontLicenseVersion(void);

#endif /* CATIME_CONFIG_SETTINGS_API_H */
