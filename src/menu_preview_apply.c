#include <windows.h>
#include "menu_preview_internal.h"
#include "color/color_state.h"
#include "config.h"
#include "drawing/drawing_effect.h"
#include "font.h"
#include "text_effect.h"
#include "tray/tray.h"
#include "window_procedure/window_commands.h"

extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern void ResetTimerWithInterval(HWND hwnd);

static BOOL ApplyTextEffectPreview(void) {
    TextEffectType effect = (TextEffectType)g_previewState.data.effect;
    if (effect != TEXT_EFFECT_NONE && !TextEffect_IsSelectable(effect)) {
        return FALSE;
    }
    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, sizeof(configPath));
    if (!WriteIniString(INI_SECTION_DISPLAY, "TEXT_EFFECT",
                        TextEffect_ToConfigString(effect), configPath)) {
        return FALSE;
    }
    TextEffectType previous = CLOCK_TEXT_EFFECT;
    CLOCK_TEXT_EFFECT = effect;
    g_AppConfig.display.text_effect = effect;
    if (TextEffect_UsesSharedEffectBuffer(previous) &&
        !TextEffect_UsesSharedEffectBuffer(effect)) {
        CleanupDrawingEffects();
    }
    return TRUE;
}

static BOOL ApplyTaskbarMonitorPreview(void) {
    BOOL cpuMemoryEnabled =
        g_previewState.data.taskbarMonitor.originalCpuMemoryEnabled;
    BOOL networkEnabled =
        g_previewState.data.taskbarMonitor.originalNetworkEnabled;
    TaskbarMonitorOption option = g_previewState.data.taskbarMonitor.option;
    BOOL enabled;
    if (option == TASKBAR_MONITOR_OPTION_CPU_MEMORY) {
        cpuMemoryEnabled = !cpuMemoryEnabled;
        enabled = cpuMemoryEnabled;
    } else if (g_previewState.data.taskbarMonitor.option ==
               TASKBAR_MONITOR_OPTION_NETWORK) {
        networkEnabled = !networkEnabled;
        enabled = networkEnabled;
    } else {
        return FALSE;
    }
    if (!TaskbarMonitor_SetOptionEnabled(option, enabled)) {
        return FALSE;
    }
    RefreshTrayBackgroundWorkState();
    return TRUE;
}

BOOL ApplyPreview(HWND hwnd) {
    if (!IsPreviewActive()) return FALSE;
    PreviewType appliedType = g_previewState.type;
    switch (appliedType) {
        case PREVIEW_TYPE_COLOR:
            if (!WriteConfigColor(g_previewState.data.colorHex)) return FALSE;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_FONT:
            if (!WriteConfigFont(g_previewState.data.font.fontName, FALSE) ||
                !FlushConfigToDisk()) {
                CancelPreview(hwnd);
                return FALSE;
            }
            strncpy_s(FONT_FILE_NAME, sizeof(FONT_FILE_NAME),
                      g_previewState.data.font.fontName, _TRUNCATE);
            strncpy_s(FONT_RUNTIME_FILE_NAME, sizeof(FONT_RUNTIME_FILE_NAME),
                      g_previewState.data.font.fontName, _TRUNCATE);
            strncpy_s(FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME),
                      g_previewState.data.font.internalName, _TRUNCATE);
            break;
        case PREVIEW_TYPE_TIME_FORMAT:
            if (!WriteConfigTimeFormat(g_previewState.data.timeFormat)) {
                return FALSE;
            }
            break;
        case PREVIEW_TYPE_MILLISECONDS:
            if (!WriteConfigShowMilliseconds(
                    g_previewState.data.showMilliseconds)) return FALSE;
            break;
        case PREVIEW_TYPE_SECONDS:
            if (!WriteConfigKeyValue(
                    "CLOCK_SHOW_SECONDS",
                    g_previewState.data.showSeconds ? "TRUE" : "FALSE")) {
                return FALSE;
            }
            CLOCK_SHOW_SECONDS = g_previewState.data.showSeconds;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_24HOUR:
            if (!WriteConfigKeyValue(
                    "CLOCK_USE_24HOUR",
                    g_previewState.data.use24Hour ? "TRUE" : "FALSE")) {
                return FALSE;
            }
            CLOCK_USE_24HOUR = g_previewState.data.use24Hour;
            break;
        case PREVIEW_TYPE_EFFECT:
            if (!ApplyTextEffectPreview()) return FALSE;
            break;
        case PREVIEW_TYPE_ANIMATION:
            break;
        case PREVIEW_TYPE_TASKBAR_MONITOR:
            if (!ApplyTaskbarMonitorPreview()) return FALSE;
            break;
        default:
            return FALSE;
    }
    g_previewState.type = PREVIEW_TYPE_NONE;
    if (appliedType == PREVIEW_TYPE_FONT) {
        RefreshCustomTextDisplayDialogFont();
    }
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

void MarkAnimationPreviewApplied(HWND hwnd) {
    if (g_previewState.type != PREVIEW_TYPE_ANIMATION) return;
    g_previewState.type = PREVIEW_TYPE_NONE;
    g_previewState.data.animationPath[0] = '\0';
    if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
}
