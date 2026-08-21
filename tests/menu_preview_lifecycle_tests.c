#include "menu_preview.h"
#include "window_procedure/window_preview_policy.h"
#include "config.h"
#include "font.h"
#include "log.h"
#include "taskbar_monitor.h"
#include "text_effect.h"
#include "../resource/resource.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

AppConfig g_AppConfig = {0};
char FONT_FILE_NAME[MAX_PATH] = "configured.ttf";
char FONT_RUNTIME_FILE_NAME[MAX_PATH] = "configured.ttf";
char FONT_INTERNAL_NAME[MAX_PATH] = "Configured Font";
bool CLOCK_SHOW_SECONDS = false;
bool CLOCK_USE_24HOUR = false;
TextEffectType CLOCK_TEXT_EFFECT = TEXT_EFFECT_NONE;
BOOL g_isPreviewActive = FALSE;
static BOOL g_fontLoadSucceeds = TRUE;
static BOOL g_hasPendingAnimationCommit = FALSE;

void CancelAnimationPreview(void) {
    g_isPreviewActive = FALSE;
}

void CleanupDrawingEffects(void) {}

const char* ExtractRelativePath(const char* path) {
    return path;
}

BOOL IsFontsFolderPath(const char* path) {
    (void)path;
    return FALSE;
}

BOOL LoadFontByNameAndGetRealName(HINSTANCE instance, const char* fontName,
                                  char* realName, size_t realNameSize) {
    (void)instance;
    if (!fontName || !fontName[0] || !realName || realNameSize == 0) {
        return FALSE;
    }
    if (!g_fontLoadSucceeds) return FALSE;
    strncpy_s(realName, realNameSize, "Preview Font", _TRUNCATE);
    return TRUE;
}

void RefreshCustomTextDisplayDialogFont(void) {}
void RefreshToastNotificationColors(void) {}
void RefreshTrayBackgroundWorkState(void) {}
void ResetTimerWithInterval(HWND hwnd) { (void)hwnd; }

BOOL StartAnimationPreview(const char* name) {
    g_isPreviewActive = name && name[0];
    return g_isPreviewActive;
}

BOOL TrayAnimation_HasPendingCommit(void) {
    return g_hasPendingAnimationCommit;
}

void TaskbarMonitor_ApplyConfig(BOOL enabled, BOOL cpuMemoryEnabled,
                                BOOL networkEnabled) {
    (void)enabled;
    (void)cpuMemoryEnabled;
    (void)networkEnabled;
}

BOOL TaskbarMonitor_IsOptionEnabled(TaskbarMonitorOption option) {
    (void)option;
    return FALSE;
}

BOOL TaskbarMonitor_SetOptionEnabled(TaskbarMonitorOption option,
                                     BOOL enabled) {
    (void)option;
    (void)enabled;
    return TRUE;
}

BOOL TextEffect_IsSelectable(TextEffectType type) {
    return type >= TEXT_EFFECT_NONE && type < TEXT_EFFECT_COUNT;
}

const char* TextEffect_ToConfigString(TextEffectType type) {
    (void)type;
    return "NONE";
}

BOOL TextEffect_UsesSharedEffectBuffer(TextEffectType type) {
    (void)type;
    return FALSE;
}

BOOL WriteConfigColor(const char* color) {
    return color && color[0];
}

BOOL WriteConfigFont(const char* fontName, BOOL shouldReload) {
    (void)shouldReload;
    return fontName && fontName[0];
}

BOOL WriteConfigKeyValue(const char* key, const char* value) {
    return key && key[0] && value;
}

BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds) {
    (void)showMilliseconds;
    return TRUE;
}

BOOL WriteConfigTimeFormat(TimeFormatType format) {
    (void)format;
    return TRUE;
}

BOOL WriteIniString(const char* section, const char* key, const char* value,
                    const char* filePath) {
    return section && key && value && filePath;
}

BOOL FlushConfigToDisk(void) {
    return TRUE;
}

