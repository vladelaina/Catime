/**
 * @file window_hotkeys.c
 * @brief Global hotkey registration and dispatch implementation
 */

#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_utils.h"
#include "config.h"
#include "hotkey.h"
#include "timer/timer_events.h"
#include "window.h"
#include "dialog/dialog_procedure.h"
#include "notification.h"
#include "window_procedure/window_procedure.h"
#include "log.h"
#include "../resource/resource.h"
#include <stdio.h>
#include <string.h>

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT   0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT     0x04
#endif

extern wchar_t inputText[256];
extern HWND g_hwndInputDialog;
extern int time_options_count;
extern int time_options[];
extern void WriteConfigShowMilliseconds(BOOL showMilliseconds);

/* ============================================================================
 * Hotkey Actions
 * ============================================================================ */

typedef void (*HotkeyAction)(HWND);

static void HotkeyToggleVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

static void HotkeyRestartTimer(HWND hwnd) {
    CloseAllNotifications();
    RestartCurrentTimer(hwnd);
}

static void HotkeyToggleMilliseconds(HWND hwnd) {
    WriteConfigShowMilliseconds(!g_AppConfig.display.time_format.show_milliseconds);
    
    /* Reset timer with new interval (10ms for milliseconds, 1000ms without) */
    extern void ResetTimerWithInterval(HWND hwnd);
    ResetTimerWithInterval(hwnd);
    
    InvalidateRect(hwnd, NULL, TRUE);
}

static void HotkeyCustomCountdown(HWND hwnd) {
    if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
        SendMessage(g_hwndInputDialog, WM_CLOSE, 0, 0);
        return;
    }
    
    countdown_message_shown = FALSE;
    ClearInputBuffer(inputText, sizeof(inputText));
    
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), 
                   hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);
    
    if (inputText[0] != L'\0') {
        int total_seconds = 0;
        Utf8String us = ToUtf8(inputText);
        if (ParseInput(us.buf, &total_seconds)) {
            CleanupBeforeTimerAction();
            StartCountdownWithTime(hwnd, total_seconds);
        }
    }
}

static void HotkeyQuickCountdown(HWND hwnd, int index) {
    StartQuickCountdownByIndex(hwnd, index);
}

/* ============================================================================
 * Hotkey Dispatch Table
 * ============================================================================ */

#define QUICK_CD(n) (void(*)(HWND))(void*)(uintptr_t)(n)

typedef struct {
    int id;
    HotkeyAction action;
} HotkeyDescriptor;

