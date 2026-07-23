/**
 * @file tray_menu_format_color.c
 * @brief Format, color, and style submenu builders.
 */

#include "tray_menu_submenus_internal.h"

typedef struct {
    UINT id;
    char color[COLOR_HEX_BUFFER];
    BOOL valid;
} ColorMenuIdMapEntry;

static ColorMenuIdMapEntry s_colorMenuIdMap[MAX_COLOR_OPTIONS];

static void ResetColorMenuIdMap(void) {
    ZeroMemory(s_colorMenuIdMap, sizeof(s_colorMenuIdMap));
}

static BOOL RememberColorMenuId(UINT id, const char* color) {
    if (!color || id < CMD_COLOR_OPTIONS_BASE) {
        return FALSE;
    }

    UINT index = id - CMD_COLOR_OPTIONS_BASE;
    if (index >= MAX_COLOR_OPTIONS) {
        return FALSE;
    }

    s_colorMenuIdMap[index].id = id;
    strncpy_s(s_colorMenuIdMap[index].color,
              sizeof(s_colorMenuIdMap[index].color),
              color,
              _TRUNCATE);
    s_colorMenuIdMap[index].valid = s_colorMenuIdMap[index].color[0] != '\0';
    return s_colorMenuIdMap[index].valid;
}

BOOL GetColorMenuColorFromId(UINT id, char* outColor, size_t outSize) {
    if (id < CMD_COLOR_OPTIONS_BASE) {
        return FALSE;
    }

    UINT index = id - CMD_COLOR_OPTIONS_BASE;
    if (index >= MAX_COLOR_OPTIONS ||
        !s_colorMenuIdMap[index].valid ||
        s_colorMenuIdMap[index].id != id) {
        return FALSE;
    }

    if (outColor && outSize > 0) {
        strncpy_s(outColor, outSize, s_colorMenuIdMap[index].color, _TRUNCATE);
    }
    return TRUE;
}

void BuildFormatSubmenu(HMENU hMenu) {
    HMENU hFormatMenu = CreatePopupMenu();
    if (!hFormatMenu) return;

    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_DEFAULT ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_DEFAULT,
                GetLocalizedString(NULL, L"Default Format"));

    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_ZERO_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_ZERO_PADDED,
                GetLocalizedString(NULL, L"09:59 Format"));

    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.format == TIME_FORMAT_FULL_PADDED ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_FULL_PADDED,
                GetLocalizedString(NULL, L"00:09:59 Format"));

    AppendMenuW(hFormatMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hFormatMenu, MF_STRING | (g_AppConfig.display.time_format.show_milliseconds ? MF_CHECKED : MF_UNCHECKED),
                CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS,
                GetLocalizedString(NULL, L"Show Milliseconds"));

    if (CLOCK_SHOW_CURRENT_TIME) {
        AppendMenuW(hFormatMenu, MF_STRING | (CLOCK_SHOW_SECONDS ? MF_CHECKED : MF_UNCHECKED),
                    CLOCK_IDM_SHOW_SECONDS,
                    GetLocalizedString(NULL, L"Show Seconds"));
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFormatMenu,
                     GetLocalizedString(NULL, L"Format"))) {
        DestroyMenu(hFormatMenu);
    }
}

/**
 * @brief Build color submenu
 * @param hMenu Parent menu handle
 */
void BuildColorSubmenu(HMENU hMenu) {
    ResetColorMenuIdMap();

    HMENU hColorSubMenu = CreatePopupMenu();
    if (!hColorSubMenu) return;

    size_t colorCount = COLOR_OPTIONS_COUNT;
    if (colorCount > MAX_COLOR_OPTIONS) {
        colorCount = MAX_COLOR_OPTIONS;
    }

    for (size_t i = 0; i < colorCount; i++) {
        const char* hexColor = COLOR_OPTIONS[i].hexColor;

        /* Display as sequence number for easier selection */
        wchar_t hexColorW[32];
        _snwprintf_s(hexColorW, 32, _TRUNCATE, L"%u", (unsigned int)(i + 1));

        MENUITEMINFO mii = {0};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_ID | MIIM_STATE | MIIM_FTYPE;
        mii.fType = MFT_STRING | MFT_OWNERDRAW;
        mii.fState = strcmp(CLOCK_TEXT_COLOR, hexColor) == 0 ? MFS_CHECKED : MFS_UNCHECKED;
        mii.wID = CMD_COLOR_OPTIONS_BASE + (UINT)i;
        mii.dwTypeData = hexColorW;

        if (InsertMenuItemW(hColorSubMenu, (UINT)i, TRUE, &mii)) {
            RememberColorMenuId(mii.wID, hexColor);
        }
    }
    AppendMenuW(hColorSubMenu, MF_SEPARATOR, 0, NULL);

    HMENU hCustomizeMenu = CreatePopupMenu();
    if (hCustomizeMenu) {
        AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_VALUE,
                    GetLocalizedString(NULL, L"Color Value"));
        AppendMenuW(hCustomizeMenu, MF_STRING, CLOCK_IDC_COLOR_PANEL,
                    GetLocalizedString(NULL, L"Color Panel"));

        if (!AppendMenuW(hColorSubMenu, MF_POPUP, (UINT_PTR)hCustomizeMenu,
                         GetLocalizedString(NULL, L"Customize"))) {
            DestroyMenu(hCustomizeMenu);
        }
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hColorSubMenu,
                     GetLocalizedString(NULL, L"Color"))) {
        DestroyMenu(hColorSubMenu);
    }
}

/**
 * @brief Build style/appearance submenu
 * @param hMenu Parent menu handle
 */
void BuildStyleSubmenu(HMENU hMenu) {
    HMENU hStyleMenu = CreatePopupMenu();
    if (!hStyleMenu) return;

    for (size_t i = 0; i < TextEffect_GetCount(); ++i) {
        const TextEffectDefinition* effect = TextEffect_GetByIndex(i);
        if (!effect) continue;

        AppendMenuW(hStyleMenu,
                    MF_STRING | (CLOCK_TEXT_EFFECT == effect->type ? MF_CHECKED : MF_UNCHECKED),
                    effect->menuId,
                    effect->menuLabelKey);
    }

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hStyleMenu,
                     GetLocalizedString(NULL, L"Style"))) {
        DestroyMenu(hStyleMenu);
    }
}

/**
 * @brief Build animation/tray icon submenu
 * @param hMenu Parent menu handle
 */
