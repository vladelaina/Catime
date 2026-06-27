/**
 * @file window_commands_plugin.c
 * @brief Plugin-related command handlers
 */

#include "window_procedure/window_commands.h"
#include "window_procedure/window_helpers.h"
#include "window_procedure/window_procedure.h"
#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "plugin/plugin_exit.h"
#include "dialog/dialog_common.h"
#include "timer/timer.h"
#include "timer/main_timer.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "window.h"
#include "pomodoro.h"
#include "notification.h"
#include "audio_player.h"
#include "plugin/plugin_process.h"
#include "log.h"
#include "language.h"
#include "menu_preview.h"
#include "utils/string_convert.h"
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

#if CLOCK_IDM_PLUGINS_BASE + MAX_PLUGINS > CLOCK_IDM_PLUGINS_SETTINGS_BASE
#error "Plugin menu command range overlaps plugin settings command range"
#endif

#define CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID 42431
#define CUSTOM_TEXT_DISPLAY_PREVIEW_DELAY_MS 180
#define CUSTOM_TEXT_DISPLAY_MAX_CHARS 4096
#define CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES (64u * 1024u)
#define CUSTOM_TEXT_DISPLAY_FILENAME_W L"custom_display.txt"
#define CUSTOM_TEXT_DISPLAY_EMPTY_PREVIEW_TEXT_W L"Ciallo\uff5e(\u2220\u30fb\u03c9<)\u2312\u2605"

typedef struct {
    HWND owner;
    BOOL pluginsStopped;
    BOOL initializing;
    wchar_t contentPath[MAX_PATH];
    wchar_t* originalText;
    HFONT editFont;
} CustomTextDisplayState;

/* ============================================================================
 * Plugin Command Handlers
 * ============================================================================ */

/**
 * @brief Handle plugin start/stop toggle
 */
static BOOL HandlePluginToggle(HWND hwnd, int pluginIndex) {
    /* Check if this plugin is already running - toggle off */
    if (PluginManager_GetActivePluginIndex() == pluginIndex ||
        PluginManager_IsPluginRunning(pluginIndex)) {
        if (!PluginManager_StopPlugin(pluginIndex)) {
            PluginData_Clear();
        }

        /* Prevent countdown completion notification from triggering */
        countdown_message_shown = true;

        /* Switch to idle state - don't reset timer to avoid 1-minute fallback */
        CLOCK_SHOW_CURRENT_TIME = false;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = true;
        CLOCK_TOTAL_TIME = 0;
        countdown_elapsed_time = 0;
        MainTimer_Stop();
        InvalidateRect(hwnd, NULL, TRUE);
        return TRUE;
    }

    /* Plugin not running - check if it needs security confirmation first */
    /* If security dialog is needed, don't change any state yet */
    if (PluginManager_NeedsSecurityCheck(pluginIndex)) {
        /* Show security dialog without changing current state */
        PluginManager_StartPlugin(pluginIndex);
        /* State will be changed in HandleDialogPluginSecurity when user confirms */
        return TRUE;
    }

    /* Plugin is trusted - proceed with state change and launch */

    /* Stop notification sound */
    StopNotificationSound();

    /* Prevent countdown completion notification from triggering */
    countdown_message_shown = true;

    /* Reset timer flags */
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;

    /* Stop internal timer */
    MainTimer_Stop();

    /* Reset Pomodoro if active */
    current_pomodoro_phase = POMODORO_PHASE_IDLE;

    /* Reset timer values */
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    countup_elapsed_time = 0;

    /* Show loading message */
    PluginInfo pluginInfo;
    BOOL hasPluginInfo = PluginManager_CopyPlugin(pluginIndex, &pluginInfo);
    if (hasPluginInfo) {
        wchar_t loadingText[256];
        PluginData_SetOutputDirectoryFromPluginPath(pluginInfo.path);
        _snwprintf_s(loadingText, 256, _TRUNCATE, L"Loading %ls...", pluginInfo.displayName);
        PluginData_SetText(loadingText);
    }

    /* Start plugin */
    BOOL startResult = PluginManager_StartPlugin(pluginIndex);

    if (!startResult) {
        /* Launch failed - show error */
        LOG_ERROR("Plugin failed to start: %ls", hasPluginInfo ? pluginInfo.displayName : L"unknown");

        const wchar_t* errorMsg = PluginProcess_GetLastError();
        if (errorMsg && errorMsg[0] != L'\0') {
            PluginData_SetStatusText(errorMsg);
        } else {
            PluginData_SetStatusText(L"FAIL");
        }
    }

    /* Check if animated gradient needs timer for smooth animation */
    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);  /* 15 FPS for smooth animation */
    }

    /* Ensure window visible and consistent with topmost policy */
    EnsureWindowVisibleWithTopmostState(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);

    return TRUE;
}

