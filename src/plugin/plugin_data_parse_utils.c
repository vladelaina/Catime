/**
 * @file plugin_data_parse_utils.c
 * @brief Numeric, FPS, polling, and UTF-8 parsing helpers.
 */

#include "plugin_data_internal.h"

BOOL ParseNonNegativeIntLimitedA(const char* start, const char* end, int* outValue) {
    int value = 0;
    if (!start || !end || !outValue || start >= end) return FALSE;

    while (start < end) {
        int digit;
        if (*start < '0' || *start > '9') return FALSE;
        digit = *start - '0';
        if (value > (INT_MAX - digit) / 10) return FALSE;
        value = value * 10 + digit;
        start++;
    }

    *outValue = value;
    return TRUE;
}

BOOL ParseNonNegativeIntLimitedW(const wchar_t* start, const wchar_t* end, int* outValue) {
    int value = 0;
    if (!start || !end || !outValue || start >= end) return FALSE;

    while (start < end) {
        int digit;
        if (*start < L'0' || *start > L'9') return FALSE;
        digit = (int)(*start - L'0');
        if (value > (INT_MAX - digit) / 10) return FALSE;
        value = value * 10 + digit;
        start++;
    }

    *outValue = value;
    return TRUE;
}

/* Cache for change detection */

DWORD GetPollIntervalMs(void) {
    LONG interval = InterlockedCompareExchange(&g_pollIntervalMs, 0, 0);
    return interval > 0 ? (DWORD)interval : DEFAULT_POLL_INTERVAL_MS;
}

void SetPollIntervalMs(DWORD intervalMs) {
    if (intervalMs == 0) intervalMs = DEFAULT_POLL_INTERVAL_MS;
    if (intervalMs > (DWORD)LONG_MAX) intervalMs = (DWORD)LONG_MAX;
    InterlockedExchange(&g_pollIntervalMs, (LONG)intervalMs);
}

void ApplyContentPollInterval(BOOL hasFpsTag, DWORD parsedPollInterval) {
    DWORD intervalMs = hasFpsTag ? parsedPollInterval : DEFAULT_POLL_INTERVAL_MS;
    if (GetPollIntervalMs() != intervalMs) {
        SetPollIntervalMs(intervalMs);
    }
}

BOOL TryParseFpsPollInterval(const char* content, DWORD* intervalOut, int* fpsOut) {
    if (!content || !intervalOut) return FALSE;

    /* Look for <fps:N> pattern */
    const char* fpsStart = strstr(content, "<fps:");
    if (!fpsStart) return FALSE;

    const char* numStart = fpsStart + 5;  /* Skip "<fps:" */
    const char* numEnd = numStart;

    /* Parse number */
    while (*numEnd >= '0' && *numEnd <= '9') {
        numEnd++;
    }

    /* Check for closing > */
    if (*numEnd != '>' || numEnd == numStart) {
        return FALSE;  /* Invalid format */
    }

    int fps = 0;
    if (!ParseNonNegativeIntLimitedA(numStart, numEnd, &fps) || fps <= 0) {
        return FALSE;
    }

    /* Convert fps to poll interval: interval = 1000 / fps */
    DWORD interval = 1000 / fps;

    /* Clamp to valid range */
    if (interval < MIN_POLL_INTERVAL_MS) interval = MIN_POLL_INTERVAL_MS;
    if (interval > MAX_POLL_INTERVAL_MS) interval = MAX_POLL_INTERVAL_MS;

    *intervalOut = interval;
    if (fpsOut) {
        *fpsOut = fps;
    }
    return TRUE;
}

size_t ClampUtf8DisplayInputLength(const char* content, size_t contentLen) {
    if (!content || contentLen <= PLUGIN_DISPLAY_MAX_INPUT_BYTES) {
        return contentLen;
    }

    size_t len = PLUGIN_DISPLAY_MAX_INPUT_BYTES;
    size_t seqStart = len;
    while (seqStart > 0 && (((unsigned char)content[seqStart - 1] & 0xC0u) == 0x80u)) {
        seqStart--;
    }

    if (seqStart == 0) {
        return 0;
    }

    if (seqStart == len) {
        unsigned char last = (unsigned char)content[len - 1];
        if ((last & 0x80u) == 0 || (last & 0xC0u) == 0xC0u) {
            return len - (((last & 0xC0u) == 0xC0u) ? 1u : 0u);
        }
        return len - 1;
    }

    unsigned char lead = (unsigned char)content[seqStart - 1];
    size_t expected = 0;
    if ((lead & 0xE0u) == 0xC0u) expected = 2;
    else if ((lead & 0xF0u) == 0xE0u) expected = 3;
    else if ((lead & 0xF8u) == 0xF0u) expected = 4;
    else return seqStart - 1;

    return (seqStart - 1 + expected <= len) ? len : seqStart - 1;
}

/**
 * @brief Remove <fps:N> tag from wide string for display
 */
void RemoveFpsTagW(wchar_t* text) {
    if (!text) return;

    wchar_t* fpsStart = wcsstr(text, L"<fps:");
    if (!fpsStart) return;

    wchar_t* fpsEnd = wcschr(fpsStart, L'>');
    if (!fpsEnd) return;

    /* Move everything after tag to tag position */
    memmove(fpsStart, fpsEnd + 1, (wcslen(fpsEnd + 1) + 1) * sizeof(wchar_t));
}

/**
 * @brief Parse and process <notify> tags from wide string
 *
 * Supported formats:
 * - <notify>message</notify>             - Use default notification type and timeout
 * - <notify:catime>message</notify>      - Force Catime notification window
 * - <notify:os>message</notify>          - Force OS system notification
 * - <notify:modal>message</notify>       - Force system modal dialog
 * - <notify:catime:5000>message</notify> - Catime notification with custom timeout (ms)
 *
 * @param text The text to parse (will be modified to remove tags)
 * @param hwnd Window handle for notification display
 *
 * @note This function posts a message to the main thread to show the notification,
 *       avoiding UI operations from the background watcher thread.
 */
