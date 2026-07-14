#ifndef STARTUP_POLICY_H
#define STARTUP_POLICY_H

#include <windows.h>
#include <stddef.h>

#define AUTO_START_PREFERENCE_DEFAULT "DEFAULT"
#define AUTO_START_PREFERENCE_ENABLED "ENABLED"
#define AUTO_START_PREFERENCE_DISABLED "DISABLED"

typedef enum {
    AUTO_START_PREFERENCE_STATE_DEFAULT = 0,
    AUTO_START_PREFERENCE_STATE_ENABLED,
    AUTO_START_PREFERENCE_STATE_DISABLED
} AutoStartPreference;

AutoStartPreference StartupPolicy_ParsePreference(const char* value);
const char* StartupPolicy_PreferenceName(AutoStartPreference preference);
BOOL StartupPolicy_ShouldEnable(AutoStartPreference preference,
                                BOOL isFirstRun, BOOL shortcutExists);
BOOL StartupPolicy_IsWindowsStartupDisabled(const unsigned char* data,
                                            size_t dataSize);

#endif /* STARTUP_POLICY_H */