static wchar_t* DuplicateWideString(const wchar_t* text) {
    const wchar_t* source = text ? text : L"";
    size_t len = wcslen(source);
    if (len > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        return NULL;
    }

    wchar_t* copy = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!copy) {
        return NULL;
    }

    memcpy(copy, source, (len + 1) * sizeof(wchar_t));
    return copy;
}

static size_t ClampUtf8ByteLength(const char* content, size_t contentLen) {
    if (!content || contentLen == 0) {
        return 0;
    }

    size_t len = contentLen;
    size_t seqStart = len;
    while (seqStart > 0 && (((unsigned char)content[seqStart - 1] & 0xC0u) == 0x80u)) {
        seqStart--;
    }

    if (seqStart == 0) {
        return 0;
    }

    if (seqStart == len) {
        unsigned char last = (unsigned char)content[len - 1];
        if ((last & 0x80u) == 0) {
            return len;
        }
        return ((last & 0xC0u) == 0xC0u) ? len - 1 : len;
    }

    unsigned char lead = (unsigned char)content[seqStart - 1];
    size_t expected = 0;
    if ((lead & 0xE0u) == 0xC0u) {
        expected = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        expected = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        expected = 4;
    } else {
        return (lead & 0x80u) == 0 ? seqStart : seqStart - 1;
    }

    return (seqStart - 1 + expected <= len) ? len : seqStart - 1;
}

static void ClampCustomTextDisplayText(wchar_t* text) {
    if (!text) {
        return;
    }

    size_t len = wcslen(text);
    if (len <= CUSTOM_TEXT_DISPLAY_MAX_CHARS) {
        return;
    }

    size_t limit = CUSTOM_TEXT_DISPLAY_MAX_CHARS;
    if (limit > 0 && text[limit - 1] >= 0xD800 && text[limit - 1] <= 0xDBFF) {
        limit--;
    }
    text[limit] = L'\0';
}

