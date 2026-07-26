/**
 * @file plugin_data_parser.c
 * @brief UTF-8 plugin content parsing and display updates.
 */

#include "plugin_data_internal.h"

PluginParseResult ParseContent(const char* content, size_t contentLen,
                                      BOOL suppressSideEffects,
                                      BOOL* displayChangedOut,
                                      BOOL* timerRecheckOut) {
    if (displayChangedOut) {
        *displayChangedOut = FALSE;
    }
    if (timerRecheckOut) {
        *timerRecheckOut = FALSE;
    }
    if (!content || contentLen == 0) return PLUGIN_PARSE_FAILED;

    DWORD parsedPollInterval = 0;
    BOOL hasFpsTag = TryParseFpsPollInterval(content, &parsedPollInterval, NULL);
    size_t displayInputLen = ClampUtf8DisplayInputLength(content, contentLen);

    /* Convert outside g_dataCS so paint-time reads are not blocked by UTF-8 work. */
    int requiredLen = MultiByteToWideChar(CP_UTF8, 0, content, (int)displayInputLen, NULL, 0);
    if (requiredLen <= 0) {
        return PLUGIN_PARSE_FAILED;
    }

    if (requiredLen > INT_MAX - 1) {
        return PLUGIN_PARSE_FAILED;
    }
    size_t requiredSize = (size_t)(requiredLen + 1);
    if (requiredSize > SIZE_MAX / sizeof(wchar_t)) {
        return PLUGIN_PARSE_FAILED;
    }

    wchar_t stackText[PLUGIN_DISPLAY_STACK_WCHARS];
    wchar_t* heapText = NULL;
    wchar_t* displayText = stackText;
    if (requiredSize > _countof(stackText)) {
        heapText = (wchar_t*)malloc(requiredSize * sizeof(wchar_t));
        if (!heapText) {
            LOG_ERROR("PluginData: Failed to allocate %zu bytes", requiredSize * sizeof(wchar_t));
            return PLUGIN_PARSE_TRANSIENT_FAILURE;
        }
        displayText = heapText;
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, content, (int)displayInputLen,
                                  displayText, (int)requiredSize);
    if (len <= 0) {
        free(heapText);
        return PLUGIN_PARSE_FAILED;
    }
    displayText[len] = L'\0';

    /* Remove BOM if present */
    if (displayText[0] == 0xFEFF) {
        memmove(displayText, &displayText[1], len * sizeof(wchar_t));
        len--;
    }

    /* Trim trailing whitespace */
    while (len > 0 && (displayText[len - 1] == L'\n' ||
                       displayText[len - 1] == L'\r' ||
                       displayText[len - 1] == L' ')) {
        displayText[--len] = L'\0';
    }

    /* Remove <fps:N> tag from display before taking the shared data lock. */
    RemoveFpsTagW(displayText);

    EnterCriticalSection(&g_dataCS);

    if (!g_pluginModeActive) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_FAILED;
    }

    BOOL hadCatimeTag = PluginDisplayHasCatimeTagLocked();

    if (!suppressSideEffects) {
        ApplyContentPollInterval(hasFpsTag, parsedPollInterval);
    }

    if (PluginExit_IsInProgress()) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_OK;
    }

    /* Process <notify> tags while holding g_dataCS for pending-notification state. */
    ParseAndShowNotifyTagW(displayText, g_hNotifyWnd, !suppressSideEffects);

    len = (int)wcslen(displayText);
    size_t displaySize = (size_t)len + 1;

    BOOL displayChanged = !g_hasPluginData ||
                          !g_pluginDisplayText ||
                          wcscmp(g_pluginDisplayText, displayText) != 0;
    BOOL hasExitTag = wcsstr(displayText, L"<exit>") != NULL &&
                      wcsstr(displayText, L"</exit>") != NULL;

    if (!displayChanged && !hasExitTag) {
        g_hasPluginData = TRUE;
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_OK;
    }

    if (!EnsurePluginDisplayTextCapacityLocked(displaySize)) {
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        return PLUGIN_PARSE_TRANSIENT_FAILURE;
    }

    memcpy(g_pluginDisplayText, displayText, displaySize * sizeof(wchar_t));

    /* Process <exit> tag - if countdown starts, set data flag and return */
    if (suppressSideEffects
            ? PreviewReplaceExitTagW(g_pluginDisplayText, &len)
            : PluginExit_ParseTag(g_pluginDisplayText, &len, g_pluginDisplayTextLen)) {
        g_hasPluginData = TRUE;
        BOOL hasCatimeTag = PluginDisplayHasCatimeTagLocked();
        LeaveCriticalSection(&g_dataCS);
        free(heapText);
        if (displayChangedOut) {
            *displayChangedOut = TRUE;
        }
        if (timerRecheckOut && hadCatimeTag != hasCatimeTag) {
            *timerRecheckOut = TRUE;
        }
        return PLUGIN_PARSE_OK;
    }

    g_hasPluginData = TRUE;
    BOOL hasCatimeTag = PluginDisplayHasCatimeTagLocked();
    LeaveCriticalSection(&g_dataCS);

    free(heapText);
    if (displayChangedOut) {
        *displayChangedOut = displayChanged;
    }
    if (timerRecheckOut && hadCatimeTag != hasCatimeTag) {
        *timerRecheckOut = TRUE;
    }
    return PLUGIN_PARSE_OK;
}
