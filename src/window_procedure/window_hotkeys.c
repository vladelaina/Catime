/**
 * @file window_hotkeys.c
 * @brief Global hotkey registration and dispatch implementation
 */

#include "window_procedure/window_hotkeys.h"
#include "window_procedure/window_utils.h"
#include "config.h"
#include "hotkey.h"
#include "timer/timer_events.h"
#include "tray/tray_events.h"
#include "window.h"
#include "dialog/dialog_procedure.h"
#include "notification.h"
#include "audio_player.h"
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
extern BOOL WriteConfigShowMilliseconds(BOOL showMilliseconds);

/* ============================================================================
 * Hotkey Actions
 * ============================================================================ */

typedef void (*HotkeyAction)(HWND);

static void HotkeyToggleVisibility(HWND hwnd) {
    ToggleWindowVisibility(hwnd);
}

static void HotkeyRestartTimer(HWND hwnd) {
    RestartCurrentTimer(hwnd);
}

static void HotkeyToggleMilliseconds(HWND hwnd) {
    ToggleMilliseconds(hwnd);
}

static void HotkeyToggleTopmost(HWND hwnd) {
    ToggleTopmost(hwnd);
}

static void HotkeyCustomCountdown(HWND hwnd) {
    if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
        SetForegroundWindow(g_hwndInputDialog);
        return;
    }

    ClearInputBuffer(inputText, sizeof(inputText));

    /* Use modeless dialog */
    ShowCountdownInputDialog(hwnd);
}

static void HotkeyQuickCountdown(HWND hwnd, int index) {
    StartQuickCountdownByIndex(hwnd, index);
}

/* ============================================================================
 * Hotkey Dispatch Table
 * ============================================================================ */

typedef struct {
    int id;
    HotkeyAction action;
    int quickCountdownIndex;
} HotkeyDescriptor;

static void HotkeyPauseResume(HWND hwnd) {
    /* Match tray menu behavior: call TogglePauseResumeTimer directly */
    TogglePauseResumeTimer(hwnd);
}

