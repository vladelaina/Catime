/** @file window_hotkey_registration.c @brief Hotkey configuration and registration. */

#include "window_procedure/window_hotkeys.h"
#include "config.h"
#include "hotkey.h"
#include "log.h"
#include <stdio.h>

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT 0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT 0x04
#endif

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

#define HOTKEY_REGISTRATION_RETRY_INTERVAL_MS 3000u
#define HOTKEY_OWNER_MUTEX_NAME \
    L"Local\\Vladelaina.Catime.GlobalHotkeyOwner"

typedef struct { int id; WORD value; const char* configKey; } HotkeyConfig;
static HotkeyConfig g_hotkeyConfigs[] = {
#define X(name, key) {HOTKEY_ID_##name, 0, key},
    HOTKEY_REGISTRY
#undef X
};
static HWND g_registeredHotkeyHwnd;
static BOOL g_hotkeyRegistrationIncomplete = FALSE;
static HANDLE g_hotkeyOwnerMutex = NULL;
static BOOL g_ownsGlobalHotkeys = FALSE;
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static BOOL TryAcquireHotkeyOwnership(void) {
    if (g_ownsGlobalHotkeys && g_hotkeyOwnerMutex) return TRUE;

    HANDLE mutex = CreateMutexW(NULL, FALSE, HOTKEY_OWNER_MUTEX_NAME);
    if (!mutex) {
        LOG_WARNING("Failed to create global hotkey ownership mutex (error=%lu)",
                    GetLastError());
        return FALSE;
    }

    DWORD waitResult = WaitForSingleObject(mutex, 0);
    if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
        if (waitResult != WAIT_TIMEOUT) {
            LOG_WARNING("Failed to acquire global hotkey ownership (result=%lu, error=%lu)",
                        waitResult, GetLastError());
        }
        CloseHandle(mutex);
        return FALSE;
    }

    g_hotkeyOwnerMutex = mutex;
    g_ownsGlobalHotkeys = TRUE;
    return TRUE;
}

static void ReleaseHotkeyOwnership(void) {
    HANDLE mutex = g_hotkeyOwnerMutex;
    BOOL ownsHotkeys = g_ownsGlobalHotkeys;
    g_hotkeyOwnerMutex = NULL;
    g_ownsGlobalHotkeys = FALSE;
    if (!mutex) return;
    if (ownsHotkeys && !ReleaseMutex(mutex)) {
        LOG_WARNING("Failed to release global hotkey ownership (error=%lu)",
                    GetLastError());
    }
    CloseHandle(mutex);
}

static void LoadValues(const char* path, WORD* values) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i) {
        char text[64];
        ReadIniString(INI_SECTION_HOTKEYS, g_hotkeyConfigs[i].configKey,
                      "None", text, sizeof(text), path);
        values[i] = StringToHotkey(text);
    }
}

static BOOL SanitizeValues(WORD* values) {
    BOOL changed = FALSE;
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i) {
        WORD normalized = NormalizeHotkeyValue(values[i]);
        if (normalized != values[i]) { values[i] = normalized; changed = TRUE; }
        if (!IsHotkeyValueAllowed(values[i])) { values[i] = 0; changed = TRUE; continue; }
        for (size_t j = 0; j < i && values[i] != 0; ++j) {
            if (values[j] == values[i]) { values[i] = 0; changed = TRUE; }
        }
    }
    return changed;
}

static BOOL RegisterOne(HWND hwnd, HotkeyConfig* config) {
    config->value = NormalizeHotkeyValue(config->value);
    if (!config->value) return FALSE;
    BYTE vk = LOBYTE(config->value), mod = HIBYTE(config->value);
    UINT flags = ((mod & HOTKEYF_ALT) ? MOD_ALT : 0) |
                 ((mod & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) |
                 ((mod & HOTKEYF_SHIFT) ? MOD_SHIFT : 0);
    if (RegisterHotKey(hwnd, config->id, flags, vk)) return TRUE;
    char text[64];
    HotkeyToString(config->value, text, sizeof(text));
    LOG_WARNING("Hotkey registration failed [%s]: %s", config->configKey, text);
    return FALSE;
}

static BOOL ValuesMatch(const WORD* values) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i)
        if (g_hotkeyConfigs[i].value != values[i]) return FALSE;
    return TRUE;
}

