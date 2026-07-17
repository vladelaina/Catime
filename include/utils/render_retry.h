/**
 * @file render_retry.h
 * @brief Bounded exponential-backoff state for presentation retries.
 */

#ifndef UTILS_RENDER_RETRY_H
#define UTILS_RENDER_RETRY_H

#include <windows.h>

typedef struct {
    UINT consecutiveFailures;
    BOOL timerArmed;
} RenderRetryController;

void RenderRetry_Reset(RenderRetryController* controller);
UINT RenderRetry_RecordFailure(RenderRetryController* controller,
                               UINT baseDelayMs,
                               UINT maxDelayMs);
UINT RenderRetry_GetDelay(const RenderRetryController* controller,
                          UINT baseDelayMs,
                          UINT maxDelayMs);
BOOL RenderRetry_IsActive(const RenderRetryController* controller);
BOOL RenderRetry_IsTimerArmed(const RenderRetryController* controller);
void RenderRetry_MarkTimerArmed(RenderRetryController* controller);
void RenderRetry_MarkTimerFired(RenderRetryController* controller);

#endif /* UTILS_RENDER_RETRY_H */
