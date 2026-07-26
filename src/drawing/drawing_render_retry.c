/**
 * @file drawing_render_retry.c
 * @brief Render retry, plugin marker, and basic value helpers.
 */

#include "drawing_render_internal.h"

BOOL IsValidRenderAnimationWindow(HWND hwnd) {
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

void ResetMainWindowRenderRetry(HWND hwnd) {
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

BOOL ArmMainWindowRenderRetry(HWND hwnd, UINT delayMs) {
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

void RecordMainWindowRenderFailure(HWND hwnd) {
    if (!IsValidRenderAnimationWindow(hwnd)) return;
    if (s_mainRenderRetryHwnd && s_mainRenderRetryHwnd != hwnd) {
        ResetMainWindowRenderRetry(s_mainRenderRetryHwnd);
    }

    UINT delay = RenderRetry_RecordFailure(&s_mainRenderRetry,
                                           MAIN_RENDER_RETRY_BASE_MS,
                                           MAIN_RENDER_RETRY_MAX_MS);
    ArmMainWindowRenderRetry(hwnd, delay);
}

BOOL ShouldLogMainWindowRenderFailure(void) {
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

    RedrawWindow(hwnd, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

    /* A successful paint resets the controller synchronously. If Windows did
     * not dispatch WM_PAINT, keep probing at the current bounded delay. */
    if (RenderRetry_IsActive(&s_mainRenderRetry) &&
        !RenderRetry_IsTimerArmed(&s_mainRenderRetry)) {
        ArmMainWindowRenderRetry(
            hwnd,
            RenderRetry_GetDelay(&s_mainRenderRetry,
                                 MAIN_RENDER_RETRY_BASE_MS,
                                 MAIN_RENDER_RETRY_MAX_MS));
    }
    return TRUE;
}
void AppendWideSpan(wchar_t** dst, size_t* remaining,
                           const wchar_t* text, size_t textLen) {
    if (!dst || !*dst || !remaining || !text || *remaining == 0 || textLen == 0) {
        return;
    }

    if (textLen > *remaining) {
        textLen = *remaining;
    }

    memcpy(*dst, text, textLen * sizeof(wchar_t));
    *dst += textLen;
    *remaining -= textLen;
}

const wchar_t* FindNextPluginTextMarker(const wchar_t* src,
                                               BOOL includeImages,
                                               PluginTextMarkerKind* markerKind) {
    if (markerKind) {
        *markerKind = PLUGIN_TEXT_MARKER_NONE;
    }
    if (!src) {
        return NULL;
    }

    for (const wchar_t* p = src; *p; ++p) {
        if (includeImages && p[0] == L'!' && p[1] == L'[') {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_IMAGE;
            }
            return p;
        }

        if (p[0] == L'<' &&
            wcsncmp(p, CATIME_OPEN_TAG, CATIME_OPEN_TAG_LEN) == 0) {
            if (markerKind) {
                *markerKind = PLUGIN_TEXT_MARKER_CATIME;
            }
            return p;
        }
    }

    return NULL;
}

COLORREF ParseColorString(const char* colorStr, const GradientInfo* gradientInfo) {
    if (!colorStr) {
        return RGB(255, 255, 255);
    }

    size_t colorLen = strlen(colorStr);
    if (colorLen == 0) {
        return RGB(255, 255, 255);
    }

    /* Use gradient start color for fallback GDI drawing paths. */
    if (gradientInfo) {
        return gradientInfo->startColor;
    }

    COLORREF parsed = RGB(255, 255, 255);
    return ColorStringToColorRef(colorStr, &parsed) ? parsed :
           RGB(255, 255, 255);
}

int CalculateRenderFontSize(int baseFontSize, float scaleFactor) {
    double scaled = (double)baseFontSize * (double)scaleFactor;
    if (!isfinite(scaled) || scaled < 1.0) {
        return 1;
    }
    if (scaled > (double)INT_MAX) {
        return INT_MAX;
    }
    return (int)scaled;
}

BOOL HasPotentialMarkdownSyntax(const wchar_t* text) {
    if (!text) return FALSE;

    for (const wchar_t* p = text; *p; ++p) {
        switch (*p) {
            case L'!':
            case L'[':
            case L']':
            case L'(':
            case L')':
            case L'<':
            case L'>':
            case L'*':
            case L'_':
            case L'`':
            case L'#':
                return TRUE;
            default:
                break;
        }
    }

    return FALSE;
}
