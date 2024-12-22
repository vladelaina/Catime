#include <windows.h>
#include "rightClickMenu.h"
// 托盘图标的右键菜单响应函数
void ShowContextMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    // 添加选项：5, 10, 25 分钟
    AppendMenu(hMenu, MF_STRING, 101, "Customize");
    AppendMenu(hMenu, MF_STRING, 102, "5");
    AppendMenu(hMenu, MF_STRING, 103, "10");
    AppendMenu(hMenu, MF_STRING, 104, "25");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // 确保菜单显示在应用程序的窗口上
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}
