#include "window_commands_internal.h"
#include "multi_window.h"

LRESULT CmdNewTimerWindow(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    (void)lp;
    (void)MultiWindow_LaunchNewTimerWindow(hwnd);
    return 0;
}

float ParseDefaultScaleOrFallback(const char* value, float fallback) {
    char* end = NULL;
    double parsed = value ? strtod(value, &end) : 0.0;
    if (!value || end == value || !DoubleIsFiniteStrict(parsed) || parsed <= 0.0) {
        return fallback;
    }
    return (float)parsed;
}

LRESULT CmdExit(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    HideWindowIntentionally(hwnd);
    DestroyWindow(hwnd);
    return 0;
}

LRESULT CmdAbout(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ShowAboutDialog(hwnd);
    return 0;
}

LRESULT CmdToggleTopmost(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    ToggleTopmost(hwnd);
    return 0;
}

void ToggleTextEffect(HWND hwnd, TextEffectType effect) {
    if (effect != TEXT_EFFECT_NONE && !TextEffect_IsSelectable(effect)) return;
    TextEffectType previous = CLOCK_TEXT_EFFECT;
    TextEffectType next = previous == effect ? TEXT_EFFECT_NONE : effect;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    if (!WriteIniString(
            INI_SECTION_DISPLAY, "TEXT_EFFECT",
            TextEffect_ToConfigString(next), configPath)) return;
    CLOCK_TEXT_EFFECT = next;
    g_AppConfig.display.text_effect = next;
    if (TextEffect_UsesSharedEffectBuffer(previous) &&
        !TextEffect_UsesSharedEffectBuffer(next)) CleanupDrawingEffects();
    InvalidateRect(hwnd, NULL, TRUE);
}

LRESULT CmdEditMode(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ToggleEditMode(hwnd); return 0;
}

LRESULT CmdToggleVisibility(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ToggleWindowVisibility(hwnd); return 0;
}

LRESULT CmdCustomizeColor(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowColorDialog(hwnd); return 0;
}

LRESULT CmdFontLicense(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    extern void ShowFontLicenseDialog(HWND);
    ShowFontLicenseDialog(hwnd);
    return 0;
}

LRESULT CmdFontAdvanced(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    wchar_t fontsPath[MAX_PATH];
    if (GetFontsFolderW(fontsPath, MAX_PATH, TRUE)) {
        ShellExecuteW(hwnd, L"open", fontsPath, NULL, NULL, SW_SHOWNORMAL);
    }
    return 0;
}

LRESULT CmdSystemFontPicker(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowSystemFontDialog(hwnd); return 0;
}

LRESULT CmdAutoStart(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (IsRunningPackagedApp()) {
        OpenStartupSettings();
        return 0;
    }
    AutoStartStatus status = GetAutoStartStatus();
    if (status == AUTO_START_STATUS_DISABLED_BY_WINDOWS) {
        OpenStartupSettings();
    } else if (status == AUTO_START_STATUS_ENABLED) {
        if (DisableAutoStart()) {
            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
        }
    } else if (EnableAutoStart()) {
        CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
    }
    return 0;
}

LRESULT CmdColorDialog(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp; ShowColorInputDialog(hwnd); return 0;
}

LRESULT CmdColorPanel(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp; (void)lp;
    if (ShowColorDialog(hwnd) != (COLORREF)-1) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
}
