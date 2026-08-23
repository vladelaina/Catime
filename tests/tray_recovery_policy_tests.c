#include "tray/tray_recovery_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    TrayRecoveryPolicyState state = {0};

    assert(!TrayRecoveryPolicy_RecordFailure(NULL, 0, 1000, 3));
    assert(!TrayRecoveryPolicy_RecordFailure(&state, 0, 1000, 0));
    assert(TrayRecoveryPolicy_RecordFailure(&state, 0, 1000, 1));
    TrayRecoveryPolicy_RecordSuccess(&state);

    assert(!TrayRecoveryPolicy_RecordFailure(&state, 1000, 120000, 3));
    assert(!TrayRecoveryPolicy_RecordFailure(&state, 31000, 120000, 3));
    assert(TrayRecoveryPolicy_RecordFailure(&state, 61000, 120000, 3));

    TrayRecoveryPolicy_RecordSuccess(&state);
    assert(state.consecutiveFailures == 0);
    assert(state.lastFailureTick == 0);

    assert(!TrayRecoveryPolicy_RecordFailure(&state, 1000, 10000, 3));
    assert(!TrayRecoveryPolicy_RecordFailure(&state, 12001, 10000, 3));
    assert(state.consecutiveFailures == 1);

    TrayRecoveryPolicy_RecordSuccess(&state);
    assert(!TrayRecoveryPolicy_RecordFailure(&state, 0, 1000, 3));
    assert(!TrayRecoveryPolicy_RecordFailure(&state, 1, 1000, 3));
    assert(state.consecutiveFailures == 2);

    state.consecutiveFailures = 2;
    state.lastFailureTick = 0xFFFFFFF0u;
    assert(TrayRecoveryPolicy_RecordFailure(
        &state, 0x00000020u, 1000, 3));

    puts("tray recovery policy tests passed");
    return 0;
}
