/**
 * @file drawing_render_state.c
 * @brief Shared renderer caches and timer state.
 */

#include "drawing_render_internal.h"

BOOL s_renderAnimationTimerActive = FALSE;
UINT s_renderAnimationTimerInterval = 0;
HWND s_renderAnimationTimerHwnd = NULL;
UINT s_renderAnimationTimerResolutionMs = 0;
DWORD s_nextMarkdownImageFileCheckTick = 0;
RenderRetryController s_mainRenderRetry = {0};
HWND s_mainRenderRetryHwnd = NULL;
PaintTextBuffers g_paintTextBuffers = {0};
ScaleGestureTextCache g_scaleGestureTextCache = {0};
MarkdownRenderCache g_markdownRenderCache = {0};
PluginPaintCache g_pluginPaintCache = {0};
TextMeasureCache g_textMeasureCache = {0};
FontPathResolveCache g_fontPathResolveCache = {0};
RenderDibCache g_renderDibCache = {0};
ScaleFrameSnapshot g_scaleFrameSnapshot = {0};
