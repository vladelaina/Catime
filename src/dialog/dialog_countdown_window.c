/**
 * @file dialog_countdown_window.c
 * @brief Window class, controls, and creation.
 */

#include "dialog_countdown_internal.h"

static void CountdownPopulateInitialInput(CountdownDialogState* state) {
    if (!state || !state->hwndEdit) return;

    int seconds = DialogInput_GetInitialSeconds(
        state->input.dialogId, state->input.pomodoroTimeIndex);
    if (seconds <= 0) return;

    char value[64] = {0};
    wchar_t wideValue[64] = {0};
    Dialog_FormatSecondsToString(seconds, value, sizeof(value));
    if (MultiByteToWideChar(CP_UTF8, 0, value, -1, wideValue,
                            _countof(wideValue)) > 0) {
        SetWindowTextW(state->hwndEdit, wideValue);
    }
}

BOOL CountdownRegisterWindowClass(void) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSEXW existing = {0};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, COUNTDOWN_WINDOW_CLASS_NAME, &existing)) {
        return TRUE;
    }

    WNDCLASSEXW windowClass = {0};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    windowClass.lpfnWndProc = CountdownDialogProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = NULL;
    windowClass.lpszClassName = COUNTDOWN_WINDOW_CLASS_NAME;
    if (RegisterClassExW(&windowClass)) {
        return TRUE;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

BOOL CountdownCreateControls(HWND hwnd, CountdownDialogState* state) {
    if (!hwnd || !state) {
        return FALSE;
    }

    CountdownLoadTexts(state);
    CountdownRefreshPalette(state);

    DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                       ES_AUTOHSCROLL | ES_LEFT | ES_NOHIDESEL;
    state->hwndEdit = CreateWindowExW(
        0, L"EDIT", L"", editStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_EDIT,
        GetModuleHandleW(NULL), NULL);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                         BS_OWNERDRAW | BS_NOTIFY;
    state->hwndCancel = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDCANCEL,
        GetModuleHandleW(NULL), NULL);
    state->hwndStart = CreateWindowExW(
        0, L"BUTTON", state->startText, buttonStyle | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)CLOCK_IDC_BUTTON_OK,
        GetModuleHandleW(NULL), NULL);
    state->hwndClose = CreateWindowExW(
        0, L"BUTTON", state->cancelText, buttonStyle,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)COUNTDOWN_CLOSE_BUTTON_ID,
        GetModuleHandleW(NULL), NULL);

    if (!state->hwndEdit || !state->hwndCancel || !state->hwndStart ||
        !state->hwndClose) {
        return FALSE;
    }

    DialogModern_ApplyTheme(hwnd, state->darkMode);
    DialogModern_ApplyTheme(state->hwndEdit, state->darkMode);

    SendMessageW(state->hwndEdit, EM_SETLIMITTEXT,
                 (WPARAM)(_countof(inputText) - 1), 0);
    SendMessageW(state->hwndEdit, EM_SETMARGINS,
                 EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELONG(CountdownScaleValue(state, 2),
                          CountdownScaleValue(state, 2)));
    SendMessageW(state->hwndEdit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)L"25m 30s");

    SetWindowSubclass(state->hwndEdit, CountdownEditSubclassProc,
                      COUNTDOWN_EDIT_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndCancel, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndStart, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);
    SetWindowSubclass(state->hwndClose, CountdownButtonSubclassProc,
                      COUNTDOWN_BUTTON_SUBCLASS_ID, (DWORD_PTR)state);

    CountdownBuildFonts(state);
    CountdownUpdateTextMetrics(hwnd, state);
    CountdownEnsureContentWidth(hwnd, state);
    CountdownLayout(hwnd, state);
    CountdownPopulateInitialInput(state);
    CountdownUpdatePreview(hwnd, state);

    SetWindowTextW(hwnd, state->title);
    DialogInstanceType instanceType =
        DialogInput_GetInstanceType(state->input.dialogId);
    Dialog_InitializeInstance(instanceType, hwnd);
    if (state->input.dialogId == CLOCK_IDD_DIALOG1) {
        g_hwndInputDialog = hwnd;
    }

    SetFocus(state->hwndEdit);
    PostMessageW(hwnd, WM_APP + 200, 0, 0);
    if (!SetTimer(hwnd, INPUT_FOCUS_TIMER_ID,
                  INPUT_FOCUS_TIMER_DELAY_MS, NULL)) {
        LOG_WARNING("CountdownDialog: failed to start focus timer (error=%lu)",
                    GetLastError());
    }
    return TRUE;
}

HWND CreateCustomTimeInputDialog(HWND hwndParent, DWORD dialogId,
                                 int pomodoroTimeIndex) {
    if (!CountdownRegisterWindowClass()) {
        return NULL;
    }

    CountdownInputState input = {dialogId, pomodoroTimeIndex};

    UINT dpi = CountdownGetDpi(hwndParent);
    int width = MulDiv(COUNTDOWN_BASE_WIDTH, (int)dpi, 96);
    int height = MulDiv(COUNTDOWN_BASE_HEIGHT, (int)dpi, 96);
    HMONITOR monitor = MonitorFromWindow(hwndParent, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {0};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor && GetMonitorInfoW(monitor, &monitorInfo)) {
        int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        int availableWidth = workWidth - MulDiv(24, (int)dpi, 96);
        int availableHeight = workHeight - MulDiv(24, (int)dpi, 96);
        int minimumWidth = MulDiv(360, (int)dpi, 96);
        int minimumHeight = MulDiv(330, (int)dpi, 96);
        if (availableWidth < MulDiv(260, (int)dpi, 96)) {
            availableWidth = MulDiv(260, (int)dpi, 96);
        }
        if (availableHeight < MulDiv(280, (int)dpi, 96)) {
            availableHeight = MulDiv(280, (int)dpi, 96);
        }
        if (width < minimumWidth) width = minimumWidth;
        if (height < minimumHeight) height = minimumHeight;
        if (width > availableWidth) width = availableWidth;
        if (height > availableHeight) height = availableHeight;

        int x = monitorInfo.rcWork.left +
                ((monitorInfo.rcWork.right - monitorInfo.rcWork.left) - width) / 2;
        int y = monitorInfo.rcWork.top +
                ((monitorInfo.rcWork.bottom - monitorInfo.rcWork.top) - height) / 2;
        return CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
            COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
            WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            x, y, width, height, hwndParent, NULL,
            GetModuleHandleW(NULL), &input);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    return CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        COUNTDOWN_WINDOW_CLASS_NAME, L"Set Countdown",
        WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, width, height, hwndParent, NULL,
        GetModuleHandleW(NULL), &input);
}
