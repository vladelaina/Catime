#include "startup_policy.h"

#include <string.h>

AutoStartPreference StartupPolicy_ParsePreference(const char* value) {
    if (value && _stricmp(value, AUTO_START_PREFERENCE_ENABLED) == 0) {
        return AUTO_START_PREFERENCE_STATE_ENABLED;
    }
    if (value && _stricmp(value, AUTO_START_PREFERENCE_DISABLED) == 0) {
        return AUTO_START_PREFERENCE_STATE_DISABLED;
    }
    return AUTO_START_PREFERENCE_STATE_DEFAULT;
}

const char* StartupPolicy_PreferenceName(AutoStartPreference preference) {
    switch (preference) {
        case AUTO_START_PREFERENCE_STATE_ENABLED:
            return AUTO_START_PREFERENCE_ENABLED;
        case AUTO_START_PREFERENCE_STATE_DISABLED:
            return AUTO_START_PREFERENCE_DISABLED;
        default:
            return AUTO_START_PREFERENCE_DEFAULT;
    }
}

BOOL StartupPolicy_ShouldEnable(AutoStartPreference preference,
                                BOOL isFirstRun, BOOL shortcutExists) {
    switch (preference) {
        case AUTO_START_PREFERENCE_STATE_ENABLED:
            return TRUE;
        case AUTO_START_PREFERENCE_STATE_DISABLED:
            return FALSE;
        default:
            return isFirstRun || shortcutExists;
    }
}
