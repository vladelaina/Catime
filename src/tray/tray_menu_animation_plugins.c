/**
 * @file tray_menu_animation_plugins.c
 * @brief Animation and plugin submenu builders.
 */

#include "tray_menu_submenus_internal.h"
#include "tray/tray_menu_pagination.h"

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
        TrayMenuPaginationRange customAnimationItems = {0};
        BOOL hasCustomAnimations = BuildAnimationMenu(
            hAnimMenu, currentAnim, &customAnimationItems);

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
        AppendMenuW(hAnimMenu, MF_STRING, CLOCK_IDM_ANIMATIONS_GET_MORE,
                    GetLocalizedString(NULL, L"Get More"));
        AppendMenuW(hAnimMenu, MF_STRING, CLOCK_IDM_ANIMATIONS_OPEN_DIR, GetLocalizedString(NULL, L"Open animations folder"));
        if (customAnimationItems.itemCount > 0 &&
            !TrayMenuPagination_ApplyRangeForCurrentMonitor(
                hAnimMenu, &customAnimationItems,
                GetLocalizedString(NULL, L"More"))) {
            LOG_WARNING("Failed to paginate the animation menu");
        }
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

    TrayMenuPaginationRange pluginItems = {0};
    if (pluginCount == 0) {
        AppendMenuW(hPluginsMenu, MF_STRING | MF_GRAYED, 0,
                    GetLocalizedString(NULL, L"No plugins found"));
    } else {
        BOOL rangeStarted = TrayMenuPagination_BeginRange(
            hPluginsMenu, &pluginItems);
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
        if (!rangeStarted ||
            !TrayMenuPagination_EndRange(hPluginsMenu, &pluginItems)) {
            pluginItems.itemCount = 0;
            LOG_WARNING("Failed to capture plugin menu items");
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

    if (pluginItems.itemCount > 0 &&
        !TrayMenuPagination_ApplyRangeForCurrentMonitor(
            hPluginsMenu, &pluginItems,
            GetLocalizedString(NULL, L"More"))) {
        LOG_WARNING("Failed to paginate the plugin menu");
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPluginsMenu,
                     GetLocalizedString(NULL, L"Plugins"))) {
        DestroyMenu(hPluginsMenu);
    }
}
