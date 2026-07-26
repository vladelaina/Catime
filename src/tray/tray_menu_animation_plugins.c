/**
 * @file tray_menu_animation_plugins.c
 * @brief Animation and plugin submenu builders.
 */

#include "tray_menu_submenus_internal.h"
#include "taskbar_monitor.h"

static BOOL IsCustomTextDisplaySourceActive(void) {
    if (!PluginData_IsActive()) {
        return FALSE;
    }

    wchar_t sourcePath[MAX_PATH] = {0};
    if (!PluginData_GetDisplaySourcePath(sourcePath, MAX_PATH)) {
        return FALSE;
    }

    const wchar_t* fileName = wcsrchr(sourcePath, L'\\');
    const wchar_t* forwardName = wcsrchr(sourcePath, L'/');
    if (!fileName || (forwardName && forwardName > fileName)) {
        fileName = forwardName;
    }
    fileName = fileName ? fileName + 1 : sourcePath;

    return _wcsicmp(fileName, L"custom_display.txt") == 0;
}

void BuildAnimationSubmenu(HMENU hMenu) {
    HMENU hAnimMenu = CreatePopupMenu();
    if (!hAnimMenu) return;
    {
        const char* currentAnim = GetCurrentAnimationName();
        BOOL hasCustomAnimations = BuildAnimationMenu(hAnimMenu, currentAnim);

        if (hasCustomAnimations) {
            AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);

            HMENU hAnimSpeedMenu = CreatePopupMenu();
            if (hAnimSpeedMenu) {
                AnimationSpeedMetric currentMetric = GetAnimationSpeedMetric();
                AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_ORIGINAL ? MF_CHECKED : MF_UNCHECKED),
                            CLOCK_IDM_ANIM_SPEED_ORIGINAL, GetLocalizedString(NULL, L"Original Speed"));
                AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_MEMORY ? MF_CHECKED : MF_UNCHECKED),
                            CLOCK_IDM_ANIM_SPEED_MEMORY, GetLocalizedString(NULL, L"By Memory Usage"));
                AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_CPU ? MF_CHECKED : MF_UNCHECKED),
                            CLOCK_IDM_ANIM_SPEED_CPU, GetLocalizedString(NULL, L"By CPU Usage"));
                AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_TIMER ? MF_CHECKED : MF_UNCHECKED),
                            CLOCK_IDM_ANIM_SPEED_TIMER, GetLocalizedString(NULL, L"By Countdown Progress"));
                AppendMenuW(hAnimSpeedMenu, MF_SEPARATOR, 0, NULL);
                wchar_t fixedSpeedLabel[160] = {0};
                _snwprintf_s(fixedSpeedLabel, _countof(fixedSpeedLabel), _TRUNCATE,
                             L"%ls (%.4gx)",
                             GetLocalizedString(NULL, L"Set Fixed Speed..."),
                             GetAnimationFixedSpeedMultiplier());
                AppendMenuW(hAnimSpeedMenu, MF_STRING | (currentMetric == ANIMATION_SPEED_FIXED ? MF_CHECKED : MF_UNCHECKED),
                            CLOCK_IDM_ANIM_SPEED_FIXED, fixedSpeedLabel);
                if (!AppendMenuW(hAnimMenu, MF_POPUP, (UINT_PTR)hAnimSpeedMenu,
                                 GetLocalizedString(NULL, L"Animation Speed Metric"))) {
                    DestroyMenu(hAnimSpeedMenu);
                }
            }
        }

        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);
        HMENU hTaskbarMonitorMenu = CreatePopupMenu();
        if (hTaskbarMonitorMenu) {
            AppendMenuW(
                hTaskbarMonitorMenu,
                MF_STRING | (TaskbarMonitor_IsOptionEnabled(
                    TASKBAR_MONITOR_OPTION_NETWORK)
                        ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TASKBAR_MONITOR_NETWORK,
                GetLocalizedString(NULL, L"Network"));
            AppendMenuW(
                hTaskbarMonitorMenu,
                MF_STRING | (TaskbarMonitor_IsOptionEnabled(
                    TASKBAR_MONITOR_OPTION_CPU_MEMORY)
                        ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TASKBAR_MONITOR_CPU_MEMORY,
                GetLocalizedString(NULL, L"CPU and Memory"));
            if (!AppendMenuW(
                    hAnimMenu, MF_POPUP, (UINT_PTR)hTaskbarMonitorMenu,
                    GetLocalizedString(NULL, L"Taskbar Monitor"))) {
                DestroyMenu(hTaskbarMonitorMenu);
            }
        }
        AppendMenuW(hAnimMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hAnimMenu, MF_STRING, CLOCK_IDM_ANIMATIONS_OPEN_DIR, GetLocalizedString(NULL, L"Open animations folder"));
    }
    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAnimMenu, GetLocalizedString(NULL, L"Tray Icon"))) {
        DestroyMenu(hAnimMenu);
    }
}

/**
 * @brief Build plugins submenu
 * @param hMenu Parent menu handle
 */
void BuildPluginsSubmenu(HMENU hMenu) {
    HMENU hPluginsMenu = CreatePopupMenu();
    if (!hPluginsMenu) return;

    PluginManager_RequestScanAsync();
    int pluginCount = PluginManager_GetPluginCount();

    int activePluginIndex = PluginManager_GetActivePluginIndex();
    if (activePluginIndex >= 0 &&
        !PluginManager_IsPluginRunning(activePluginIndex)) {
        activePluginIndex = -1;
    }

    if (pluginCount == 0) {
        AppendMenuW(hPluginsMenu, MF_STRING | MF_GRAYED, 0,
                    GetLocalizedString(NULL, L"No plugins found"));
    } else {
        for (int i = 0; i < pluginCount; i++) {
            PluginInfo plugin;
            if (PluginManager_CopyPlugin(i, &plugin)) {
                // Check if this is the active plugin (by user action, not just process state)
                UINT flags = MF_STRING;
                if (i == activePluginIndex) {
                    flags |= MF_CHECKED;
                }

                /* plugin->displayName is already wchar_t, use directly */
                AppendMenuW(hPluginsMenu, flags, CLOCK_IDM_PLUGINS_BASE + i, plugin.displayName);
            }
        }
    }

    AppendMenuW(hPluginsMenu, MF_SEPARATOR, 0, NULL);

    // Custom text display - edits and previews custom_display.txt without running a plugin
    {
        UINT flags = MF_STRING;
        if (activePluginIndex < 0 && IsCustomTextDisplaySourceActive()) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(hPluginsMenu, flags, CLOCK_IDM_CUSTOM_TEXT_DISPLAY,
                    GetLocalizedString(NULL, L"Custom Text Display"));
    }
    AppendMenuW(hPluginsMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hPluginsMenu, MF_STRING, CLOCK_IDM_PLUGINS_OPEN_DIR,
                GetLocalizedString(NULL, L"Open plugins folder"));

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPluginsMenu,
                     GetLocalizedString(NULL, L"Plugins"))) {
        DestroyMenu(hPluginsMenu);
    }
}
