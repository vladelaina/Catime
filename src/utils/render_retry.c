/**
 * @file render_retry.c
 * @brief Presentation retry backoff implementation.
 */

#include "utils/render_retry.h"

#include <limits.h>

static UINT NormalizeDelay(UINT value) {
    return value > 0 ? value : 1u;
}

void RenderRetry_Reset(RenderRetryController* controller) {
    if (!controller) return;
    controller->consecutiveFailures = 0;
    controller->timerArmed = FALSE;
}

UINT RenderRetry_GetDelay(const RenderRetryController* controller,
                          UINT baseDelayMs,
                          UINT maxDelayMs) {
    UINT base = NormalizeDelay(baseDelayMs);
    UINT maximum = NormalizeDelay(maxDelayMs);
    if (maximum < base) maximum = base;

    UINT failures = controller ? controller->consecutiveFailures : 0;
    if (failures <= 1) return base;

    UINT delay = base;
    UINT doublings = failures - 1;
    while (doublings-- > 0 && delay < maximum) {
        if (delay > maximum / 2u) {
            delay = maximum;
        } else {
            delay *= 2u;
        }
    }
    return delay > maximum ? maximum : delay;
}

UINT RenderRetry_RecordFailure(RenderRetryController* controller,
                               UINT baseDelayMs,
                               UINT maxDelayMs) {
    if (!controller) {
        return RenderRetry_GetDelay(NULL, baseDelayMs, maxDelayMs);
    }
    if (controller->consecutiveFailures < UINT_MAX) {
        controller->consecutiveFailures++;
    }
    return RenderRetry_GetDelay(controller, baseDelayMs, maxDelayMs);
}

BOOL RenderRetry_IsActive(const RenderRetryController* controller) {
    return controller && controller->consecutiveFailures > 0;
}

BOOL RenderRetry_IsTimerArmed(const RenderRetryController* controller) {
    return controller && controller->timerArmed;
}

void RenderRetry_MarkTimerArmed(RenderRetryController* controller) {
    if (controller) controller->timerArmed = TRUE;
}

void RenderRetry_MarkTimerFired(RenderRetryController* controller) {
    if (controller) controller->timerArmed = FALSE;
}
