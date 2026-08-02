/**
 * @file tray_menu_pagination.c
 * @brief Recursive continuation submenus for tall native popup menus.
 */

#include "tray/tray_menu_pagination.h"

#include "utils/win32_dynamic_loader.h"

#include <stdlib.h>

#define TRAY_MENU_BASE_ROW_HEIGHT 22
#define TRAY_MENU_MIN_PAGE_ITEMS 4
#define TRAY_MENU_MAX_PAGINATION_DEPTH 256

typedef HRESULT(WINAPI* GetDpiForMonitorFunc)(
    HMONITOR monitor, int dpiType, UINT* dpiX, UINT* dpiY);

static UINT GetEffectiveMonitorDpi(HMONITOR monitor) {
    UINT dpiX = 96;
    UINT dpiY = 96;
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        GetDpiForMonitorFunc getDpiForMonitor = NULL;
        CATIME_LOAD_PROC_ADDRESS(shcore, "GetDpiForMonitor", getDpiForMonitor);
        if (!getDpiForMonitor ||
            FAILED(getDpiForMonitor(monitor, 0, &dpiX, &dpiY))) {
            dpiY = 96;
        }
        FreeLibrary(shcore);
    } else {
        HDC dc = GetDC(NULL);
        if (dc) {
            int deviceDpi = GetDeviceCaps(dc, LOGPIXELSY);
            if (deviceDpi > 0) dpiY = (UINT)deviceDpi;
            ReleaseDC(NULL, dc);
        }
    }
    return dpiY > 0 ? dpiY : 96u;
}

int TrayMenuPagination_GetScreenItemLimit(void) {
    POINT cursor = {0};
    if (!GetCursorPos(&cursor)) {
        cursor.x = GetSystemMetrics(SM_CXSCREEN) / 2;
        cursor.y = GetSystemMetrics(SM_CYSCREEN) / 2;
    }

    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {0};
    info.cbSize = sizeof(info);
    int workHeight = GetSystemMetrics(SM_CYSCREEN);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        int monitorWorkHeight = info.rcWork.bottom - info.rcWork.top;
        if (monitorWorkHeight > 0) workHeight = monitorWorkHeight;
    }

    UINT dpi = GetEffectiveMonitorDpi(monitor);
    int rowHeight = GetSystemMetrics(SM_CYMENU);
    int scaledMinimum = MulDiv(TRAY_MENU_BASE_ROW_HEIGHT, (int)dpi, 96);
    if (rowHeight < scaledMinimum) rowHeight = scaledMinimum;
    if (rowHeight < 1) rowHeight = TRAY_MENU_BASE_ROW_HEIGHT;

    int availableHeight = MulDiv(workHeight, 2, 3);
    int borderHeight = GetSystemMetrics(SM_CYEDGE) * 2;
    if (availableHeight > borderHeight) availableHeight -= borderHeight;
    int itemLimit = availableHeight / rowHeight;
    return itemLimit < TRAY_MENU_MIN_PAGE_ITEMS
        ? TRAY_MENU_MIN_PAGE_ITEMS : itemLimit;
}

BOOL TrayMenuPagination_BeginRange(
    HMENU menu, TrayMenuPaginationRange* range) {
    if (!range) return FALSE;
    range->firstItem = -1;
    range->itemCount = 0;
    if (!menu) return FALSE;
    int itemCount = GetMenuItemCount(menu);
    if (itemCount < 0) return FALSE;
    range->firstItem = itemCount;
    return TRUE;
}

BOOL TrayMenuPagination_EndRange(
    HMENU menu, TrayMenuPaginationRange* range) {
    if (!menu || !range || range->firstItem < 0) return FALSE;
    int itemCount = GetMenuItemCount(menu);
    if (itemCount < range->firstItem) return FALSE;
    range->itemCount = itemCount - range->firstItem;
    return TRUE;
}

