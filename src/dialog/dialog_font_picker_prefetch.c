/**
 * @file dialog_font_picker_prefetch.c
 * @brief Reusable system-font enumeration cache.
 */

#include "dialog_font_picker_internal.h"
#include "dialog/dialog_common.h"

void PrefetchSystemFontDialogResources(void) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER) ||
        DialogFontPickerInternal_IsFontMapCacheReady() ||
        !DialogFontPickerInternal_CleanupCompletedEnumeration()) {
        return;
    }
    DialogFontPickerInternal_ResetFontMap();
    (void)DialogFontPickerInternal_StartEnumeration(NULL);
}

void InvalidateSystemFontDialogCache(void) {
    BOOL dialogOpen = Dialog_IsOpen(DIALOG_INSTANCE_FONT_PICKER);
    InterlockedIncrement(&g_fontEnumGeneration);
    InterlockedExchange(&g_fontMapCacheReady, 0);

    if (!g_fontEnumThread) {
        if (!dialogOpen) {
            DialogFontPickerInternal_ResetFontMap();
        }
        return;
    }

    g_fontEnumRestartAfterCleanup = dialogOpen;
    if (g_fontEnumStopEvent) {
        SetEvent(g_fontEnumStopEvent);
    }
    if (dialogOpen) {
        HWND hdlg = Dialog_GetInstance(DIALOG_INSTANCE_FONT_PICKER);
        if (hdlg) {
            DialogFontPickerInternal_StartPollTimer(hdlg);
        }
    } else {
        DialogFontPickerInternal_ScheduleDeferredCleanup();
    }
}
