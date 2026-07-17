#include "utils/render_retry.h"

#include <stdio.h>

static int g_failures = 0;

static void Expect(const char* name, BOOL value) {
    if (!value) {
        fprintf(stderr, "%s\n", name);
        g_failures++;
    }
}

int main(void) {
    RenderRetryController controller = {0};

    Expect("fresh controller should be inactive", !RenderRetry_IsActive(&controller));
    Expect("first failure delay should use base",
           RenderRetry_RecordFailure(&controller, 100, 2000) == 100);
    Expect("second failure should double delay",
           RenderRetry_RecordFailure(&controller, 100, 2000) == 200);
    Expect("third failure should double delay again",
           RenderRetry_RecordFailure(&controller, 100, 2000) == 400);

    for (int i = 0; i < 20; ++i) {
        RenderRetry_RecordFailure(&controller, 100, 2000);
    }
    Expect("persistent failures should clamp at max delay",
           RenderRetry_GetDelay(&controller, 100, 2000) == 2000);

    RenderRetry_MarkTimerArmed(&controller);
    Expect("armed timer state was not recorded",
           RenderRetry_IsTimerArmed(&controller));
    RenderRetry_MarkTimerFired(&controller);
    Expect("fired timer remained armed",
           !RenderRetry_IsTimerArmed(&controller));

    RenderRetry_Reset(&controller);
    Expect("reset did not clear failure state", !RenderRetry_IsActive(&controller));
    Expect("reset did not clear timer state", !RenderRetry_IsTimerArmed(&controller));
    Expect("zero delays should normalize safely",
           RenderRetry_RecordFailure(&controller, 0, 0) == 1);

    if (g_failures != 0) {
        fprintf(stderr, "%d render retry test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
