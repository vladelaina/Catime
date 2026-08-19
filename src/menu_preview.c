#include <windows.h>
#include <string.h>
#include "menu_preview.h"
#include "menu_preview_internal.h"
#include "config.h"
#include "font.h"
#include "color/color.h"
#include "color/color_parser.h"
#include "drawing/drawing_effect.h"
#include "tray/tray_animation_core.h"
#include "window_procedure/window_commands.h"
#include "notification.h"
#include "log.h"
extern char FONT_FILE_NAME[MAX_PATH];
extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern BOOL IS_PREVIEWING;
extern char PREVIEW_FONT_NAME[MAX_PATH];
extern char PREVIEW_INTERNAL_NAME[MAX_PATH];
extern void ResetTimerWithInterval(HWND hwnd);
PreviewState g_previewState = {0};
static BOOL ShouldCleanupPreviewEffectBuffers(EffectType previousPreviewEffect,
                                              EffectType nextPreviewEffect) {
    return TextEffect_UsesSharedEffectBuffer(previousPreviewEffect) &&
           !TextEffect_UsesSharedEffectBuffer(nextPreviewEffect) &&
           !TextEffect_UsesSharedEffectBuffer(CLOCK_TEXT_EFFECT);
}
BOOL IsPreviewActive(void) {
    return g_previewState.type != PREVIEW_TYPE_NONE;
}
PreviewType GetActivePreviewType(void) {
    return g_previewState.type;
}
PreviewSource GetActivePreviewSource(void) {
    return IsPreviewActive() ? g_previewState.source : PREVIEW_SOURCE_NONE;
}
static BOOL LoadPreviewFontName(const char* fontName, char* internalName, size_t internalNameSize) {
    if (!fontName || !fontName[0] || !internalName || internalNameSize == 0) {
        return FALSE;
    }
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!LoadFontByNameAndGetRealName(hInstance, fontName, internalName, internalNameSize)) {
        LOG_WARNING("Preview: failed to load font preview: %s", fontName);
        return FALSE;
    }
    return TRUE;
}
static BOOL RestoreConfiguredFontAfterPreview(void) {
    if (FONT_RUNTIME_FILE_NAME[0] == '\0') {
        return FALSE;
    }
    const char* loadName = FONT_RUNTIME_FILE_NAME;
    if (IsFontsFolderPath(FONT_RUNTIME_FILE_NAME)) {
        const char* relativePath = ExtractRelativePath(FONT_RUNTIME_FILE_NAME);
        if (!relativePath || relativePath[0] == '\0') {
            return FALSE;
        }
        loadName = relativePath;
    }
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!LoadFontByNameAndGetRealName(hInstance, loadName,
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_WARNING("Preview: failed to restore font after preview: %s",
                    FONT_RUNTIME_FILE_NAME);
        return FALSE;
    }
    return TRUE;
}
static BOOL IsValidTaskbarMonitorOption(TaskbarMonitorOption option) {
    return option == TASKBAR_MONITOR_OPTION_CPU_MEMORY ||
           option == TASKBAR_MONITOR_OPTION_NETWORK;
}

static void ApplyTaskbarMonitorPreview(void) {
    BOOL cpuMemoryEnabled =
        g_previewState.data.taskbarMonitor.originalCpuMemoryEnabled;
    BOOL networkEnabled =
        g_previewState.data.taskbarMonitor.originalNetworkEnabled;
    if (g_previewState.data.taskbarMonitor.option ==
        TASKBAR_MONITOR_OPTION_CPU_MEMORY) {
        cpuMemoryEnabled = !cpuMemoryEnabled;
    } else if (g_previewState.data.taskbarMonitor.option ==
               TASKBAR_MONITOR_OPTION_NETWORK) {
        networkEnabled = !networkEnabled;
    }
    TaskbarMonitor_ApplyConfig(TRUE, cpuMemoryEnabled, networkEnabled);
    RefreshTrayBackgroundWorkState();
}

