/**
 * @file tray_menu_pagination.h
 * @brief Height-aware pagination for contiguous native popup-menu ranges.
 */

#ifndef CATIME_TRAY_MENU_PAGINATION_H
#define CATIME_TRAY_MENU_PAGINATION_H

#include <windows.h>

typedef struct {
    int firstItem;
    int itemCount;
} TrayMenuPaginationRange;

/**
 * @brief Estimate how many native menu rows fit in two thirds of the monitor.
 */
int TrayMenuPagination_GetScreenItemLimit(void);

/**
 * @brief Mark the current end of a menu as the start of an appended range.
 */
BOOL TrayMenuPagination_BeginRange(
    HMENU menu, TrayMenuPaginationRange* range);

/**
 * @brief Finish an appended range at the menu's current end.
 */
BOOL TrayMenuPagination_EndRange(
    HMENU menu, TrayMenuPaginationRange* range);

/**
 * @brief Paginate a previously captured contiguous menu-item range.
 *
 * Existing child menus in the range are paginated recursively. Items beyond
 * the first page are moved into a chain of localized More submenus. Items
 * already following the range remain on the first page and count toward its
 * height limit.
 *
 * @param menu Menu containing the captured range
 * @param range Range produced by BeginRange and EndRange
 * @param pageItemLimit Maximum rows on each page
 * @param moreLabel Localized label for continuation submenus
 */
BOOL TrayMenuPagination_ApplyRange(
    HMENU menu, const TrayMenuPaginationRange* range,
    int pageItemLimit, const wchar_t* moreLabel);

/**
 * @brief Paginate using the two-thirds item limit for the current monitor.
 */
BOOL TrayMenuPagination_ApplyRangeForCurrentMonitor(
    HMENU menu, const TrayMenuPaginationRange* range,
    const wchar_t* moreLabel);

#endif /* CATIME_TRAY_MENU_PAGINATION_H */
