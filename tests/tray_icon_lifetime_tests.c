#include "tray/tray_icon_lifetime.h"

#include <stdio.h>

static int g_failures = 0;

static HICON CreateTestIcon(BYTE fill) {
    BYTE andMask[64];
    BYTE xorMask[64];
    for (size_t i = 0; i < sizeof(andMask); ++i) {
        andMask[i] = 0;
        xorMask[i] = fill;
    }
    return CreateIcon(NULL, 16, 16, 1, 1, andMask, xorMask);
}

static BOOL IsIconUsable(HICON icon) {
    ICONINFO info = {0};
    if (!icon || !GetIconInfo(icon, &info)) return FALSE;
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return TRUE;
}

static void Expect(const char* name, BOOL value) {
    if (!value) {
        fprintf(stderr, "%s\n", name);
        g_failures++;
    }
}

static void TestBoundedResourceUse(void) {
    TrayIconLifetime_ReleaseAll();
    DWORD baselineUser = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    DWORD baselineGdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);

    for (int i = 0; i < 5000; ++i) {
        HICON icon = CreateTestIcon((BYTE)(i + 1));
        if (!icon) {
            Expect("stress icon creation failed", FALSE);
            break;
        }
        TrayIconLifetime_Retain(icon);
    }

    DWORD retainedUser = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    DWORD retainedGdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    Expect("retained USER objects grew without bound",
           retainedUser <= baselineUser + 8u);
    Expect("retained GDI objects grew without bound",
           retainedGdi <= baselineGdi + 8u);

    TrayIconLifetime_ReleaseAll();
    DWORD releasedUser = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    DWORD releasedGdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    Expect("USER objects did not return near baseline",
           releasedUser <= baselineUser + 2u);
    Expect("GDI objects did not return near baseline",
           releasedGdi <= baselineGdi + 2u);
}

int main(void) {
    TrayIconLifetime_ReleaseAll();

    HICON duplicate = CreateTestIcon(0x09);
    Expect("failed to create duplicate-retain test icon", duplicate != NULL);
    if (!duplicate) return 1;
    TrayIconLifetime_Retain(duplicate);
    TrayIconLifetime_Retain(duplicate);
    Expect("duplicate retain invalidated the current icon",
           IsIconUsable(duplicate));
    TrayIconLifetime_ReleaseAll();
    Expect("duplicate-retained icon survived release",
           !IsIconUsable(duplicate));

    HICON first = CreateTestIcon(0x11);
    HICON second = CreateTestIcon(0x22);
    HICON third = CreateTestIcon(0x33);
    Expect("failed to create test icons", first && second && third);
    if (!first || !second || !third) {
        if (first) DestroyIcon(first);
        if (second) DestroyIcon(second);
        if (third) DestroyIcon(third);
        return 1;
    }

    TrayIconLifetime_Retain(first);
    TrayIconLifetime_Retain(second);
    Expect("first icon was not retained for one replacement", IsIconUsable(first));
    Expect("second icon is not usable", IsIconUsable(second));

    TrayIconLifetime_Retain(first);
    Expect("resubmitting the previous icon invalidated it", IsIconUsable(first));
    Expect("resubmitting the previous icon invalidated the other generation",
           IsIconUsable(second));
    TrayIconLifetime_Retain(second);

    TrayIconLifetime_Retain(third);
    Expect("oldest icon remained usable after the third submission",
           !IsIconUsable(first));
    Expect("previous icon was released too early", IsIconUsable(second));
    Expect("current icon is not usable", IsIconUsable(third));

    TrayIconLifetime_ReleaseAll();
    Expect("previous icon survived release", !IsIconUsable(second));
    Expect("current icon survived release", !IsIconUsable(third));

    TestBoundedResourceUse();

    if (g_failures != 0) {
        fprintf(stderr, "%d tray icon lifetime test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
