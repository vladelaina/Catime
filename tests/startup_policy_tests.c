#include "startup_policy.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void Expect(const char* name, BOOL actual, BOOL expected) {
    if (!!actual != !!expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

int main(void) {
    Expect("new install defaults enabled",
           StartupPolicy_ShouldEnable(AUTO_START_PREFERENCE_STATE_DEFAULT,
                                      TRUE, FALSE), TRUE);
    Expect("legacy enabled state is adopted",
           StartupPolicy_ShouldEnable(AUTO_START_PREFERENCE_STATE_DEFAULT,
                                      FALSE, TRUE), TRUE);
    Expect("legacy opt-out is preserved",
           StartupPolicy_ShouldEnable(AUTO_START_PREFERENCE_STATE_DEFAULT,
                                      FALSE, FALSE), FALSE);
    Expect("explicit enable repairs missing shortcut",
           StartupPolicy_ShouldEnable(AUTO_START_PREFERENCE_STATE_ENABLED,
                                      FALSE, FALSE), TRUE);
    Expect("explicit disable wins over stale shortcut",
           StartupPolicy_ShouldEnable(AUTO_START_PREFERENCE_STATE_DISABLED,
                                      TRUE, TRUE), FALSE);
    Expect("invalid preference migrates as default",
           StartupPolicy_ParsePreference("corrupt") ==
               AUTO_START_PREFERENCE_STATE_DEFAULT,
           TRUE);
    Expect("preference parsing is case insensitive",
           StartupPolicy_ParsePreference("enabled") ==
               AUTO_START_PREFERENCE_STATE_ENABLED,
           TRUE);
    Expect("disabled preference serializes canonically",
           strcmp(StartupPolicy_PreferenceName(
                      AUTO_START_PREFERENCE_STATE_DISABLED),
                  AUTO_START_PREFERENCE_DISABLED) == 0,
           TRUE);

    if (failures == 0) {
        puts("startup policy tests passed");
    }
    return failures == 0 ? 0 : 1;
}
