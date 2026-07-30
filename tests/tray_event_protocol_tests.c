#include "tray/tray_event_protocol.h"
#include "../resource/resource.h"
#include <shellapi.h>
#include <assert.h>
#include <stdio.h>

#if !defined(WINVER) || WINVER != 0x0601
#error "Tray protocol tests must target Windows 7 (WINVER 0x0601)"
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT != 0x0601
#error "Tray protocol tests must target Windows 7 (_WIN32_WINNT 0x0601)"
#endif

#if !defined(NTDDI_VERSION) || NTDDI_VERSION != 0x06010000
#error "Tray protocol tests must target Windows 7 (NTDDI 0x06010000)"
#endif

static LPARAM Version4Param(UINT message, UINT iconId) {
    return MAKELPARAM(message, iconId);
}

static void TestWindows7TrayContract(void) {
    assert(NOTIFYICON_VERSION_4 == 4);
    assert(NIM_SETFOCUS == 0x00000003);
    assert(NIF_SHOWTIP == 0x00000080);
    assert(NIN_POPUPOPEN == WM_USER + 6);
    assert(NIN_POPUPCLOSE == WM_USER + 7);
    assert(sizeof(NOTIFYICONDATAW) ==
           NOTIFYICONDATAW_V3_SIZE + sizeof(HICON));
}

static void TestLegacyCallbacks(void) {
    TrayCallbackEvent event = {0};
    assert(TrayCallback_Decode(FALSE, CLOCK_ID_TRAY_APP_ICON,
                               WM_LBUTTONUP, &event));
    assert(event.iconId == CLOCK_ID_TRAY_APP_ICON);
    assert(event.message == WM_LBUTTONUP);
    assert(event.kind == TRAY_CALLBACK_PRIMARY_MENU);
    assert(!event.hasAnchor);

    assert(TrayCallback_Decode(FALSE, CLOCK_ID_TRAY_APP_ICON,
                               WM_RBUTTONUP, &event));
    assert(event.kind == TRAY_CALLBACK_SECONDARY_MENU);

    assert(TrayCallback_Decode(FALSE, CLOCK_ID_TRAY_APP_ICON,
                               WM_MOUSEMOVE, &event));
    assert(event.kind == TRAY_CALLBACK_HOVER_MOVE);
}

static void TestVersion4Callbacks(void) {
    TrayCallbackEvent event = {0};
    WPARAM anchor = (WPARAM)MAKELPARAM((WORD)-24, (WORD)315);
    assert(TrayCallback_Decode(
        TRUE, anchor,
        Version4Param(NIN_SELECT, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.iconId == CLOCK_ID_TRAY_APP_ICON);
    assert(event.kind == TRAY_CALLBACK_PRIMARY_MENU);
    assert(event.hasAnchor);
    assert(event.anchor.x == -24);
    assert(event.anchor.y == 315);

    assert(TrayCallback_Decode(
        TRUE, anchor,
        Version4Param(NIN_KEYSELECT, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.kind == TRAY_CALLBACK_PRIMARY_MENU);

    assert(TrayCallback_Decode(
        TRUE, anchor,
        Version4Param(WM_CONTEXTMENU, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.kind == TRAY_CALLBACK_SECONDARY_MENU);

    assert(TrayCallback_Decode(
        TRUE, anchor,
        Version4Param(WM_LBUTTONUP, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.kind == TRAY_CALLBACK_NONE);
}

static void TestHoverCallbacks(void) {
    TrayCallbackEvent event = {0};
    assert(TrayCallback_Decode(
        TRUE, 0,
        Version4Param(NIN_POPUPOPEN, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.kind == TRAY_CALLBACK_HOVER_OPEN);
    assert(!event.hasAnchor);

    assert(TrayCallback_Decode(
        TRUE, 0,
        Version4Param(NIN_POPUPCLOSE, CLOCK_ID_TRAY_APP_ICON), &event));
    assert(event.kind == TRAY_CALLBACK_HOVER_CLOSE);
}

int main(void) {
    assert(!TrayCallback_Decode(TRUE, 0, 0, NULL));
    TestWindows7TrayContract();
    TestLegacyCallbacks();
    TestVersion4Callbacks();
    TestHoverCallbacks();
    puts("tray event protocol tests passed");
    return 0;
}
