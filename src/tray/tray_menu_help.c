/**
 * @file tray_menu_help.c
 * @brief Help, support, and update submenu builder.
 */

#include "tray_menu_submenus_internal.h"

static void FormatSupportLabel(wchar_t* buffer, size_t bufferCount,
                               const wchar_t* textKey, const wchar_t* face) {
    if (!buffer || bufferCount == 0) return;

    _snwprintf_s(buffer, bufferCount, _TRUNCATE, L"%s %s",
                 GetLocalizedString(NULL, textKey), face);
}

static int FindDirectMenuItemByCommand(HMENU hMenu, UINT commandId) {
    int itemCount = GetMenuItemCount(hMenu);
    for (int i = 0; i < itemCount; ++i) {
        if (GetMenuItemID(hMenu, i) == commandId) {
            return i;
        }
    }
    return -1;
}

BOOL UpdateHelpSubmenuSupportFace(HMENU hMenu) {
    if (!hMenu) return FALSE;

    int supportItemPos = FindDirectMenuItemByCommand(hMenu, CLOCK_IDM_SUPPORT);
    if (supportItemPos < 0) {
        return FALSE;
    }

    static BOOL s_offerMilkTea = FALSE;
    const wchar_t* supportTextKey = s_offerMilkTea
        ? L"Support Catime Milk Tea"
        : L"Support Catime";
    const wchar_t* supportFace = s_offerMilkTea ? L"ovO" : L"Ovo";
    MENUITEMINFOW menuItemInfo = {0};
    wchar_t supportLabel[96];

    s_offerMilkTea = !s_offerMilkTea;
    FormatSupportLabel(supportLabel, _countof(supportLabel),
                       supportTextKey, supportFace);

    menuItemInfo.cbSize = sizeof(menuItemInfo);
    menuItemInfo.fMask = MIIM_STRING;
    menuItemInfo.dwTypeData = supportLabel;
    SetMenuItemInfoW(hMenu, (UINT)supportItemPos, TRUE, &menuItemInfo);

    return TRUE;
}

void BuildHelpSubmenu(HMENU hMenu) {
    HMENU hAboutMenu = CreatePopupMenu();
    if (!hAboutMenu) return;
    wchar_t supportLabel[96];

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_ABOUT, GetLocalizedString(NULL, L"About"));
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    FormatSupportLabel(supportLabel, _countof(supportLabel),
                       L"Support Catime", L"Ovo");
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_SUPPORT, supportLabel);
    HBITMAP hSupportHeart = TraySubmenu_GetSupportHeartBitmap();
    MENUITEMINFOW supportMii = {0};
    supportMii.cbSize = sizeof(supportMii);
    supportMii.fMask = MIIM_BITMAP;
    supportMii.hbmpItem = hSupportHeart;
    SetMenuItemInfoW(hAboutMenu, CLOCK_IDM_SUPPORT, FALSE, &supportMii);

    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_VLAINA,
                GetLocalizedString(NULL, L"Need to take notes? Try Vlaina"));
    HBITMAP hVlainaCheck = TraySubmenu_GetVlainaCheckBitmap();
    MENUITEMINFOW vlainaMii = {0};
    vlainaMii.cbSize = sizeof(vlainaMii);
    vlainaMii.fMask = MIIM_BITMAP;
    vlainaMii.hbmpItem = hVlainaCheck;
    SetMenuItemInfoW(hAboutMenu, CLOCK_IDM_VLAINA, FALSE, &vlainaMii);
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_FEEDBACK, GetLocalizedString(NULL, L"Feedback"));
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_HELP, GetLocalizedString(NULL, L"User Guide"));

    char newVersion[32] = {0};
    BOOL isNewVersionAvailable = GetNewVersionStatus(newVersion, sizeof(newVersion));

    if (isNewVersionAvailable) {
        wchar_t updateText[64];
        wchar_t wNewVersion[32] = L"?";
        wchar_t wCurrentVersion[32] = L"?";

        MultiByteToWideChar(CP_UTF8, 0, newVersion, -1, wNewVersion, 32);
        MultiByteToWideChar(CP_UTF8, 0, CATIME_VERSION, -1, wCurrentVersion, 32);

        _snwprintf_s(updateText, 64, _TRUNCATE, L"v%s -> v%s", wCurrentVersion, wNewVersion);
        AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, updateText);

        HBITMAP hDot = TraySubmenu_GetUpdateDotBitmap();
        MENUITEMINFOW mii = {0};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_BITMAP;
        mii.hbmpItem = hDot;
        SetMenuItemInfoW(hAboutMenu, CLOCK_IDM_CHECK_UPDATE, FALSE, &mii);
    } else {
        AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_CHECK_UPDATE, GetLocalizedString(NULL, L"Check for Updates"));
    }

    HMENU hLangMenu = CreatePopupMenu();
    if (hLangMenu) {
#define X(Enum, Code, Native, Eng, ConfigKey, ResId, MenuId, ...) \
        AppendMenuW(hLangMenu, MF_STRING | (CURRENT_LANGUAGE == Enum ? MF_CHECKED : MF_UNCHECKED), \
                    MenuId, Native);
#include "language_def.h"
        LANGUAGE_LIST
#undef X

        if (!AppendMenuW(hAboutMenu, MF_POPUP, (UINT_PTR)hLangMenu, L"Language")) {
            DestroyMenu(hLangMenu);
        }
    }
    AppendMenuW(hAboutMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_RESET_POSITION, GetLocalizedString(NULL, L"Reset Position"));
    AppendMenuW(hAboutMenu, MF_STRING, CLOCK_IDM_RESET_ALL, GetLocalizedString(NULL, L"Reset"));

    if (!AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAboutMenu, GetLocalizedString(NULL, L"Help"))) {
        DestroyMenu(hAboutMenu);
        return;
    }

    if (isNewVersionAvailable) {
        int count = GetMenuItemCount(hMenu);
        if (count > 0) {
            HBITMAP hDot = TraySubmenu_GetUpdateDotBitmap();
            MENUITEMINFOW mii = {0};
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_BITMAP;
            mii.hbmpItem = hDot;
            SetMenuItemInfoW(hMenu, count - 1, TRUE, &mii);
        }
    }
}
