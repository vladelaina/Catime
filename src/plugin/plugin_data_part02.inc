
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

static size_t ClampUtf8DisplayInputLength(const char* content, size_t contentLen) {
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
static void RemoveFpsTagW(wchar_t* text) {
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
static void ParseAndShowNotifyTagW(wchar_t* text, HWND hwnd, BOOL showNotification) {
    if (!text) return;

    /* Only process notifications when plugin mode is active */
    if (!g_pluginModeActive) return;

    wchar_t* searchStart = text;

    while (1) {
        /* Find <notify tag start */
        wchar_t* notifyStart = wcsstr(searchStart, L"<notify");
        if (!notifyStart) break;

        /* Find closing > of opening tag */
        wchar_t* tagEnd = wcschr(notifyStart, L'>');
        if (!tagEnd) break;

        /* Find </notify> closing tag */
        wchar_t* closeTag = wcsstr(tagEnd, L"</notify>");
        if (!closeTag) break;

        /* Extract message content (between > and </notify>) */
        size_t msgLen = closeTag - (tagEnd + 1);
        if (msgLen == 0 || msgLen > 1024) {
            /* Empty or too long message, skip this tag */
            searchStart = closeTag + 9;
            continue;
        }

        wchar_t message[1025] = {0};
        wcsncpy(message, tagEnd + 1, msgLen);
        message[msgLen] = L'\0';

        /* Parse notification type and timeout from tag attributes */
        /* Format: <notify> or <notify:type> or <notify:type:timeout> */
        int notifyType = -1;  /* -1 = use default */
        int customTimeout = 0;  /* 0 = use default */

        wchar_t* colonPos = wcschr(notifyStart + 7, L':');
        if (colonPos && colonPos < tagEnd) {
            /* Has type parameter */
            const wchar_t* typeStart = colonPos + 1;
            wchar_t* typeEnd = wcschr(typeStart, L':');
            if (!typeEnd || typeEnd > tagEnd) {
                typeEnd = tagEnd;
            }

            size_t typeLen = typeEnd - typeStart;
            if (typeLen > 0 && typeLen < 16) {
                wchar_t typeStr[16] = {0};
                wcsncpy(typeStr, typeStart, typeLen);
                typeStr[typeLen] = L'\0';

                if (_wcsicmp(typeStr, L"catime") == 0) {
                    notifyType = NOTIFICATION_TYPE_CATIME;
                } else if (_wcsicmp(typeStr, L"os") == 0) {
                    notifyType = NOTIFICATION_TYPE_OS;
                } else if (_wcsicmp(typeStr, L"modal") == 0) {
                    notifyType = NOTIFICATION_TYPE_SYSTEM_MODAL;
                }
            }

            /* Check for timeout parameter (only for toast) */
            if (typeEnd < tagEnd && *typeEnd == L':') {
                const wchar_t* timeoutStart = typeEnd + 1;
                ParseNonNegativeIntLimitedW(timeoutStart, tagEnd, &customTimeout);
                if (customTimeout < 0) customTimeout = 0;
                if (customTimeout > 60000) customTimeout = 60000;  /* Max 60 seconds */
            }
        }

        if (showNotification && hwnd) {
            /* Throttle: check if enough time has passed since last notification */
            /* Note: GetTickCount wraps around after ~49.7 days, but the subtraction
             * still works correctly due to unsigned arithmetic */
            DWORD now = GetTickCount();
            DWORD elapsed = now - g_lastNotifyTime;
            if (elapsed >= NOTIFY_MIN_INTERVAL_MS) {
                /* Store pending notification for main thread to process
                 * Note: This is called from ParseContent which already holds g_dataCS,
                 * so we don't need to acquire the lock here. The main thread will
                 * acquire the lock in PluginData_ProcessPendingNotification. */
                wcsncpy(g_pendingNotify.message, message, 1024);
                g_pendingNotify.message[1024] = L'\0';
                g_pendingNotify.type = notifyType;
                g_pendingNotify.timeout = customTimeout;
                g_pendingNotify.pending = TRUE;

                /* Post message to main thread to show notification */
                if (!IsValidPluginDataNotifyWindow(hwnd) ||
                    !PostMessage(hwnd, WM_PLUGIN_NOTIFY, 0, 0)) {
                    g_pendingNotify.pending = FALSE;
                } else {
                    g_lastNotifyTime = now;
                }
            }
        }

        /* Remove the entire <notify>...</notify> tag from text */
        wchar_t* afterClose = closeTag + 9;  /* Skip "</notify>" */
        memmove(notifyStart, afterClose, (wcslen(afterClose) + 1) * sizeof(wchar_t));

        /* Continue searching from same position (text shifted) */
        searchStart = notifyStart;
    }
}

static BOOL PreviewReplaceExitTagW(wchar_t* text, int* textLen) {
    if (!text || !textLen) return FALSE;

    wchar_t* start = wcsstr(text, L"<exit>");
    wchar_t* end = wcsstr(text, L"</exit>");
    if (!start || !end || end <= start) {
        return FALSE;
    }

    int seconds = 3;
    const wchar_t* numStart = start + 6;
    BOOL validNumber = TRUE;

    if (numStart < end) {
        while (numStart < end && (*numStart == L' ' || *numStart == L'\t')) {
            numStart++;
        }

        if (numStart < end) {
            int parsed = 0;
            const wchar_t* parsePtr = numStart;
            while (parsePtr < end && *parsePtr != L' ' && *parsePtr != L'\t') {
                int digit = (int)(*parsePtr - L'0');
                if (*parsePtr < L'0' || *parsePtr > L'9' ||
                    parsed > (INT_MAX - digit) / 10) {
                    validNumber = FALSE;
                    break;
                }
                parsed = parsed * 10 + digit;
                parsePtr++;
            }

            if (validNumber) {
                if (parsed > 0) {
                    seconds = parsed > PLUGIN_PREVIEW_MAX_EXIT_COUNTDOWN_SECONDS
                        ? PLUGIN_PREVIEW_MAX_EXIT_COUNTDOWN_SECONDS
                        : parsed;
                } else {
                    validNumber = FALSE;
                }
            }
        }
    }

    if (!validNumber) {
        return FALSE;
    }

    wchar_t countdownNum[16];
    _snwprintf_s(countdownNum, 16, _TRUNCATE, L"%d", seconds);

    wchar_t* suffixStart = end + 7;
    size_t suffixLen = wcslen(suffixStart);
    size_t numLen = wcslen(countdownNum);
    memmove(start + numLen, suffixStart, (suffixLen + 1) * sizeof(wchar_t));
    memcpy(start, countdownNum, numLen * sizeof(wchar_t));
    *textLen = (int)wcslen(text);
    return TRUE;
