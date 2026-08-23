/**
 * @file tray_recovery_policy.c
 * @brief Pure policy for deciding when Shell update failures require recovery.
 */

#include "tray/tray_recovery_policy.h"

#include <limits.h>

void TrayRecoveryPolicy_RecordSuccess(TrayRecoveryPolicyState* state) {
    if (!state) return;
    state->consecutiveFailures = 0;
    state->lastFailureTick = 0;
}

BOOL TrayRecoveryPolicy_RecordFailure(
    TrayRecoveryPolicyState* state, DWORD now,
    DWORD failureWindowMs, UINT failureThreshold) {
    if (!state || failureThreshold == 0) return FALSE;

    if (state->consecutiveFailures == 0 ||
        (DWORD)(now - state->lastFailureTick) > failureWindowMs) {
        state->consecutiveFailures = 0;
    }
    state->lastFailureTick = now;
    if (state->consecutiveFailures < UINT_MAX) {
        state->consecutiveFailures++;
    }
    return state->consecutiveFailures >= failureThreshold;
}