static BOOL EnsureParentDirectoryExists(const wchar_t* filePath) {
    if (!filePath || filePath[0] == L'\0') {
        return FALSE;
    }

    wchar_t dirPath[MAX_PATH];
    wcsncpy_s(dirPath, _countof(dirPath), filePath, _TRUNCATE);

    wchar_t* lastSlash = wcsrchr(dirPath, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(dirPath, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == dirPath) {
        return FALSE;
    }

    *lastSlash = L'\0';
    int result = SHCreateDirectoryExW(NULL, dirPath, NULL);
    return result == ERROR_SUCCESS ||
           result == ERROR_ALREADY_EXISTS ||
           result == ERROR_FILE_EXISTS;
}

static BOOL GetCustomTextDisplayPath(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    buffer[0] = L'\0';

    char configPath[MAX_PATH] = {0};
    GetConfigPath(configPath, sizeof(configPath));
    if (configPath[0] == '\0') {
        return FALSE;
    }

    wchar_t configPathW[MAX_PATH] = {0};
    if (!Utf8ToWide(configPath, configPathW, _countof(configPathW))) {
        return FALSE;
    }

    wchar_t* lastSlash = wcsrchr(configPathW, L'\\');
    wchar_t* lastForwardSlash = wcsrchr(configPathW, L'/');
    if (!lastSlash || (lastForwardSlash && lastForwardSlash > lastSlash)) {
        lastSlash = lastForwardSlash;
    }
    if (!lastSlash || lastSlash == configPathW) {
        return FALSE;
    }

    *lastSlash = L'\0';
    int written = _snwprintf_s(buffer, bufferSize, _TRUNCATE,
                               L"%s\\%s", configPathW,
                               CUSTOM_TEXT_DISPLAY_FILENAME_W);
    if (written < 0 || (size_t)written >= bufferSize) {
        buffer[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

static wchar_t* LoadCustomTextDisplayContent(const wchar_t* filePath) {
    if (!filePath || filePath[0] == L'\0') {
        return DuplicateWideString(L"");
    }

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
            LOG_WARNING("Custom text display failed to open content file for reading: %lu", error);
        }
        return DuplicateWideString(L"");
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return DuplicateWideString(L"");
    }

    DWORD bytesToRead = fileSize.QuadPart > CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES
                            ? CUSTOM_TEXT_DISPLAY_MAX_FILE_BYTES
                            : (DWORD)fileSize.QuadPart;
    char* bytes = (char*)malloc((size_t)bytesToRead + 1);
    if (!bytes) {
        CloseHandle(hFile);
        return DuplicateWideString(L"");
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, bytes, bytesToRead, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!ok) {
        LOG_WARNING("Custom text display failed to read content file: %lu", GetLastError());
        free(bytes);
        return DuplicateWideString(L"");
    }

    size_t validBytes = ClampUtf8ByteLength(bytes, bytesRead);
    bytes[validBytes] = '\0';
    wchar_t* content = Utf8ToWideAlloc(bytes);
    free(bytes);
    if (!content) {
        return DuplicateWideString(L"");
    }

    ClampCustomTextDisplayText(content);
    return content;
}

static BOOL GetCustomTextDisplayText(HWND hwndDlg, wchar_t** outText) {
    if (!outText) {
        return FALSE;
    }
    *outText = NULL;

    HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
    if (!hwndEdit) {
        return FALSE;
    }

    int textLen = GetWindowTextLengthW(hwndEdit);
    if (textLen < 0 || textLen > CUSTOM_TEXT_DISPLAY_MAX_CHARS) {
        return FALSE;
    }

    wchar_t* text = (wchar_t*)malloc(((size_t)textLen + 1) * sizeof(wchar_t));
    if (!text) {
        return FALSE;
    }

    GetWindowTextW(hwndEdit, text, textLen + 1);
    *outText = text;
    return TRUE;
}

static void MoveEditCaretToEnd(HWND hwndEdit) {
    if (!hwndEdit) {
        return;
    }

    int textLen = GetWindowTextLengthW(hwndEdit);
    if (textLen < 0) {
        textLen = 0;
    }

    SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);
}

static HFONT CreateCustomTextDisplayEditFont(HWND hwndEdit) {
    if (!hwndEdit) {
        return NULL;
    }

    char activeFontFile[MAX_PATH] = {0};
    char activeFontInternalName[MAX_PATH] = {0};
    GetActiveFont(activeFontFile, activeFontInternalName, sizeof(activeFontInternalName));

    if (activeFontInternalName[0] == '\0') {
        return NULL;
    }

    wchar_t activeFaceName[MAX_PATH] = {0};
    if (!Utf8ToWide(activeFontInternalName, activeFaceName, _countof(activeFaceName))) {
        return NULL;
    }

    LOGFONTW lf = {0};
    HFONT currentFont = (HFONT)SendMessageW(hwndEdit, WM_GETFONT, 0, 0);
    if (!currentFont || GetObjectW(currentFont, sizeof(lf), &lf) != sizeof(lf)) {
        HFONT defaultFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        if (!defaultFont || GetObjectW(defaultFont, sizeof(lf), &lf) != sizeof(lf)) {
            return NULL;
        }
    }

    wcsncpy_s(lf.lfFaceName, _countof(lf.lfFaceName), activeFaceName, _TRUNCATE);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;

    HFONT editFont = CreateFontIndirectW(&lf);
    if (!editFont) {
        return NULL;
    }

    return editFont;
}

void RefreshCustomTextDisplayDialogFont(void) {
    HWND hwndDlg = Dialog_GetInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY);
    if (!hwndDlg || !IsWindow(hwndDlg)) {
        return;
    }

    CustomTextDisplayState* state =
        (CustomTextDisplayState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);
    if (!state) {
        return;
    }

    HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
    HFONT newFont = CreateCustomTextDisplayEditFont(hwndEdit);
    if (!newFont) {
        return;
    }

    HFONT oldFont = state->editFont;
    state->editFont = newFont;
    SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)state->editFont, TRUE);

    if (oldFont && oldFont != state->editFont) {
        DeleteObject(oldFont);
    }
    InvalidateRect(hwndEdit, NULL, TRUE);
}

