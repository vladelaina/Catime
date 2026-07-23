}

/**
 * @brief Parse plain text content and update display text
 *
 * The file content is displayed as-is, supporting:
 * - Plain text
 * - Multi-line text (real newlines, no \n escaping needed)
 * - Markdown formatting
 */
static PluginParseResult ParseContent(const char* content, size_t contentLen,
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

/*
 * Design note: output.txt is a small local IPC surface scoped to the active
 * plugin folder. Catime starts the plugin script and displays that folder's
 * output.txt while plugin mode is active. Outside plugin mode, Catime
 * intentionally ignores this file.
 * Catime-owned local state text such as "Loading..." or "FAIL" is also allowed
 * to override the file-driven content when needed.
 */
/* Plugin output file name */
#define PLUGIN_OUTPUT_FILENAME "output.txt"
#define PLUGIN_OUTPUT_FILENAME_W L"output.txt"

static BOOL GetDefaultPluginOutputDirectoryW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)INT_MAX) {
        return FALSE;
    }
    buffer[0] = L'\0';

    char pluginDirUtf8[MAX_PATH] = {0};
    GetPluginsFolderPath(pluginDirUtf8, MAX_PATH);
    if (pluginDirUtf8[0] == '\0' ||
        MultiByteToWideChar(CP_UTF8, 0, pluginDirUtf8, -1,
                            buffer, (int)bufferSize) <= 0) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL SetDefaultPluginOutputDirectoryLocked(void) {
    wchar_t defaultDir[MAX_PATH];
    if (!GetDefaultPluginOutputDirectoryW(defaultDir, MAX_PATH)) {
        g_pluginOutputDirectory[0] = L'\0';
        return FALSE;
    }

    wcsncpy(g_pluginOutputDirectory, defaultDir, MAX_PATH - 1);
    g_pluginOutputDirectory[MAX_PATH - 1] = L'\0';
    return TRUE;
}

static BOOL EnsurePluginOutputDirectoryLocked(void) {
    if (g_pluginOutputDirectory[0] != L'\0') {
        return TRUE;
    }
    return SetDefaultPluginOutputDirectoryLocked();
}

static BOOL GetPluginOutputDirectory(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    EnterCriticalSection(&g_dataCS);
    BOOL ok = EnsurePluginOutputDirectoryLocked();
    if (ok) {
        wcsncpy(buffer, g_pluginOutputDirectory, bufferSize - 1);
        buffer[bufferSize - 1] = L'\0';
        ok = wcslen(g_pluginOutputDirectory) < bufferSize;
    }
    LeaveCriticalSection(&g_dataCS);
    return ok;
}

/**
 * @brief Get plugin output file path
 * @return TRUE if successful, FALSE otherwise
 */
static BOOL GetPluginOutputPathW(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0 || bufferSize > (size_t)MAXDWORD) {
        return FALSE;
    }
    buffer[0] = L'\0';

    wchar_t outputDir[MAX_PATH];
    if (!GetPluginOutputDirectory(outputDir, MAX_PATH)) {
        buffer[0] = L'\0';
        return FALSE;
    }

    int written = _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                               L"%s\\%s", outputDir, PLUGIN_OUTPUT_FILENAME_W);
    if (written < 0 || (size_t)written >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static void SetDisplaySourcePathLocked(const wchar_t* sourcePath) {
    if (!sourcePath || sourcePath[0] == L'\0' || wcslen(sourcePath) >= MAX_PATH) {
        g_displaySourcePath[0] = L'\0';
        return;
    }

    wcsncpy(g_displaySourcePath, sourcePath, MAX_PATH - 1);
    g_displaySourcePath[MAX_PATH - 1] = L'\0';
}

static BOOL GetDirectoryFromPathW(const wchar_t* path,
                                  wchar_t* directory,
                                  size_t directorySize) {
    if (!path || !directory || directorySize == 0) return FALSE;
    directory[0] = L'\0';
