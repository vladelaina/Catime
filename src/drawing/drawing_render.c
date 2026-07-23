/**
 * @file drawing_render.c
 * @brief GDI rendering pipeline with double-buffering
 */

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <windows.h>
#include <mmsystem.h>
#include "drawing/drawing_render.h"
#include "drawing/drawing_time_format.h"
#include "drawing/drawing_text_stb.h"
#include "drawing/drawing_markdown_stb.h"
#include "drawing.h"
#include "font.h"
#include "color/color.h"
#include "color/gradient.h"
#include "timer/timer.h"
#include "timer/timer_events.h"
#include "config.h"
#include "window_procedure/window_procedure.h"
#include "window/window_core.h"
#include "window/window_desktop_integration.h"
#include "window/window_placement.h"
#include "window/window_visual_effects.h"
#include "drag_scale.h"
#include "menu_preview.h"
#include "preview_display.h"
#include "text_effect.h"
#include "font/font_path_manager.h"
#include "log.h"
#include "plugin/plugin_data.h"
#include "drawing/drawing_image.h"
#include "markdown/markdown_parser.h"
#include "markdown/markdown_image.h"
#include "markdown/markdown_interactive.h"
#include "color/color_parser.h"
#include "utils/string_convert.h"
#include "utils/render_retry.h"
#include "../resource/resource.h"

extern char FONT_INTERNAL_NAME[MAX_PATH];
extern char CLOCK_TEXT_COLOR[COLOR_HEX_BUFFER];
extern int CLOCK_BASE_FONT_SIZE;
extern float CLOCK_FONT_SCALE_FACTOR;
extern float PLUGIN_FONT_SCALE_FACTOR;

#define MAX_RENDER_DIB_DIMENSION 4096
#define MAX_RENDER_DIB_PIXELS (4096u * 4096u)
#define RENDER_DIB_SHRINK_THRESHOLD_MULTIPLIER 4u
#define SCALE_SNAPSHOT_MIN_PIXELS 500000u
#define PLUGIN_IMAGE_STACK_CAPACITY 4
#define CATIME_MAIN_WINDOW_CLASS_NAME L"CatimeWindowClass"
#define FONT_PATH_RESOLVE_FAILURE_RETRY_MS 5000u
#define MARKDOWN_IMAGE_FILE_RECHECK_MS 1000u
#define RENDER_TIMER_RESOLUTION_MIN_MS 1u
#define RENDER_TIMER_RESOLUTION_MAX_MS 10u
#define MAIN_RENDER_RETRY_BASE_MS 100u
#define MAIN_RENDER_RETRY_MAX_MS 2000u

static const wchar_t CATIME_OPEN_TAG[] = L"<catime>";
static const wchar_t CATIME_CLOSE_TAG[] = L"</catime>";

#define CATIME_OPEN_TAG_LEN (_countof(CATIME_OPEN_TAG) - 1)
#define CATIME_CLOSE_TAG_LEN (_countof(CATIME_CLOSE_TAG) - 1)

static BOOL s_renderAnimationTimerActive = FALSE;
static UINT s_renderAnimationTimerInterval = 0;
static HWND s_renderAnimationTimerHwnd = NULL;
static UINT s_renderAnimationTimerResolutionMs = 0;
static DWORD s_nextMarkdownImageFileCheckTick = 0;
static RenderRetryController s_mainRenderRetry = {0};
static HWND s_mainRenderRetryHwnd = NULL;

typedef struct {
    wchar_t timeText[TIME_TEXT_MAX_LEN];
    wchar_t timerTextSnapshot[TIME_TEXT_MAX_LEN];
    wchar_t pluginText[TIME_TEXT_MAX_LEN];
    wchar_t pluginResult[TIME_TEXT_MAX_LEN];
    wchar_t measureText[TIME_TEXT_MAX_LEN];
} PaintTextBuffers;

/* Paint runs on the UI thread; keeping these off the stack avoids large frames
 * during animation/plugin redraw bursts without adding per-frame heap churn.
 */
static PaintTextBuffers g_paintTextBuffers = {0};

