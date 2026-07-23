#include "window_commands_internal.h"

LRESULT CmdAnimationSpeed(HWND hwnd, AnimationSpeedMetric metric) {
    AnimationSpeedMetric previous = GetAnimationSpeedMetric();
    if (WriteConfigAnimationSpeedMetric(metric) &&
        previous != GetAnimationSpeedMetric()) {
        TrayAnimation_RecomputeTimerDelay();
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}

typedef struct {
    double lastMultiplier;
    BOOL hasPreview;
} FixedAnimationSpeedPreviewState;

static void PreviewFixedAnimationSpeed(
    const wchar_t* text, void* context) {
    FixedAnimationSpeedPreviewState* state = context;
    double multiplier = 0.0;
    if (!state || !TryParseFixedAnimationSpeed(text, &multiplier)) return;
    if (state->hasPreview &&
        fabs(state->lastMultiplier - multiplier) < 0.000001) return;
    SetAnimationSpeedRuntimeState(ANIMATION_SPEED_FIXED, multiplier);
    TrayAnimation_RecomputeTimerDelay();
    state->lastMultiplier = multiplier;
    state->hasPreview = TRUE;
}

LRESULT CmdAnimationFixedSpeed(HWND hwnd) {
    AnimationSpeedMetric originalMetric = GetAnimationSpeedMetric();
    double originalMultiplier = GetAnimationFixedSpeedMultiplier();
    FixedAnimationSpeedPreviewState preview = {0};
    wchar_t input[32] = {0};
    _snwprintf_s(input, _countof(input), _TRUNCATE,
                 L"%.10g", originalMultiplier);
    while (InputBoxWithPreview(
        hwnd, GetLocalizedString(NULL, L"Set Fixed Animation Speed"),
        GetLocalizedString(
            NULL, L"Enter a fixed speed from 0.1 to 30 (example: 2):"),
        input, input, _countof(input), PreviewFixedAnimationSpeed, &preview)) {
        double multiplier = 0.0;
        if (!TryParseFixedAnimationSpeed(input, &multiplier)) {
            DialogMessage_Show(
                hwnd, GetLocalizedString(NULL, L"Invalid input format"),
                GetLocalizedString(
                    NULL, L"Please enter a number from 0.1 to 30 (example: 2)."),
                DIALOG_MESSAGE_WARNING);
            continue;
        }
        if (!WriteConfigAnimationFixedSpeed(multiplier)) {
            SetAnimationSpeedRuntimeState(
                originalMetric, originalMultiplier);
            TrayAnimation_RecomputeTimerDelay();
            DialogMessage_Show(
                hwnd, GetLocalizedString(NULL, L"Error"),
                GetLocalizedString(
                    NULL, L"Failed to save the fixed animation speed."),
                DIALOG_MESSAGE_ERROR);
            return 0;
        }
        TrayAnimation_RecomputeTimerDelay();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    SetAnimationSpeedRuntimeState(originalMetric, originalMultiplier);
    TrayAnimation_RecomputeTimerDelay();
    return 0;
}
