/**
 * @file tray_recovery_policy.h
 * @brief Pure policy for deciding when Shell update failures require recovery.
 */

#ifndef TRAY_RECOVERY_POLICY_H
#define TRAY_RECOVERY_POLICY_H

#include <windows.h>

typedef struct {
    UINT consecutiveFailures;
    DWORD lastFailureTick;
} TrayRecoveryPolicyState;

/** Clear accumulated Shell update failures after a successful probe. */
void TrayRecoveryPolicy_RecordSuccess(TrayRecoveryPolicyState* state);

/**
 * Record a failed Shell update.
 *
 * @return TRUE when the failure threshold has been reached and the tray item
 *         should be marked stale and recreated.
 */
BOOL TrayRecoveryPolicy_RecordFailure(
    TrayRecoveryPolicyState* state, DWORD now,
    DWORD failureWindowMs, UINT failureThreshold);

#endif /* TRAY_RECOVERY_POLICY_H */