typedef struct {
    BOOL valid;
    HWND hwnd;
    DWORD gestureSerial;
    wchar_t text[TIME_TEXT_MAX_LEN];
} ScaleGestureTextCache;

static ScaleGestureTextCache g_scaleGestureTextCache = {0};

static BOOL IsValidRenderAnimationWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return FALSE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return FALSE;
    }

    wchar_t className[64] = {0};
    if (GetClassNameW(hwnd, className, _countof(className)) == 0) {
        return FALSE;
    }

    return wcscmp(className, CATIME_MAIN_WINDOW_CLASS_NAME) == 0;
}

static void ResetMainWindowRenderRetry(HWND hwnd) {
    HWND trackedHwnd = s_mainRenderRetryHwnd;
    if (IsValidRenderAnimationWindow(hwnd)) {
        KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
    }
    if (trackedHwnd != hwnd && IsValidRenderAnimationWindow(trackedHwnd)) {
        KillTimer(trackedHwnd, TIMER_ID_FORCE_REDRAW);
    }
    s_mainRenderRetryHwnd = NULL;
    RenderRetry_Reset(&s_mainRenderRetry);
}

static BOOL ArmMainWindowRenderRetry(HWND hwnd, UINT delayMs) {
    if (!IsValidRenderAnimationWindow(hwnd)) return FALSE;
    if (RenderRetry_IsTimerArmed(&s_mainRenderRetry) &&
        s_mainRenderRetryHwnd == hwnd) {
        return TRUE;
    }

    if (s_mainRenderRetryHwnd && s_mainRenderRetryHwnd != hwnd) {
        ResetMainWindowRenderRetry(s_mainRenderRetryHwnd);
    }

    if (SetTimer(hwnd, TIMER_ID_FORCE_REDRAW, delayMs > 0 ? delayMs : 1u, NULL) == 0) {
        WriteLog(LOG_LEVEL_WARNING,
                 "Failed to schedule main render retry (delay=%u, error=%lu)",
                 delayMs, GetLastError());
        return FALSE;
    }

    s_mainRenderRetryHwnd = hwnd;
    RenderRetry_MarkTimerArmed(&s_mainRenderRetry);
    return TRUE;
}

static void RecordMainWindowRenderFailure(HWND hwnd) {
    if (!IsValidRenderAnimationWindow(hwnd)) return;
    if (s_mainRenderRetryHwnd && s_mainRenderRetryHwnd != hwnd) {
        ResetMainWindowRenderRetry(s_mainRenderRetryHwnd);
    }

    UINT delay = RenderRetry_RecordFailure(&s_mainRenderRetry,
                                           MAIN_RENDER_RETRY_BASE_MS,
                                           MAIN_RENDER_RETRY_MAX_MS);
    ArmMainWindowRenderRetry(hwnd, delay);
}

static BOOL ShouldLogMainWindowRenderFailure(void) {
    UINT failures = s_mainRenderRetry.consecutiveFailures;
    return failures == 0 || (failures & (failures - 1u)) == 0;
}

BOOL HandleDrawingRenderRetryTimer(HWND hwnd) {
    if (!IsValidRenderAnimationWindow(hwnd)) return FALSE;

    KillTimer(hwnd, TIMER_ID_FORCE_REDRAW);
    RenderRetry_MarkTimerFired(&s_mainRenderRetry);
    s_mainRenderRetryHwnd = hwnd;

    if (!RenderRetry_IsActive(&s_mainRenderRetry)) {
        ResetMainWindowRenderRetry(hwnd);
        return TRUE;
    }

    if (!IsWindowVisible(hwnd)) {
        ResetMainWindowRenderRetry(hwnd);
        return TRUE;
    }

    if (CLOCK_IS_DRAGGING) {
        ArmMainWindowRenderRetry(hwnd, MAIN_RENDER_RETRY_BASE_MS);
        return TRUE;
    }

#include "drawing_render_part01.inc"
#include "drawing_render_part02.inc"
#include "drawing_render_part03.inc"
#include "drawing_render_part04.inc"
#include "drawing_render_part05.inc"
#include "drawing_render_part06.inc"
#include "drawing_render_part07.inc"
#include "drawing_render_part08.inc"