static BOOL MenuContainsCheckedItem(HMENU menu, int depth) {
    if (!menu || depth > TRAY_MENU_MAX_PAGINATION_DEPTH) return FALSE;
    int count = GetMenuItemCount(menu);
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW item = {0};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_STATE | MIIM_SUBMENU;
        if (!GetMenuItemInfoW(menu, (UINT)i, TRUE, &item)) continue;
        if (item.fState & MFS_CHECKED) return TRUE;
        if (item.hSubMenu && MenuContainsCheckedItem(item.hSubMenu, depth + 1)) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL RangeContainsCheckedItem(
    HMENU menu, int firstItem, int itemCount, int depth) {
    if (!menu || itemCount <= 0 ||
        depth > TRAY_MENU_MAX_PAGINATION_DEPTH) return FALSE;
    for (int i = 0; i < itemCount; ++i) {
        MENUITEMINFOW item = {0};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_STATE | MIIM_SUBMENU;
        if (!GetMenuItemInfoW(
                menu, (UINT)(firstItem + i), TRUE, &item)) continue;
        if (item.fState & MFS_CHECKED) return TRUE;
        if (item.hSubMenu && MenuContainsCheckedItem(item.hSubMenu, depth + 1)) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL MoveMenuItem(HMENU source, int sourcePosition, HMENU destination) {
    MENUITEMINFOW item = {0};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_BITMAP | MIIM_CHECKMARKS | MIIM_DATA | MIIM_FTYPE |
                 MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
    if (!GetMenuItemInfoW(source, (UINT)sourcePosition, TRUE, &item)) {
        return FALSE;
    }

    wchar_t* text = NULL;
    if (!(item.fType & (MFT_SEPARATOR | MFT_OWNERDRAW))) {
        MENUITEMINFOW textItem = {0};
        textItem.cbSize = sizeof(textItem);
        textItem.fMask = MIIM_STRING;
        if (!GetMenuItemInfoW(
                source, (UINT)sourcePosition, TRUE, &textItem)) return FALSE;
        text = (wchar_t*)calloc((size_t)textItem.cch + 1u, sizeof(*text));
        if (!text) return FALSE;
        textItem.dwTypeData = text;
        textItem.cch++;
        if (!GetMenuItemInfoW(
                source, (UINT)sourcePosition, TRUE, &textItem)) {
            free(text);
            return FALSE;
        }
        item.fMask |= MIIM_STRING;
        item.dwTypeData = text;
        item.cch = textItem.cch;
    }

    if (!RemoveMenu(source, (UINT)sourcePosition, MF_BYPOSITION)) {
        free(text);
        return FALSE;
    }
    int destinationPosition = GetMenuItemCount(destination);
    if (destinationPosition < 0 ||
        !InsertMenuItemW(
            destination, (UINT)destinationPosition, TRUE, &item)) {
        if (!InsertMenuItemW(source, (UINT)sourcePosition, TRUE, &item) &&
            item.hSubMenu) {
            DestroyMenu(item.hSubMenu);
        }
        free(text);
        return FALSE;
    }
    free(text);
    return TRUE;
}

static BOOL PaginateMenuRange(
    HMENU menu, int firstRangeItem, int rangeItemCount,
    int reservedTrailingItemCount, int pageItemLimit,
    const wchar_t* moreLabel, int depth) {
    if (depth > TRAY_MENU_MAX_PAGINATION_DEPTH) return FALSE;

    for (int i = 0; i < rangeItemCount; ++i) {
        MENUITEMINFOW item = {0};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_SUBMENU;
        if (!GetMenuItemInfoW(
                menu, (UINT)(firstRangeItem + i), TRUE, &item)) {
            return FALSE;
        }
        if (item.hSubMenu) {
            int childCount = GetMenuItemCount(item.hSubMenu);
            if (childCount < 0 || !PaginateMenuRange(
                    item.hSubMenu, 0, childCount, 0, pageItemLimit,
                    moreLabel, depth + 1)) return FALSE;
        }
    }

    if (firstRangeItem + rangeItemCount +
        reservedTrailingItemCount <= pageItemLimit) return TRUE;

    int firstPageRangeCount = pageItemLimit - firstRangeItem -
                              reservedTrailingItemCount - 1;
    if (rangeItemCount > 0 && firstPageRangeCount < 1) {
        firstPageRangeCount = 1;
    }
    if (firstPageRangeCount >= rangeItemCount) return TRUE;

    int continuationCount = rangeItemCount - firstPageRangeCount;
    int continuationPosition = firstRangeItem + firstPageRangeCount;
    BOOL continuationChecked = RangeContainsCheckedItem(
        menu, continuationPosition, continuationCount, depth);
    HMENU continuation = CreatePopupMenu();
    if (!continuation) return FALSE;

    MENUITEMINFOW moreItem = {0};
    moreItem.cbSize = sizeof(moreItem);
    moreItem.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;
    moreItem.fType = MFT_STRING;
    moreItem.fState = continuationChecked ? MFS_CHECKED : MFS_UNCHECKED;
    moreItem.dwTypeData = (wchar_t*)moreLabel;
    moreItem.hSubMenu = continuation;
    if (!InsertMenuItemW(
            menu, (UINT)continuationPosition, TRUE, &moreItem)) {
        DestroyMenu(continuation);
        return FALSE;
    }

    for (int i = 0; i < continuationCount; ++i) {
        if (!MoveMenuItem(menu, continuationPosition + 1, continuation)) {
            return FALSE;
        }
    }
    return PaginateMenuRange(
        continuation, 0, continuationCount, 0, pageItemLimit,
        moreLabel, depth + 1);
}

BOOL TrayMenuPagination_ApplyRange(
    HMENU menu, const TrayMenuPaginationRange* range,
    int pageItemLimit, const wchar_t* moreLabel) {
    if (!menu || !range || !moreLabel || moreLabel[0] == L'\0' ||
        range->firstItem < 0 || range->itemCount < 0 ||
        pageItemLimit < 2) return FALSE;
    int menuItemCount = GetMenuItemCount(menu);
    if (menuItemCount < 0 || range->firstItem > menuItemCount ||
        range->itemCount > menuItemCount - range->firstItem) return FALSE;
    int existingTrailingItemCount = menuItemCount -
        range->firstItem - range->itemCount;
    return PaginateMenuRange(
        menu, range->firstItem, range->itemCount,
        existingTrailingItemCount, pageItemLimit, moreLabel, 0);
}

BOOL TrayMenuPagination_ApplyRangeForCurrentMonitor(
    HMENU menu, const TrayMenuPaginationRange* range,
    const wchar_t* moreLabel) {
    return TrayMenuPagination_ApplyRange(
        menu, range,
        TrayMenuPagination_GetScreenItemLimit(), moreLabel);
}
