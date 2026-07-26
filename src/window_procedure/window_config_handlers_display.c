/**
 * @file window_config_handlers_display.c
 * @brief Reloads display-related configuration.
 */

#include "window_procedure/window_config_handlers_internal.h"
#include "color/color.h"
#include "config.h"
#include "config/config_defaults.h"
#include "drawing/drawing_effect.h"
#include "log.h"
#include "notification.h"
#include "text_effect.h"
#include "timer/timer.h"
#include "window.h"
#include "window_procedure/window_utils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];

LRESULT HandleAppDisplayChanged(HWND hwnd) {
    BOOL changed = FALSE;
    BOOL textColorChanged = FALSE;

    /* Text color */
    char textColorBuf[COLOR_HEX_BUFFER];
    ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_TEXT_COLOR, CLOCK_TEXT_COLOR,
                  textColorBuf, sizeof(textColorBuf));

    char normalizedTextColor[COLOR_HEX_BUFFER];
    WindowConfigInternal_NormalizeTextColor(textColorBuf, normalizedTextColor,
                                   sizeof(normalizedTextColor));
    if (strcmp(normalizedTextColor, CLOCK_TEXT_COLOR) != 0) {
        strncpy_s(CLOCK_TEXT_COLOR, sizeof(CLOCK_TEXT_COLOR),
                  normalizedTextColor, _TRUNCATE);
        changed = TRUE;
        textColorChanged = TRUE;
    }

    /* Font size */
    int newBaseFontSize = WindowConfigInternal_NormalizeBaseFontSize(
        ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_BASE_FONT_SIZE, CLOCK_BASE_FONT_SIZE));
    if (newBaseFontSize != CLOCK_BASE_FONT_SIZE) {
        CLOCK_BASE_FONT_SIZE = newBaseFontSize;
        changed = TRUE;
    }

    char fontConfigValue[MAX_PATH] = {0};
    if (WindowConfigInternal_ReadStringExact(CFG_SECTION_DISPLAY, "FONT_FILE_NAME",
                                 FONTS_PATH_PREFIX DEFAULT_FONT_NAME,
                                 fontConfigValue, sizeof(fontConfigValue)) &&
        WindowConfigInternal_ApplyFont(fontConfigValue)) {
        changed = TRUE;
    }

    /* Window settings (only if not in edit mode) */
    if (!CLOCK_EDIT_MODE) {
        int configPosX = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_X, CLOCK_WINDOW_POS_X);
        int posY = ReadConfigInt(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_POS_Y, CLOCK_WINDOW_POS_Y);
        int posX = configPosX;
        BOOL manualPosition = ReadConfigBool(CFG_SECTION_DISPLAY,
                                             CFG_KEY_WINDOW_POSITION_MANUAL,
                                             FALSE);
        CLOCK_WINDOW_POSITION_MANUAL = manualPosition;

        /* Skip position handling for special/default sentinels during hot-reload.
         * Reset/apply paths resolve them with finalized window dimensions. */
        BOOL skipPositionUpdate = ((!manualPosition &&
                                    (configPosX == -2 || configPosX == -1 ||
                                     posY == DEFAULT_WINDOW_POS_Y)) ||
                                    IsSystemPositionChangeGuardActive());
        if (skipPositionUpdate) {
            posX = CLOCK_WINDOW_POS_X;
            posY = CLOCK_WINDOW_POS_Y;
        }

        char scaleStr[64];
        ReadConfigStr(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_SCALE, "1.62", scaleStr, sizeof(scaleStr));
        float newScale = CLOCK_WINDOW_SCALE;
        BOOL hasValidScale = WindowConfigInternal_ParseScaleFactor(scaleStr, &newScale);

        char pluginScaleStr[64];
        ReadConfigStr(CFG_SECTION_DISPLAY, "PLUGIN_SCALE", "1.0", pluginScaleStr, sizeof(pluginScaleStr));
        float newPluginScale = PLUGIN_FONT_SCALE_FACTOR;
        BOOL hasValidPluginScale = WindowConfigInternal_ParseScaleFactor(pluginScaleStr, &newPluginScale);
        BOOL newTopmost = ReadConfigBool(CFG_SECTION_DISPLAY, CFG_KEY_WINDOW_TOPMOST, CLOCK_WINDOW_TOPMOST);
        int newOpacity = ReadConfigInt(CFG_SECTION_DISPLAY, "WINDOW_OPACITY", CLOCK_WINDOW_OPACITY);

        BOOL posChanged = !skipPositionUpdate && ((posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y));
        BOOL scaleChanged = hasValidScale && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f;

        /* Placement metadata can change independently of the legacy absolute
         * X/Y keys (for example after a taskbar moves to another edge). */
        if (!skipPositionUpdate && manualPosition) {
            RECT currentRect = {0};
            BOOL hasCurrentRect = GetWindowRect(hwnd, &currentRect);
            int placementWidth = hasCurrentRect
                ? currentRect.right - currentRect.left
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH,
                                              CLOCK_WINDOW_SCALE);
            int placementHeight = hasCurrentRect
                ? currentRect.bottom - currentRect.top
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT,
                                              CLOCK_WINDOW_SCALE);
            int resolvedX = posX;
            int resolvedY = posY;
            ResolveConfiguredWindowPosition(placementWidth, placementHeight,
                                            &resolvedX, &resolvedY);
            long long deltaX = llabs((long long)resolvedX -
                                     CLOCK_WINDOW_POS_X);
            long long deltaY = llabs((long long)resolvedY -
                                     CLOCK_WINDOW_POS_Y);
            if (deltaX > 1 || deltaY > 1) {
                posX = resolvedX;
                posY = resolvedY;
                posChanged = TRUE;
            } else {
                posX = CLOCK_WINDOW_POS_X;
                posY = CLOCK_WINDOW_POS_Y;
                posChanged = FALSE;
            }
        }


        if (scaleChanged) {
            CLOCK_WINDOW_SCALE = newScale;
            CLOCK_FONT_SCALE_FACTOR = newScale;
            changed = TRUE;
        }

        if (hasValidPluginScale && fabsf(newPluginScale - PLUGIN_FONT_SCALE_FACTOR) > 0.0001f) {
            PLUGIN_FONT_SCALE_FACTOR = newPluginScale;
            changed = TRUE;
        }

        if (posChanged) {
            RECT currentRect = {0};
            BOOL hasCurrentRect = GetWindowRect(hwnd, &currentRect);
            int width = hasCurrentRect
                ? currentRect.right - currentRect.left
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_WIDTH,
                                              CLOCK_WINDOW_SCALE);
            int height = hasCurrentRect
                ? currentRect.bottom - currentRect.top
                : ScaleWindowDimensionClamped(CLOCK_BASE_WINDOW_HEIGHT,
                                              CLOCK_WINDOW_SCALE);

            ResolveConfiguredWindowPosition(width, height, &posX, &posY);

            LOG_DEBUG("Hot reload applying window position: pos=(%d, %d), scaleChanged=%d",
                      posX, posY, scaleChanged);
            SetWindowPos(hwnd, NULL, posX, posY, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            CLOCK_WINDOW_POS_X = posX;
            CLOCK_WINDOW_POS_Y = posY;
            changed = TRUE;
        }

        if (newTopmost != CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmostFromConfig(hwnd, newTopmost);
            changed = TRUE;
        }

        if (newOpacity != CLOCK_WINDOW_OPACITY) {
            if (newOpacity < MIN_VISIBLE_OPACITY) newOpacity = MIN_VISIBLE_OPACITY;
            if (newOpacity > MAX_OPACITY) newOpacity = MAX_OPACITY;
            CLOCK_WINDOW_OPACITY = newOpacity;
            SetBlurBehind(hwnd, CLOCK_EDIT_MODE);
            changed = TRUE;
        }
    }

    g_AppConfig.display.move_step_small = WindowConfigInternal_ClampInt(
        "MOVE_STEP_SMALL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "MOVE_STEP_SMALL",
                      g_AppConfig.display.move_step_small),
        MIN_MOVE_STEP, MAX_MOVE_STEP);
    g_AppConfig.display.move_step_large = WindowConfigInternal_ClampInt(
        "MOVE_STEP_LARGE",
        ReadConfigInt(CFG_SECTION_DISPLAY, "MOVE_STEP_LARGE",
                      g_AppConfig.display.move_step_large),
        MIN_MOVE_STEP, MAX_MOVE_STEP);
    g_AppConfig.display.opacity_step_normal = WindowConfigInternal_ClampInt(
        "OPACITY_STEP_NORMAL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "OPACITY_STEP_NORMAL",
                      g_AppConfig.display.opacity_step_normal),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.opacity_step_fast = WindowConfigInternal_ClampInt(
        "OPACITY_STEP_FAST",
        ReadConfigInt(CFG_SECTION_DISPLAY, "OPACITY_STEP_FAST",
                      g_AppConfig.display.opacity_step_fast),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.scale_step_normal = WindowConfigInternal_ClampInt(
        "SCALE_STEP_NORMAL",
        ReadConfigInt(CFG_SECTION_DISPLAY, "SCALE_STEP_NORMAL",
                      g_AppConfig.display.scale_step_normal),
        MIN_OPACITY, MAX_OPACITY);
    g_AppConfig.display.scale_step_fast = WindowConfigInternal_ClampInt(
        "SCALE_STEP_FAST",
        ReadConfigInt(CFG_SECTION_DISPLAY, "SCALE_STEP_FAST",
                      g_AppConfig.display.scale_step_fast),
        MIN_OPACITY, MAX_OPACITY);

    char effectBuf[32];
    ReadConfigStr(CFG_SECTION_DISPLAY, "TEXT_EFFECT", "NONE", effectBuf, sizeof(effectBuf));
    TextEffectType previousTextEffect = CLOCK_TEXT_EFFECT;
    TextEffectType newTextEffect = TextEffect_FromConfigString(effectBuf);
    if (newTextEffect != previousTextEffect) {
        CLOCK_TEXT_EFFECT = newTextEffect;
        g_AppConfig.display.text_effect = newTextEffect;
        if (TextEffect_UsesSharedEffectBuffer(previousTextEffect) &&
            !TextEffect_UsesSharedEffectBuffer(newTextEffect)) {
            CleanupDrawingEffects();
        }
        changed = TRUE;
    }

    if (changed) {
        ResetTimerWithInterval(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
    }
    if (textColorChanged) {
        RefreshToastNotificationColors();
    }

    return 0;
}
