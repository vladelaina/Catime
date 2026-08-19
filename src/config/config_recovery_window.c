#include "config/config_recovery.h"
#include "config/config_defaults.h"
#include "config.h"
#include "log.h"
#include "window.h"
#include "color/gradient.h"
#include "utils/string_safe.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
BOOL ValidateWindowConfig(ConfigSnapshot* snapshot) {
    if (!snapshot) return FALSE;
    BOOL modified = FALSE;
    if (!isfinite(snapshot->windowScale)) {
        LOG_WARNING("Window scale is not finite, setting to %.2f", MIN_SCALE_FACTOR);
        snapshot->windowScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->windowScale < MIN_SCALE_FACTOR) {
        LOG_WARNING("Window scale too small (%.2f), setting to %.2f",
                   snapshot->windowScale, MIN_SCALE_FACTOR);
        snapshot->windowScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->windowScale > MAX_SCALE_FACTOR) {
        LOG_WARNING("Window scale too large (%.2f), setting to %.2f",
                   snapshot->windowScale, MAX_SCALE_FACTOR);
        snapshot->windowScale = MAX_SCALE_FACTOR;
        modified = TRUE;
    }
    if (!isfinite(snapshot->pluginScale)) {
        LOG_WARNING("Plugin scale is not finite, setting to %.2f", MIN_SCALE_FACTOR);
        snapshot->pluginScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->pluginScale < MIN_SCALE_FACTOR) {
        LOG_WARNING("Plugin scale too small (%.2f), setting to %.2f",
                   snapshot->pluginScale, MIN_SCALE_FACTOR);
        snapshot->pluginScale = MIN_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->pluginScale > MAX_SCALE_FACTOR) {
        LOG_WARNING("Plugin scale too large (%.2f), setting to %.2f",
                   snapshot->pluginScale, MAX_SCALE_FACTOR);
        snapshot->pluginScale = MAX_SCALE_FACTOR;
        modified = TRUE;
    }
    if (snapshot->windowOpacity < MIN_VISIBLE_OPACITY) {
        snapshot->windowOpacity = MIN_VISIBLE_OPACITY;
        modified = TRUE;
    }
    if (snapshot->windowOpacity > MAX_OPACITY) {
        snapshot->windowOpacity = MAX_OPACITY;
        modified = TRUE;
    }
    if (snapshot->moveStepSmall < MIN_MOVE_STEP) {
        snapshot->moveStepSmall = MIN_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepSmall > MAX_MOVE_STEP) {
        snapshot->moveStepSmall = MAX_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepLarge < MIN_MOVE_STEP) {
        snapshot->moveStepLarge = MIN_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->moveStepLarge > MAX_MOVE_STEP) {
        snapshot->moveStepLarge = MAX_MOVE_STEP;
        modified = TRUE;
    }
    if (snapshot->opacityStepNormal < MIN_OPACITY) {
        snapshot->opacityStepNormal = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->opacityStepNormal > MAX_OPACITY) {
        snapshot->opacityStepNormal = MAX_OPACITY;
        modified = TRUE;
    }
    if (snapshot->opacityStepFast < MIN_OPACITY) {
        snapshot->opacityStepFast = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->opacityStepFast > MAX_OPACITY) {
        snapshot->opacityStepFast = MAX_OPACITY;
        modified = TRUE;
    }
    if (snapshot->scaleStepNormal < MIN_OPACITY) {
        snapshot->scaleStepNormal = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->scaleStepNormal > MAX_OPACITY) {
        snapshot->scaleStepNormal = MAX_OPACITY;
        modified = TRUE;
    }
    if (snapshot->scaleStepFast < MIN_OPACITY) {
        snapshot->scaleStepFast = MIN_OPACITY;
        modified = TRUE;
    }
    if (snapshot->scaleStepFast > MAX_OPACITY) {
        snapshot->scaleStepFast = MAX_OPACITY;
        modified = TRUE;
    }
    return modified;
}