static const HotkeyDescriptor HOTKEY_DISPATCH_TABLE[] = {
    {HOTKEY_ID_SHOW_TIME, ToggleShowTimeMode},
    {HOTKEY_ID_COUNT_UP, StartCountUp},
    {HOTKEY_ID_COUNTDOWN, StartDefaultCountDown},
    {HOTKEY_ID_QUICK_COUNTDOWN1, QUICK_CD(1)},
    {HOTKEY_ID_QUICK_COUNTDOWN2, QUICK_CD(2)},
    {HOTKEY_ID_QUICK_COUNTDOWN3, QUICK_CD(3)},
    {HOTKEY_ID_POMODORO, StartPomodoroTimer},
    {HOTKEY_ID_TOGGLE_VISIBILITY, HotkeyToggleVisibility},
    {HOTKEY_ID_EDIT_MODE, ToggleEditMode},
    {HOTKEY_ID_PAUSE_RESUME, TogglePauseResume},
    {HOTKEY_ID_RESTART_TIMER, HotkeyRestartTimer},
    {HOTKEY_ID_CUSTOM_COUNTDOWN, HotkeyCustomCountdown},
    {HOTKEY_ID_TOGGLE_MILLISECONDS, HotkeyToggleMilliseconds}
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

BOOL DispatchHotkey(HWND hwnd, int hotkeyId) {
    for (size_t i = 0; i < ARRAY_SIZE(HOTKEY_DISPATCH_TABLE); i++) {
        if (HOTKEY_DISPATCH_TABLE[i].id == hotkeyId) {
            HotkeyAction action = HOTKEY_DISPATCH_TABLE[i].action;
            
            if (hotkeyId >= HOTKEY_ID_QUICK_COUNTDOWN1 && hotkeyId <= HOTKEY_ID_QUICK_COUNTDOWN3) {
                int index = (int)(uintptr_t)(void*)action;
                HotkeyQuickCountdown(hwnd, index);
            } else {
                action(hwnd);
            }
            return TRUE;
        }
    }
    return FALSE;
}

/* ============================================================================
 * Hotkey Registration System
 * ============================================================================ */

#define HOTKEY_REGISTRY \
    X(SHOW_TIME, "HOTKEY_SHOW_TIME") \
    X(COUNT_UP, "HOTKEY_COUNT_UP") \
    X(COUNTDOWN, "HOTKEY_COUNTDOWN") \
    X(QUICK_COUNTDOWN1, "HOTKEY_QUICK_COUNTDOWN1") \
    X(QUICK_COUNTDOWN2, "HOTKEY_QUICK_COUNTDOWN2") \
    X(QUICK_COUNTDOWN3, "HOTKEY_QUICK_COUNTDOWN3") \
    X(POMODORO, "HOTKEY_POMODORO") \
    X(TOGGLE_VISIBILITY, "HOTKEY_TOGGLE_VISIBILITY") \
    X(EDIT_MODE, "HOTKEY_EDIT_MODE") \
    X(PAUSE_RESUME, "HOTKEY_PAUSE_RESUME") \
    X(RESTART_TIMER, "HOTKEY_RESTART_TIMER") \
    X(CUSTOM_COUNTDOWN, "HOTKEY_CUSTOM_COUNTDOWN") \
    X(TOGGLE_MILLISECONDS, "HOTKEY_TOGGLE_MILLISECONDS")

typedef struct {
    int id;
    WORD value;
    const char* configKey;
} HotkeyConfig;

static HotkeyConfig g_hotkeyConfigs[] = {
    #define X(name, key) {HOTKEY_ID_##name, 0, key},
    HOTKEY_REGISTRY
    #undef X
};

static BOOL RegisterSingleHotkey(HWND hwnd, HotkeyConfig* config) {
    if (config->value == 0) return FALSE;
    
    BYTE vk = LOBYTE(config->value);
    BYTE mod = HIBYTE(config->value);
    
    UINT fsModifiers = 0;
    if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
    if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
    if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
    
    if (!RegisterHotKey(hwnd, config->id, fsModifiers, vk)) {
        extern void HotkeyToString(WORD hotkey, char* out, size_t size);
        char hotkeyStr[64];
        HotkeyToString(config->value, hotkeyStr, sizeof(hotkeyStr));
        LOG_WARNING("Hotkey registration failed [%s]: %s (may conflict with other applications)",
                   config->configKey, hotkeyStr);
        config->value = 0;
        return FALSE;
    }
    return TRUE;
}

BOOL RegisterGlobalHotkeys(HWND hwnd) {
    extern WORD StringToHotkey(const char* str);
    extern void HotkeyToString(WORD hotkey, char* out, size_t size);
    
    LOG_INFO("Registering global hotkeys...");
    UnregisterGlobalHotkeys(hwnd);
    
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        char hotkeyStr[64];
        ReadIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey, 
                     "None", hotkeyStr, sizeof(hotkeyStr), config_path);
        g_hotkeyConfigs[i].value = StringToHotkey(hotkeyStr);
    }
    
    BOOL anyRegistered = FALSE;
    BOOL configChanged = FALSE;
    int successCount = 0;
    int failCount = 0;
    
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        WORD oldValue = g_hotkeyConfigs[i].value;
        if (RegisterSingleHotkey(hwnd, &g_hotkeyConfigs[i])) {
            anyRegistered = TRUE;
            successCount++;
            char hotkeyStr[64];
            HotkeyToString(g_hotkeyConfigs[i].value, hotkeyStr, sizeof(hotkeyStr));
            LOG_INFO("Hotkey registered successfully [%s]: %s",
                    g_hotkeyConfigs[i].configKey, hotkeyStr);
        } else if (oldValue != 0) {
            configChanged = TRUE;
            failCount++;
        }
    }
    
    if (configChanged) {
        LOG_INFO("Updating configuration to clear failed hotkeys");
        for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
            char hotkeyStr[64];
            HotkeyToString(g_hotkeyConfigs[i].value, hotkeyStr, sizeof(hotkeyStr));
            WriteIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey, 
                          hotkeyStr, config_path);
        }
    }
    
    LOG_INFO("Hotkey registration completed: %d successful, %d failed\n", successCount, failCount);
    return anyRegistered;
}

void UnregisterGlobalHotkeys(HWND hwnd) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        UnregisterHotKey(hwnd, g_hotkeyConfigs[i].id);
    }
}

