#include "tray/tray_menu_pagination.h"

#include <assert.h>
#include <stdio.h>

static void AppendCommand(HMENU menu, UINT id, BOOL checked) {
    wchar_t label[32];
    _snwprintf_s(label, _countof(label), _TRUNCATE, L"Item %u", id);
    assert(AppendMenuW(
        menu, MF_STRING | (checked ? MF_CHECKED : MF_UNCHECKED),
        id, label));
}

static MENUITEMINFOW GetItemInfo(HMENU menu, int position) {
    MENUITEMINFOW item = {0};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
    assert(GetMenuItemInfoW(menu, (UINT)position, TRUE, &item));
    return item;
}

static void AssertItemId(HMENU menu, int position, UINT expectedId) {
    MENUITEMINFOW item = GetItemInfo(menu, position);
    assert(!item.hSubMenu);
    assert(item.wID == expectedId);
}

static HMENU AssertCheckedMore(HMENU menu, int position) {
    MENUITEMINFOW item = GetItemInfo(menu, position);
    assert(item.hSubMenu != NULL);
    assert(item.fState & MFS_CHECKED);

    wchar_t label[16] = {0};
    MENUITEMINFOW textItem = {0};
    textItem.cbSize = sizeof(textItem);
    textItem.fMask = MIIM_STRING;
    textItem.dwTypeData = label;
    textItem.cch = _countof(label);
    assert(GetMenuItemInfoW(menu, (UINT)position, TRUE, &textItem));
    assert(wcscmp(label, L"More") == 0);
    return item.hSubMenu;
}

static int CollectCommandIds(
    HMENU menu, UINT* output, int outputCapacity, int count) {
    int itemCount = GetMenuItemCount(menu);
    assert(itemCount >= 0);
    for (int i = 0; i < itemCount; ++i) {
        MENUITEMINFOW item = GetItemInfo(menu, i);
        if (item.hSubMenu) {
            count = CollectCommandIds(
                item.hSubMenu, output, outputCapacity, count);
        } else if (!(item.fType & MFT_SEPARATOR)) {
            assert(count < outputCapacity);
            output[count++] = item.wID;
        }
    }
    return count;
}

static void AssertPageLimits(HMENU menu, int limit) {
    int itemCount = GetMenuItemCount(menu);
    assert(itemCount >= 0 && itemCount <= limit);
    for (int i = 0; i < itemCount; ++i) {
        MENUITEMINFOW item = GetItemInfo(menu, i);
        if (item.hSubMenu) AssertPageLimits(item.hSubMenu, limit);
    }
}

static void TestMenuWithinLimitIsUnchanged(void) {
    HMENU menu = CreatePopupMenu();
    assert(menu != NULL);
    AppendCommand(menu, 10, FALSE);
    TrayMenuPaginationRange range = {0};
    assert(TrayMenuPagination_BeginRange(menu, &range));
    AppendCommand(menu, 100, FALSE);
    AppendCommand(menu, 101, FALSE);
    AppendCommand(menu, 102, FALSE);
    assert(TrayMenuPagination_EndRange(menu, &range));
    AppendCommand(menu, 20, FALSE);
    AppendCommand(menu, 21, FALSE);

    assert(TrayMenuPagination_ApplyRange(
        menu, &range, 6, L"More"));
    assert(GetMenuItemCount(menu) == 6);
    for (int i = 0; i < 6; ++i) {
        const UINT expected[] = {10, 100, 101, 102, 20, 21};
        AssertItemId(menu, i, expected[i]);
    }
    assert(DestroyMenu(menu));
}

static void TestFlatItemsCreateContinuationChain(void) {
    HMENU menu = CreatePopupMenu();
    assert(menu != NULL);
    AppendCommand(menu, 10, FALSE);
    for (UINT id = 100; id < 110; ++id) {
        AppendCommand(menu, id, id == 108);
    }
    AppendCommand(menu, 20, FALSE);
    AppendCommand(menu, 21, FALSE);

    TrayMenuPaginationRange range = {1, 10};
    assert(TrayMenuPagination_ApplyRange(
        menu, &range, 6, L"More"));
    assert(GetMenuItemCount(menu) == 6);
    AssertItemId(menu, 0, 10);
    AssertItemId(menu, 1, 100);
    AssertItemId(menu, 2, 101);
    HMENU secondPage = AssertCheckedMore(menu, 3);
    AssertItemId(menu, 4, 20);
    AssertItemId(menu, 5, 21);

    assert(GetMenuItemCount(secondPage) == 6);
    for (int i = 0; i < 5; ++i) {
        AssertItemId(secondPage, i, 102u + (UINT)i);
    }
    HMENU thirdPage = AssertCheckedMore(secondPage, 5);
    assert(GetMenuItemCount(thirdPage) == 3);
    AssertItemId(thirdPage, 0, 107);
    AssertItemId(thirdPage, 1, 108);
    AssertItemId(thirdPage, 2, 109);
    assert(GetItemInfo(thirdPage, 1).fState & MFS_CHECKED);

    UINT ids[16] = {0};
    int collected = CollectCommandIds(menu, ids, _countof(ids), 0);
    const UINT expected[] = {
        10, 100, 101, 102, 103, 104, 105, 106,
        107, 108, 109, 20, 21
    };
    assert(collected == (int)_countof(expected));
    for (int i = 0; i < collected; ++i) assert(ids[i] == expected[i]);
    AssertPageLimits(menu, 6);
    assert(DestroyMenu(menu));
}

