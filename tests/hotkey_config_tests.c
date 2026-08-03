#include "config/config_settings_api.h"

#include <stdio.h>
#include <windows.h>

#ifndef HOTKEYF_SHIFT
#define HOTKEYF_SHIFT 0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT 0x04
#endif

#ifndef VK_PROCESSKEY
#define VK_PROCESSKEY 0xE5
#endif
#ifndef VK_PACKET
#define VK_PACKET 0xE7
#endif

static int g_failures = 0;

static void ExpectAllowed(const char* label, WORD hotkey, BOOL expected) {
    BOOL actual = IsHotkeyValueAllowed(hotkey);
    if (actual != expected) {
        fprintf(stderr, "%s: expected allowed=%d, got %d\n",
                label, expected, actual);
        g_failures++;
    }
}

int main(void) {
    ExpectAllowed("Ctrl+A", MAKEWORD('A', HOTKEYF_CONTROL), TRUE);
    ExpectAllowed("Alt+F4", MAKEWORD(VK_F4, HOTKEYF_ALT), TRUE);
    ExpectAllowed("modifier only", MAKEWORD(VK_SHIFT, 0), FALSE);

    ExpectAllowed("IME process key", MAKEWORD(VK_PROCESSKEY, 0), FALSE);
    ExpectAllowed("Shift+IME process key",
                  MAKEWORD(VK_PROCESSKEY, HOTKEYF_SHIFT), FALSE);
    ExpectAllowed("Ctrl+IME process key",
                  MAKEWORD(VK_PROCESSKEY, HOTKEYF_CONTROL), FALSE);
    ExpectAllowed("Unicode packet", MAKEWORD(VK_PACKET, 0), FALSE);
    ExpectAllowed("Alt+Unicode packet",
                  MAKEWORD(VK_PACKET, HOTKEYF_ALT), FALSE);

    if (g_failures != 0) {
        fprintf(stderr, "%d hotkey config test(s) failed\n", g_failures);
        return 1;
    }
    return 0;
}
