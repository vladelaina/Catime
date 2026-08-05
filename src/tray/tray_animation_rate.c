#include "tray/tray_animation_timer.h"
#include "utils/finite_double.h"
#include <math.h>

#define TRAY_UPDATE_INTERVAL_MS 50

void FrameRateController_Init(FrameRateController* ctrl, UINT targetMs) {
    if (!ctrl) return;
    ctrl->targetInterval = targetMs > 0 ? targetMs : TRAY_UPDATE_INTERVAL_MS;
    ctrl->trayAccumulatorMs = 0.0;
}

BOOL FrameRateController_ShouldUpdateTray(FrameRateController* ctrl,
                                          double elapsedMs) {
    if (!ctrl || ctrl->targetInterval == 0 ||
        !DoubleIsFiniteStrict(elapsedMs) || elapsedMs <= 0.0) return FALSE;
    ctrl->trayAccumulatorMs += elapsedMs;
    if (!DoubleIsFiniteStrict(ctrl->trayAccumulatorMs) ||
        ctrl->trayAccumulatorMs < 0.0) {
        ctrl->trayAccumulatorMs = 0.0;
        return FALSE;
    }
    if (ctrl->trayAccumulatorMs < (double)ctrl->targetInterval) return FALSE;
    ctrl->trayAccumulatorMs = fmod(ctrl->trayAccumulatorMs,
                                   (double)ctrl->targetInterval);
    return TRUE;
}

BOOL AnimationUpdateBackoff_ShouldRetry(BOOL backoffActive, DWORD lastFailureTick,
                                        DWORD now, UINT backoffMs) {
    if (!backoffActive || lastFailureTick == 0 || backoffMs == 0) return TRUE;
    return (DWORD)(now - lastFailureTick) >= backoffMs;
}