static BOOL AnyValue(const WORD* values) {
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i)
        if (values[i]) return TRUE;
    return FALSE;
}

BOOL RegisterGlobalHotkeys(HWND hwnd) {
    char path[MAX_PATH];
    GetConfigPath(path, MAX_PATH);
    WORD desired[ARRAY_SIZE(g_hotkeyConfigs)] = {0};
    LoadValues(path, desired);
    BOOL changed = SanitizeValues(desired);
    if (!changed && g_ownsGlobalHotkeys &&
        g_registeredHotkeyHwnd == hwnd &&
        !g_hotkeyRegistrationIncomplete && ValuesMatch(desired))
        return AnyValue(desired);

    if (g_registeredHotkeyHwnd) UnregisterGlobalHotkeys(g_registeredHotkeyHwnd);
    else UnregisterGlobalHotkeys(hwnd);
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i) {
        g_hotkeyConfigs[i].value = desired[i];
    }
    if (changed) {
        char values[ARRAY_SIZE(g_hotkeyConfigs)][64];
        IniKeyValue updates[ARRAY_SIZE(g_hotkeyConfigs)];
        for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i) {
            HotkeyToString(g_hotkeyConfigs[i].value, values[i], sizeof(values[i]));
            updates[i] = (IniKeyValue){INI_SECTION_HOTKEYS,
                                       g_hotkeyConfigs[i].configKey, values[i]};
        }
        if (!WriteIniMultipleAtomic(path, updates, ARRAY_SIZE(updates)))
            LOG_WARNING("Failed to persist cleared hotkeys");
    }

    if (!AnyValue(desired)) {
        ReleaseHotkeyOwnership();
        return FALSE;
    }
    if (!TryAcquireHotkeyOwnership()) {
        g_registeredHotkeyHwnd = hwnd;
        g_hotkeyRegistrationIncomplete = TRUE;
        if (!SetTimer(hwnd, HOTKEY_REGISTRATION_RETRY_TIMER_ID,
                      HOTKEY_REGISTRATION_RETRY_INTERVAL_MS, NULL)) {
            LOG_WARNING("Failed to schedule global hotkey ownership retry (error=%lu)",
                        GetLastError());
        }
        return FALSE;
    }

    BOOL any = FALSE;
    int failures = 0;
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i) {
        WORD old = desired[i];
        if (RegisterOne(hwnd, &g_hotkeyConfigs[i])) any = TRUE;
        else if (old) { ++failures; }
    }
    g_registeredHotkeyHwnd = hwnd;
    g_hotkeyRegistrationIncomplete = failures > 0;
    if (g_hotkeyRegistrationIncomplete) {
        if (!SetTimer(hwnd, HOTKEY_REGISTRATION_RETRY_TIMER_ID,
                      HOTKEY_REGISTRATION_RETRY_INTERVAL_MS, NULL)) {
            LOG_WARNING("Failed to schedule global hotkey retry (error=%lu)",
                        GetLastError());
        }
    } else {
        KillTimer(hwnd, HOTKEY_REGISTRATION_RETRY_TIMER_ID);
    }
    if (failures) LOG_WARNING("%d global hotkeys could not be registered", failures);
    return any;
}

void UnregisterGlobalHotkeys(HWND hwnd) {
    if (hwnd) KillTimer(hwnd, HOTKEY_REGISTRATION_RETRY_TIMER_ID);
    for (size_t i = 0; i < ARRAY_SIZE(g_hotkeyConfigs); ++i)
        UnregisterHotKey(hwnd, g_hotkeyConfigs[i].id);
    if (!hwnd || g_registeredHotkeyHwnd == hwnd) {
        g_registeredHotkeyHwnd = NULL;
        g_hotkeyRegistrationIncomplete = FALSE;
    }
    if (!hwnd) ReleaseHotkeyOwnership();
}

void ShutdownGlobalHotkeys(HWND hwnd) {
    UnregisterGlobalHotkeys(hwnd);
    ReleaseHotkeyOwnership();
}