static BOOL StartPreviewInternal(PreviewType type, const void* data, HWND hwnd) {
    if (type <= PREVIEW_TYPE_NONE || type > PREVIEW_TYPE_TASKBAR_MONITOR ||
        !data) {
        return FALSE;
    }
    if (type == PREVIEW_TYPE_TASKBAR_MONITOR) {
        TaskbarMonitorOption option = *(const TaskbarMonitorOption*)data;
        if (!IsValidTaskbarMonitorOption(option)) return FALSE;
        if (g_previewState.type == PREVIEW_TYPE_TASKBAR_MONITOR) {
            if (g_previewState.data.taskbarMonitor.option == option) {
                return TRUE;
            }
            g_previewState.data.taskbarMonitor.option = option;
            ApplyTaskbarMonitorPreview();
            return TRUE;
        }
        if (IsPreviewActive()) CancelPreview(hwnd);
        g_previewState.type = PREVIEW_TYPE_TASKBAR_MONITOR;
        g_previewState.data.taskbarMonitor.option = option;
        g_previewState.data.taskbarMonitor.originalCpuMemoryEnabled =
            TaskbarMonitor_IsOptionEnabled(
                TASKBAR_MONITOR_OPTION_CPU_MEMORY);
        g_previewState.data.taskbarMonitor.originalNetworkEnabled =
            TaskbarMonitor_IsOptionEnabled(TASKBAR_MONITOR_OPTION_NETWORK);
        ApplyTaskbarMonitorPreview();
        return TRUE;
    }
    if ((type == PREVIEW_TYPE_COLOR || type == PREVIEW_TYPE_FONT ||
         type == PREVIEW_TYPE_ANIMATION) && !*(const char*)data) return FALSE;
    if (type == PREVIEW_TYPE_COLOR &&
        g_previewState.type == PREVIEW_TYPE_COLOR) {
        const char* colorHex = (const char*)data;
        if (!colorHex[0] ||
            _stricmp(g_previewState.data.colorHex, colorHex) == 0) {
            return colorHex[0] != '\0';
        }
        strncpy_s(g_previewState.data.colorHex,
                  sizeof(g_previewState.data.colorHex), colorHex, _TRUNCATE);
        if (hwnd) ResetTimerWithInterval(hwnd);
        RefreshToastNotificationColors();
        if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
        return TRUE;
    }
    if (type == PREVIEW_TYPE_ANIMATION &&
        g_previewState.type == PREVIEW_TYPE_ANIMATION) {
        const char* animPath = (const char*)data;
        if (!animPath || !animPath[0] ||
            _stricmp(g_previewState.data.animationPath, animPath) == 0) {
            return animPath && animPath[0];
        }
        if (!StartAnimationPreview(animPath)) {
            return FALSE;
        }
        strncpy_s(g_previewState.data.animationPath, MAX_PATH,
                  animPath, _TRUNCATE);
        return TRUE;
    }
    if (type == PREVIEW_TYPE_EFFECT && g_previewState.type == PREVIEW_TYPE_EFFECT) {
        if (!data) {
            return FALSE;
        }
        EffectType effect = *(const EffectType*)data;
        if (g_previewState.data.effect == effect) {
            return TRUE;
        }
        EffectType previousEffect = g_previewState.data.effect;
        g_previewState.data.effect = effect;
        if (ShouldCleanupPreviewEffectBuffers(previousEffect, effect)) {
            CleanupDrawingEffects();
        }
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }
    if (type == PREVIEW_TYPE_FONT && g_previewState.type == PREVIEW_TYPE_FONT) {
        const char* fontName = (const char*)data;
        if (!fontName || !fontName[0]) {
            return FALSE;
        }
        if (_stricmp(g_previewState.data.font.fontName, fontName) == 0) {
            return TRUE;
        }
        char internalName[MAX_PATH] = {0};
        if (!LoadPreviewFontName(fontName, internalName, sizeof(internalName))) {
            return FALSE;
        }
        strncpy_s(g_previewState.data.font.fontName, MAX_PATH, fontName, _TRUNCATE);
        strncpy_s(g_previewState.data.font.internalName, MAX_PATH, internalName, _TRUNCATE);
        RefreshCustomTextDisplayDialogFont();
        if (hwnd) InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }
    if (IsPreviewActive()) CancelPreview(hwnd);
    g_previewState.type = type;
    switch (type) {
        case PREVIEW_TYPE_COLOR: {
            const char* colorHex = (const char*)data;
            strncpy_s(g_previewState.data.colorHex, sizeof(g_previewState.data.colorHex),
                     colorHex, _TRUNCATE);
            if (hwnd) ResetTimerWithInterval(hwnd);
            RefreshToastNotificationColors();
            break;
        }
        case PREVIEW_TYPE_FONT: {
            const char* fontName = (const char*)data;
            char internalName[MAX_PATH] = {0};
            if (!LoadPreviewFontName(fontName, internalName, sizeof(internalName))) {
                g_previewState.type = PREVIEW_TYPE_NONE;
                return FALSE;
            }
            strncpy_s(g_previewState.data.font.fontName, MAX_PATH, fontName, _TRUNCATE);
            strncpy_s(g_previewState.data.font.internalName, MAX_PATH, internalName, _TRUNCATE);
            RefreshCustomTextDisplayDialogFont();
            break;
        }
        case PREVIEW_TYPE_TIME_FORMAT:
            g_previewState.data.timeFormat = *(TimeFormatType*)data;
            break;
        case PREVIEW_TYPE_MILLISECONDS:
            g_previewState.data.showMilliseconds = *(BOOL*)data;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_SECONDS:
            g_previewState.data.showSeconds = *(BOOL*)data;
            if (hwnd) ResetTimerWithInterval(hwnd);
            break;
        case PREVIEW_TYPE_24HOUR:
            g_previewState.data.use24Hour = *(BOOL*)data;
            break;
        case PREVIEW_TYPE_ANIMATION: {
            const char* animPath = (const char*)data;
            if (!StartAnimationPreview(animPath)) {
                g_previewState.type = PREVIEW_TYPE_NONE;
                return FALSE;
            }
            strncpy_s(g_previewState.data.animationPath, MAX_PATH, animPath, _TRUNCATE);
            break;
        }
        case PREVIEW_TYPE_EFFECT:
            g_previewState.data.effect = *(const EffectType*)data;
            if (TextEffect_UsesSharedEffectBuffer(CLOCK_TEXT_EFFECT) &&
                !TextEffect_UsesSharedEffectBuffer(
                    g_previewState.data.effect)) {
                CleanupDrawingEffects();
            }
            break;
        default:
            g_previewState.type = PREVIEW_TYPE_NONE;
            return FALSE;
    }
    if (hwnd && type != PREVIEW_TYPE_ANIMATION) {
        InvalidateRect(hwnd, NULL, type == PREVIEW_TYPE_COLOR ? FALSE : TRUE);
    }
    return TRUE;
}
BOOL StartPreviewWithSource(PreviewType type, const void* data,
                            PreviewSource source, HWND hwnd) {
    if (source <= PREVIEW_SOURCE_NONE ||
        source > PREVIEW_SOURCE_FONT_PICKER) return FALSE;
    BOOL started = StartPreviewInternal(type, data, hwnd);
    if (started) g_previewState.source = source;
    return started;
}
BOOL StartPreview(PreviewType type, const void* data, HWND hwnd) {
    return StartPreviewWithSource(
        type, data, PREVIEW_SOURCE_TRANSIENT, hwnd);
}
void CancelPreview(HWND hwnd) {
    if ((g_isPreviewActive || g_previewState.type == PREVIEW_TYPE_ANIMATION) &&
        !TrayAnimation_HasPendingCommit()) {
        CancelAnimationPreview();
    }
    if (!IsPreviewActive()) return;
    PreviewType canceledType = g_previewState.type;
    BOOL needsTaskbarMonitorRestore =
        canceledType == PREVIEW_TYPE_TASKBAR_MONITOR;
    BOOL needsRedraw = (canceledType != PREVIEW_TYPE_ANIMATION &&
                        canceledType != PREVIEW_TYPE_NONE &&
                        !needsTaskbarMonitorRestore);
    BOOL needsTimerReset = (canceledType == PREVIEW_TYPE_MILLISECONDS ||
                            canceledType == PREVIEW_TYPE_SECONDS ||
                            canceledType == PREVIEW_TYPE_COLOR);
    BOOL needsColorRefresh = (canceledType == PREVIEW_TYPE_COLOR);
    BOOL needsFontReload = (canceledType == PREVIEW_TYPE_FONT);
    BOOL needsEffectCleanup = (canceledType == PREVIEW_TYPE_EFFECT &&
                               ShouldCleanupPreviewEffectBuffers(
                                   g_previewState.data.effect,
                                   (EffectType)CLOCK_TEXT_EFFECT));
    if (needsTaskbarMonitorRestore) {
        TaskbarMonitor_ApplyConfig(
            TRUE,
            g_previewState.data.taskbarMonitor.originalCpuMemoryEnabled,
            g_previewState.data.taskbarMonitor.originalNetworkEnabled);
        RefreshTrayBackgroundWorkState();
    }
    if (needsFontReload) {
        RestoreConfiguredFontAfterPreview();
    }
    g_previewState.type = PREVIEW_TYPE_NONE;
    g_previewState.source = PREVIEW_SOURCE_NONE;
    if (needsFontReload) {
        RefreshCustomTextDisplayDialogFont();
    }
    if (needsColorRefresh) {
        RefreshToastNotificationColors();
    }
    if (needsEffectCleanup) {
        CleanupDrawingEffects();
    }
    if (needsTimerReset && hwnd) ResetTimerWithInterval(hwnd);
    if (needsRedraw && hwnd) InvalidateRect(hwnd, NULL, TRUE);
}