static const HotkeyDescriptor HOTKEY_DISPATCH_TABLE[] = {
    {HOTKEY_ID_SHOW_TIME, ToggleShowTimeMode, 0},
    {HOTKEY_ID_COUNT_UP, StartCountUp, 0},
    {HOTKEY_ID_COUNTDOWN, StartDefaultCountDown, 0},
    {HOTKEY_ID_QUICK_COUNTDOWN1, NULL, 1},
    {HOTKEY_ID_QUICK_COUNTDOWN2, NULL, 2},
    {HOTKEY_ID_QUICK_COUNTDOWN3, NULL, 3},
    {HOTKEY_ID_POMODORO, StartPomodoroTimer, 0},
    {HOTKEY_ID_TOGGLE_VISIBILITY, HotkeyToggleVisibility, 0},
    {HOTKEY_ID_EDIT_MODE, ToggleEditMode, 0},
    {HOTKEY_ID_PAUSE_RESUME, HotkeyPauseResume, 0},
    {HOTKEY_ID_RESTART_TIMER, HotkeyRestartTimer, 0},
    {HOTKEY_ID_CUSTOM_COUNTDOWN, HotkeyCustomCountdown, 0},
    {HOTKEY_ID_TOGGLE_MILLISECONDS, HotkeyToggleMilliseconds, 0},
    {HOTKEY_ID_TOPMOST, HotkeyToggleTopmost, 0}
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

BOOL DispatchHotkey(HWND hwnd, int hotkeyId) {
    for (size_t i = 0; i < ARRAY_SIZE(HOTKEY_DISPATCH_TABLE); i++) {
        if (HOTKEY_DISPATCH_TABLE[i].id == hotkeyId) {
            const HotkeyDescriptor* descriptor = &HOTKEY_DISPATCH_TABLE[i];

            /* Match tray clicks: any configured hotkey dismisses active audio. */
            StopNotificationSound();

            if (descriptor->quickCountdownIndex > 0) {
                HotkeyQuickCountdown(hwnd, descriptor->quickCountdownIndex);
            } else {
                descriptor->action(hwnd);
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
    X(TOGGLE_MILLISECONDS, "HOTKEY_TOGGLE_MILLISECONDS") \
    X(TOPMOST, "HOTKEY_TOPMOST")

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

static HWND g_registeredHotkeyHwnd = NULL;

static void LoadHotkeyConfigValues(const char* configPath, WORD* values, size_t count) {
    if (!configPath || !values) return;

    for (size_t i = 0; i < count && i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        char hotkeyStr[64];
        ReadIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey,
                      "None", hotkeyStr, sizeof(hotkeyStr), configPath);
        values[i] = StringToHotkey(hotkeyStr);
    }
}

static BOOL HotkeyConfigValuesMatch(const WORD* values, size_t count) {
    if (!values || count != ARRAY_SIZE(g_hotkeyConfigs)) return FALSE;

    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        if (g_hotkeyConfigs[i].value != values[i]) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL HasAnyHotkeyValue(const WORD* values, size_t count) {
    if (!values) return FALSE;

    for (size_t i = 0; i < count; i++) {
        if (values[i] != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL SanitizeLoadedHotkeyValues(WORD* values, size_t count) {
    if (!values) return FALSE;

    BOOL changed = FALSE;
    for (size_t i = 0; i < count; i++) {
        WORD normalized = NormalizeHotkeyValue(values[i]);
        if (values[i] != normalized) {
            values[i] = normalized;
            changed = TRUE;
        }

        if (!IsHotkeyValueAllowed(values[i])) {
            values[i] = 0;
            changed = TRUE;
            continue;
        }

        if (values[i] == 0) {
            continue;
        }

        for (size_t previous = 0; previous < i; previous++) {
            if (values[previous] == values[i]) {
                values[i] = 0;
                changed = TRUE;
                break;
            }
        }
    }

    return changed;
}

static BOOL RegisterSingleHotkey(HWND hwnd, HotkeyConfig* config) {
    config->value = NormalizeHotkeyValue(config->value);
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
    extern void HotkeyToString(WORD hotkey, char* out, size_t size);

    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);

    WORD desiredValues[ARRAY_SIZE(g_hotkeyConfigs)] = {0};
    LoadHotkeyConfigValues(config_path, desiredValues, ARRAY_SIZE(desiredValues));
    BOOL configChanged = SanitizeLoadedHotkeyValues(desiredValues, ARRAY_SIZE(desiredValues));

    if (!configChanged &&
        g_registeredHotkeyHwnd == hwnd &&
        HotkeyConfigValuesMatch(desiredValues, ARRAY_SIZE(desiredValues))) {
        return HasAnyHotkeyValue(desiredValues, ARRAY_SIZE(desiredValues));
    }

    LOG_INFO("Registering global hotkeys...");
    if (g_registeredHotkeyHwnd) {
        UnregisterGlobalHotkeys(g_registeredHotkeyHwnd);
    } else {
        UnregisterGlobalHotkeys(hwnd);
    }

    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        g_hotkeyConfigs[i].value = desiredValues[i];
    }

    BOOL anyRegistered = FALSE;
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
        char hotkeyStrings[ARRAY_SIZE(g_hotkeyConfigs)][64];
        IniKeyValue updates[ARRAY_SIZE(g_hotkeyConfigs)];

        for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
            HotkeyToString(g_hotkeyConfigs[i].value, hotkeyStrings[i],
                           sizeof(hotkeyStrings[i]));
            updates[i].section = INI_SECTION_HOTKEYS;
            updates[i].key = g_hotkeyConfigs[i].configKey;
            updates[i].value = hotkeyStrings[i];
        }
        if (!WriteIniMultipleAtomic(config_path, updates, ARRAY_SIZE(g_hotkeyConfigs))) {
            LOG_WARNING("Failed to persist cleared hotkeys after registration conflicts");
        }
    }

    g_registeredHotkeyHwnd = hwnd;

    LOG_INFO("Hotkey registration completed: %d successful, %d failed\n", successCount, failCount);
    return anyRegistered;
}

void UnregisterGlobalHotkeys(HWND hwnd) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); i++) {
        UnregisterHotKey(hwnd, g_hotkeyConfigs[i].id);
    }
    if (!hwnd || g_registeredHotkeyHwnd == hwnd) {
        g_registeredHotkeyHwnd = NULL;
    }
}