static BOOL SaveCustomTextDisplayContent(const wchar_t* filePath, const wchar_t* text) {
    if (!filePath || filePath[0] == L'\0') {
        LOG_WARNING("Custom text display could not resolve content file path");
        return FALSE;
    }

    char* utf8 = WideToUtf8Alloc(text ? text : L"");
    if (!utf8) {
        LOG_WARNING("Custom text display failed to convert content to UTF-8");
        return FALSE;
    }

    if (!EnsureParentDirectoryExists(filePath)) {
        LOG_WARNING("Custom text display could not ensure content directory exists");
        free(utf8);
        return FALSE;
    }

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG_WARNING("Custom text display failed to open content file for writing: %lu", GetLastError());
        free(utf8);
        return FALSE;
    }

    DWORD bytesToWrite = (DWORD)strlen(utf8);
    DWORD bytesWritten = 0;
    BOOL ok = bytesToWrite == 0 ||
              WriteFile(hFile, utf8, bytesToWrite, &bytesWritten, NULL);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(hFile);
    free(utf8);

    if (!ok || bytesWritten != bytesToWrite) {
        LOG_WARNING("Custom text display failed to write content file: %lu", error);
        return FALSE;
    }

    return TRUE;
}

static void ApplyCustomTextDisplayWindowState(HWND hwnd, BOOL preserveDialogFocus) {
    countdown_message_shown = true;

    if (!PluginData_HasCatimeTag()) {
        MainTimer_Stop();
        CLOCK_SHOW_CURRENT_TIME = false;
        CLOCK_COUNT_UP = false;
        CLOCK_IS_PAUSED = false;
    }

    char activeColor[COLOR_HEX_BUFFER];
    GetActiveColor(activeColor, sizeof(activeColor));
    if (IsGradientNameAnimated(activeColor)) {
        MainTimer_Start(hwnd, 66);
    }

    if (!preserveDialogFocus) {
        EnsureWindowVisibleWithTopmostState(hwnd);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void StopPluginsForCustomTextDisplay(CustomTextDisplayState* state) {
    if (state && state->pluginsStopped) {
        return;
    }
    PluginManager_StopAllPlugins();
    if (state) {
        state->pluginsStopped = TRUE;
    }
}

static BOOL ApplyCustomTextDisplayPreview(HWND hwnd, const wchar_t* text,
                                          const wchar_t* sourcePath,
                                          BOOL preserveDialogFocus) {
    const wchar_t* previewText = (text && text[0] != L'\0')
                                     ? text
                                     : CUSTOM_TEXT_DISPLAY_EMPTY_PREVIEW_TEXT_W;
    if (!PluginData_SetPreviewTextWithSource(previewText, sourcePath)) {
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    ApplyCustomTextDisplayWindowState(hwnd, preserveDialogFocus);
    return TRUE;
}

static BOOL QueueCustomTextDisplayPreview(HWND hwndDlg) {
    KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
    return SetTimer(hwndDlg,
                    CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID,
                    CUSTOM_TEXT_DISPLAY_PREVIEW_DELAY_MS,
                    NULL) != 0;
}

static BOOL ApplyCustomTextDisplayText(HWND hwndDlg, CustomTextDisplayState* state,
                                       BOOL preserveDialogFocus) {
    wchar_t* text = NULL;
    if (!state || !GetCustomTextDisplayText(hwndDlg, &text)) {
        return FALSE;
    }

    if (!SaveCustomTextDisplayContent(state->contentPath, text)) {
        free(text);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    StopPluginsForCustomTextDisplay(state);
    BOOL applied = ApplyCustomTextDisplayPreview(state->owner, text,
                                                 state->contentPath,
                                                 preserveDialogFocus);
    free(text);
    return applied;
}

static INT_PTR CALLBACK CustomTextDisplayDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    CustomTextDisplayState* state =
        (CustomTextDisplayState*)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);

    switch (msg) {
        case WM_INITDIALOG: {
            state = (CustomTextDisplayState*)lParam;
            if (!state) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, (LONG_PTR)state);
            Dialog_RegisterInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY, hwndDlg);
            state->initializing = TRUE;

            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Custom Text Display"));
            SetDlgItemTextW(hwndDlg, IDOK, GetLocalizedString(NULL, L"OK"));
            SetDlgItemTextW(hwndDlg, IDCANCEL, GetLocalizedString(NULL, L"Close"));
            SetDlgItemTextW(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_HINT,
                            GetLocalizedString(NULL, L"Use <md>...</md> to enable Markdown"));
            Dialog_CenterOnPrimaryScreen(hwndDlg);

            HWND hwndEdit = GetDlgItem(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT);
            state->editFont = CreateCustomTextDisplayEditFont(hwndEdit);
            if (state->editFont) {
                SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)state->editFont, TRUE);
            }
            SendMessageW(hwndEdit, EM_LIMITTEXT, CUSTOM_TEXT_DISPLAY_MAX_CHARS, 0);
            SetDlgItemTextW(hwndDlg, IDC_CUSTOM_TEXT_DISPLAY_TEXT,
                            state && state->originalText ? state->originalText : L"");
            state->initializing = FALSE;
            StopPluginsForCustomTextDisplay(state);
            ApplyCustomTextDisplayPreview(state->owner,
                                          state->originalText ? state->originalText : L"",
                                          state->contentPath,
                                          TRUE);
            SetFocus(hwndEdit);
            MoveEditCaretToEnd(hwndEdit);
            return FALSE;
        }

        case WM_TIMER:
            if (wParam == CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                ApplyCustomTextDisplayText(hwndDlg, state, TRUE);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_CUSTOM_TEXT_DISPLAY_TEXT && HIWORD(wParam) == EN_CHANGE) {
                if (state && state->initializing) {
                    return TRUE;
                }
                if (!QueueCustomTextDisplayPreview(hwndDlg)) {
                    ApplyCustomTextDisplayText(hwndDlg, state, TRUE);
                }
                return TRUE;
            }

            if (LOWORD(wParam) == IDOK) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                if (!ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }

            if (LOWORD(wParam) == IDCANCEL) {
                KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
                if (!ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                    return TRUE;
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
            if (!ApplyCustomTextDisplayText(hwndDlg, state, FALSE)) {
                return TRUE;
            }
            DestroyWindow(hwndDlg);
            return TRUE;

        case WM_DESTROY:
            KillTimer(hwndDlg, CUSTOM_TEXT_DISPLAY_PREVIEW_TIMER_ID);
            Dialog_UnregisterInstanceForWindow(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY, hwndDlg);
            if (state) {
                if (state->editFont) {
                    DeleteObject(state->editFont);
                }
                free(state->originalText);
                free(state);
                SetWindowLongPtrW(hwndDlg, GWLP_USERDATA, 0);
            }
            return TRUE;
    }

    return FALSE;
}