void GetConfigPath(char* path, size_t size) {
    if (path && size > 0) strncpy_s(path, size, "test.ini", _TRUNCATE);
}

void WriteLog(LogLevel level, const char* format, ...) {
    (void)level;
    (void)format;
}

static void ResetPreview(void) {
    CancelPreview(NULL);
    assert(!IsPreviewActive());
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_NONE);
}

static void TestCommandPolicy(void) {
    assert(WindowPreview_ShouldKeepForCommand(
        CLOCK_IDC_EDIT_MODE, PREVIEW_SOURCE_FONT_PICKER));
    assert(!WindowPreview_ShouldKeepForCommand(
        CLOCK_IDC_EDIT_MODE, PREVIEW_SOURCE_TRANSIENT));
    assert(!WindowPreview_ShouldKeepForCommand(
        CLOCK_IDM_TOPMOST, PREVIEW_SOURCE_FONT_PICKER));
}

static void TestPreviewSourceTransitions(void) {
    ResetPreview();
    assert(StartPreviewWithSource(PREVIEW_TYPE_FONT, "picker.ttf",
                                  PREVIEW_SOURCE_FONT_PICKER, NULL));
    assert(GetActivePreviewType() == PREVIEW_TYPE_FONT);
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_FONT_PICKER);

    assert(StartPreview(PREVIEW_TYPE_FONT, "menu.ttf", NULL));
    assert(GetActivePreviewType() == PREVIEW_TYPE_FONT);
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_TRANSIENT);
    ResetPreview();
}

static void TestInvalidSourceDoesNotReplacePreview(void) {
    TimeFormatType format = TIME_FORMAT_ZERO_PADDED;
    assert(StartPreview(PREVIEW_TYPE_TIME_FORMAT, &format, NULL));
    assert(!StartPreviewWithSource(PREVIEW_TYPE_TIME_FORMAT, &format,
                                   PREVIEW_SOURCE_NONE, NULL));
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_TRANSIENT);
    ResetPreview();
}

static void TestFailedPreviewDoesNotTransferSource(void) {
    assert(StartPreviewWithSource(PREVIEW_TYPE_FONT, "picker.ttf",
                                  PREVIEW_SOURCE_FONT_PICKER, NULL));
    g_fontLoadSucceeds = FALSE;
    assert(!StartPreview(PREVIEW_TYPE_FONT, "missing.ttf", NULL));
    g_fontLoadSucceeds = TRUE;
    assert(GetActivePreviewType() == PREVIEW_TYPE_FONT);
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_FONT_PICKER);
    ResetPreview();
}

static void TestApplyClearsPreviewSource(void) {
    assert(StartPreviewWithSource(PREVIEW_TYPE_FONT, "applied.ttf",
                                  PREVIEW_SOURCE_FONT_PICKER, NULL));
    assert(ApplyPreview(NULL));
    assert(!IsPreviewActive());
    assert(GetActivePreviewSource() == PREVIEW_SOURCE_NONE);
    assert(strcmp(FONT_RUNTIME_FILE_NAME, "applied.ttf") == 0);
}

static void TestPendingAnimationCommitSurvivesCancel(void) {
    ResetPreview();
    assert(StartPreview(PREVIEW_TYPE_ANIMATION, "walking.gif", NULL));
    assert(g_isPreviewActive);

    g_hasPendingAnimationCommit = TRUE;
    CancelPreview(NULL);
    assert(!IsPreviewActive());
    assert(g_isPreviewActive);

    g_hasPendingAnimationCommit = FALSE;
    CancelPreview(NULL);
    assert(!g_isPreviewActive);
}

int main(void) {
    TestCommandPolicy();
    TestPreviewSourceTransitions();
    TestInvalidSourceDoesNotReplacePreview();
    TestFailedPreviewDoesNotTransferSource();
    TestApplyClearsPreviewSource();
    TestPendingAnimationCommitSurvivesCancel();
    puts("menu preview lifecycle tests passed");
    return 0;
}