static void TestTrailingItemsAreDetectedAutomatically(void) {
    HMENU menu = CreatePopupMenu();
    assert(menu != NULL);
    AppendCommand(menu, 10, FALSE);
    for (UINT id = 100; id < 106; ++id) AppendCommand(menu, id, FALSE);

    TrayMenuPaginationRange range = {1, 6};
    AppendCommand(menu, 20, FALSE);
    AppendCommand(menu, 21, FALSE);
    assert(TrayMenuPagination_ApplyRange(
        menu, &range, 6, L"More"));
    assert(GetMenuItemCount(menu) == 6);

    UINT ids[10] = {0};
    int collected = CollectCommandIds(menu, ids, _countof(ids), 0);
    const UINT expected[] = {10, 100, 101, 102, 103, 104, 105, 20, 21};
    assert(collected == (int)_countof(expected));
    for (int i = 0; i < collected; ++i) assert(ids[i] == expected[i]);
    AssertPageLimits(menu, 6);
    assert(DestroyMenu(menu));
}

static void TestFirstPageAlwaysKeepsOneDynamicItem(void) {
    HMENU menu = CreatePopupMenu();
    assert(menu != NULL);
    for (UINT id = 10; id < 20; ++id) AppendCommand(menu, id, FALSE);
    for (UINT id = 100; id < 103; ++id) AppendCommand(menu, id, FALSE);
    for (UINT id = 20; id < 25; ++id) AppendCommand(menu, id, FALSE);

    TrayMenuPaginationRange range = {10, 3};
    assert(TrayMenuPagination_ApplyRange(
        menu, &range, 12, L"More"));
    assert(GetMenuItemCount(menu) == 17);
    AssertItemId(menu, 10, 100);
    MENUITEMINFOW moreItem = GetItemInfo(menu, 11);
    assert(moreItem.hSubMenu != NULL);
    assert(GetMenuItemCount(moreItem.hSubMenu) == 2);
    AssertItemId(moreItem.hSubMenu, 0, 101);
    AssertItemId(moreItem.hSubMenu, 1, 102);
    for (int i = 0; i < 5; ++i) {
        AssertItemId(menu, 12 + i, 20u + (UINT)i);
    }
    assert(DestroyMenu(menu));
}

static void TestExistingChildMenuIsPaginated(void) {
    HMENU menu = CreatePopupMenu();
    HMENU folder = CreatePopupMenu();
    assert(menu != NULL && folder != NULL);
    for (UINT id = 200; id < 213; ++id) {
        AppendCommand(folder, id, id == 211);
    }
    assert(AppendMenuW(menu, MF_POPUP, (UINT_PTR)folder, L"Folder"));

    TrayMenuPaginationRange range = {0, 1};
    assert(TrayMenuPagination_ApplyRange(
        menu, &range, 5, L"More"));
    assert(GetMenuItemCount(menu) == 1);
    assert(GetMenuItemCount(folder) == 5);
    HMENU secondPage = AssertCheckedMore(folder, 4);
    assert(GetMenuItemCount(secondPage) == 5);
    HMENU thirdPage = AssertCheckedMore(secondPage, 4);
    assert(GetMenuItemCount(thirdPage) == 5);

    UINT ids[16] = {0};
    int collected = CollectCommandIds(menu, ids, _countof(ids), 0);
    assert(collected == 13);
    for (int i = 0; i < collected; ++i) {
        assert(ids[i] == 200u + (UINT)i);
    }
    AssertPageLimits(menu, 5);
    assert(DestroyMenu(menu));
}

int main(void) {
    assert(TrayMenuPagination_GetScreenItemLimit() >= 4);
    TestMenuWithinLimitIsUnchanged();
    TestFlatItemsCreateContinuationChain();
    TestTrailingItemsAreDetectedAutomatically();
    TestFirstPageAlwaysKeepsOneDynamicItem();
    TestExistingChildMenuIsPaginated();
    puts("tray menu pagination tests passed");
    return 0;
}