static BOOL ShowCustomTextDisplayDialog(HWND hwnd) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_CUSTOM_TEXT_DISPLAY);
        SetForegroundWindow(existing);
        return TRUE;
    }

    CustomTextDisplayState* state =
        (CustomTextDisplayState*)calloc(1, sizeof(CustomTextDisplayState));
    if (!state) {
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    state->owner = hwnd;
    if (!GetCustomTextDisplayPath(state->contentPath, _countof(state->contentPath))) {
        free(state);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    state->originalText = LoadCustomTextDisplayContent(state->contentPath);
    if (!state->originalText) {
        state->originalText = DuplicateWideString(L"");
    }

    HWND hwndDlg = CreateDialogParamW(GetModuleHandleW(NULL),
                                      MAKEINTRESOURCEW(IDD_CUSTOM_TEXT_DISPLAY_DIALOG),
                                      hwnd,
                                      CustomTextDisplayDlgProc,
                                      (LPARAM)state);
    if (!hwndDlg) {
        free(state->originalText);
        free(state);
        MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    ShowWindow(hwndDlg, SW_SHOW);
    return TRUE;
}

/**
 * @brief Handle custom text display command
 */
static BOOL HandleCustomTextDisplay(HWND hwnd) {
    return ShowCustomTextDisplayDialog(hwnd);
}

/* ============================================================================
 * Plugin Exit Handler (for <exit> tag)
 * ============================================================================ */

/**
 * @brief Handle plugin exit request (from <exit> tag countdown)
 * Reuses the same logic as manually clicking to stop a plugin
 */
void HandlePluginExit(HWND hwnd) {
    /* Cancel any pending exit countdown */
    PluginExit_Cancel();

    /* Stop all plugins */
    PluginManager_StopAllPlugins();

    /* Prevent countdown completion notification from triggering */
    countdown_message_shown = true;

    /* Switch to idle state - don't reset timer to avoid 1-minute fallback */
    CLOCK_SHOW_CURRENT_TIME = false;
    CLOCK_COUNT_UP = false;
    CLOCK_IS_PAUSED = true;
    CLOCK_TOTAL_TIME = 0;
    countdown_elapsed_time = 0;
    MainTimer_Stop();
    InvalidateRect(hwnd, NULL, TRUE);

    LOG_INFO("Plugin exit completed via <exit> tag");
}

/* ============================================================================
 * Plugin Command Dispatcher
 * ============================================================================ */

BOOL HandlePluginCommand(HWND hwnd, UINT cmd) {
    /* Plugin start/stop */
    if (cmd >= CLOCK_IDM_PLUGINS_BASE && cmd < CLOCK_IDM_PLUGINS_BASE + MAX_PLUGINS) {
        int pluginIndex = cmd - CLOCK_IDM_PLUGINS_BASE;
        return HandlePluginToggle(hwnd, pluginIndex);
    }

    /* Plugin settings (deprecated but kept for safety) */
    if (cmd >= CLOCK_IDM_PLUGINS_SETTINGS_BASE && cmd < CLOCK_IDM_CUSTOM_TEXT_DISPLAY) {
        return TRUE;
    }

    /* Custom text display */
    if (cmd == CLOCK_IDM_CUSTOM_TEXT_DISPLAY) {
        return HandleCustomTextDisplay(hwnd);
    }

    /* Open plugin folder */
    if (cmd == CLOCK_IDM_PLUGINS_OPEN_DIR) {
        PluginManager_OpenPluginFolder();
        return TRUE;
    }

    return FALSE;
}
