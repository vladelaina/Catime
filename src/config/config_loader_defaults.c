#include "config_loader_internal.h"
#include "config.h"
#include "config/config_defaults.h"
#include "text_effect.h"
#include <string.h>

void InitializeDefaultSnapshot(ConfigSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(ConfigSnapshot));
    snapshot->baseFontSize = DEFAULT_FONT_SIZE;
    snapshot->windowPosX = DEFAULT_WINDOW_POS_X;
    snapshot->windowPosY = DEFAULT_WINDOW_POS_Y;
    snapshot->windowScale = 1.62f;
    snapshot->pluginScale = 1.0f;
    snapshot->windowTopmost = TRUE;
    snapshot->moveStepSmall = DEFAULT_MOVE_STEP_SMALL;
    snapshot->moveStepLarge = DEFAULT_MOVE_STEP_LARGE;
    snapshot->opacityStepNormal = MIN_OPACITY;
    snapshot->opacityStepFast = 5;
    snapshot->scaleStepNormal = DEFAULT_SCALE_STEP_NORMAL;
    snapshot->scaleStepFast = DEFAULT_SCALE_STEP_FAST;
    snapshot->textEffect = TEXT_EFFECT_NONE;
    snapshot->defaultStartTime = DEFAULT_START_TIME_SECONDS;
    snapshot->notificationTimeoutMs = DEFAULT_NOTIFICATION_TIMEOUT_MS;
    snapshot->notificationMaxOpacity = DEFAULT_NOTIFICATION_MAX_OPACITY;
    snapshot->notificationCornerRadius = DEFAULT_NOTIFICATION_CORNER_RADIUS;
    snapshot->notificationFontSize = DEFAULT_NOTIFICATION_FONT_SIZE;
    snapshot->notificationSoundVolume = DEFAULT_NOTIFICATION_VOLUME;
}
