#include "tray/tray_animation_timer.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

static void Expect(const char* name, BOOL actual, BOOL expected) {
    if (!!actual != !!expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, !!expected, !!actual);
        g_failures++;
    }
}

int main(void) {
    FrameRateController controller;
    FrameRateController_Init(&controller, 50);
    Expect("partial interval", FrameRateController_ShouldUpdateTray(&controller, 20.0), FALSE);
    Expect("completed interval", FrameRateController_ShouldUpdateTray(&controller, 30.0), TRUE);
    Expect("jitter catch-up", FrameRateController_ShouldUpdateTray(&controller, 120.0), TRUE);
    Expect("jitter remainder", FrameRateController_ShouldUpdateTray(&controller, 29.0), FALSE);
    Expect("jitter remainder completes", FrameRateController_ShouldUpdateTray(&controller, 1.0), TRUE);

    double beforeInvalid = controller.trayAccumulatorMs;
    Expect("NaN rejected", FrameRateController_ShouldUpdateTray(&controller, NAN), FALSE);
    if (controller.trayAccumulatorMs != beforeInvalid) {
        fprintf(stderr, "NaN changed the tray accumulator\n");
        g_failures++;
    }

    Expect("huge elapsed bounded",
           FrameRateController_ShouldUpdateTray(&controller, 1.0e15), TRUE);
    if (!isfinite(controller.trayAccumulatorMs) ||
        controller.trayAccumulatorMs < 0.0 || controller.trayAccumulatorMs >= 50.0) {
        fprintf(stderr, "huge elapsed produced an invalid tray accumulator\n");
        g_failures++;
    }

    controller.targetInterval = 0;
    Expect("zero target rejected",
           FrameRateController_ShouldUpdateTray(&controller, 50.0), FALSE);

    Expect("inactive backoff allows update",
           AnimationUpdateBackoff_ShouldRetry(FALSE, 1000, 1001, 1000), TRUE);
    Expect("backoff blocks early retry",
           AnimationUpdateBackoff_ShouldRetry(TRUE, 1000, 1999, 1000), FALSE);
    Expect("backoff allows due retry",
           AnimationUpdateBackoff_ShouldRetry(TRUE, 1000, 2000, 1000), TRUE);
    Expect("zero last failure allows recovery probe",
           AnimationUpdateBackoff_ShouldRetry(TRUE, 0, 1000, 1000), TRUE);
    Expect("tick wrap preserves backoff",
           AnimationUpdateBackoff_ShouldRetry(TRUE, 0xFFFFFF00u,
                                              0x00000100u, 1000), FALSE);
    Expect("tick wrap eventually allows retry",
           AnimationUpdateBackoff_ShouldRetry(TRUE, 0xFFFFFF00u,
                                              0x00000400u, 1000), TRUE);

    if (g_failures != 0) {
        fprintf(stderr, "%d tray animation timer test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
