#include "shortcut_checker_internal.h"
#include "startup_shortcut.h"
#include "utils/string_convert.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures = 0;

static void Expect(const char* name, BOOL actual, BOOL expected) {
    if (!!actual != !!expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

static BOOL ResolveTestAumid(wchar_t* aumid, size_t aumidSize) {
    static const wchar_t* candidates[] = {
        L"windows.immersivecontrolpanel_cw5n1h2txyewy!"
        L"microsoft.windows.immersivecontrolpanel",
        L"MicrosoftWindows.Client.CBS_cw5n1h2txyewy!WindowsBackup"
    };
    wchar_t parsingName[MAX_PATH] = {0};
    for (size_t i = 0; i < _countof(candidates); ++i) {
        PIDLIST_ABSOLUTE pidl = NULL;
        if (_snwprintf_s(parsingName, _countof(parsingName), _TRUNCATE,
                         L"shell:AppsFolder\\%ls", candidates[i]) < 0) {
            continue;
        }
        HRESULT hr = SHParseDisplayName(parsingName, NULL, &pidl, 0, NULL);
        if (SUCCEEDED(hr) && pidl) {
            CoTaskMemFree(pidl);
            return wcscpy_s(aumid, aumidSize, candidates[i]) == 0;
        }
        if (pidl) CoTaskMemFree(pidl);
    }
    return FALSE;
}

int main(void) {
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
        fprintf(stderr, "COM initialization failed: 0x%08X\n",
                (unsigned int)init);
        return 1;
    }

    wchar_t aumid[MAX_PATH] = {0};
    wchar_t tempDirectory[MAX_PATH] = {0};
    wchar_t shortcutW[MAX_PATH] = {0};
    wchar_t legacyShortcutW[MAX_PATH] = {0};
    char shortcut[MAX_PATH] = {0};
    DWORD tempLength = GetTempPathW(_countof(tempDirectory), tempDirectory);
    if (!ResolveTestAumid(aumid, _countof(aumid)) || tempLength == 0 ||
        tempLength >= _countof(tempDirectory) ||
        _snwprintf_s(shortcutW, _countof(shortcutW), _TRUNCATE,
                     L"%lsCatimePackagedShortcutTest-%lu.lnk",
                     tempDirectory, GetCurrentProcessId()) < 0 ||
        _snwprintf_s(legacyShortcutW, _countof(legacyShortcutW), _TRUNCATE,
                     L"%lsCatimeLegacyPackagedShortcutTest-%lu.lnk",
                     tempDirectory, GetCurrentProcessId()) < 0 ||
        !WideToUtf8(shortcutW, shortcut, sizeof(shortcut))) {
        fputs("failed to resolve packaged shortcut test inputs\n", stderr);
        if (SUCCEEDED(init)) CoUninitialize();
        return 1;
    }

    DeleteFileW(shortcutW);
    DeleteFileW(legacyShortcutW);
    Expect("invalid AUMID is rejected",
           ShortcutShell_CreateOrUpdatePackaged(
               L"invalid.package!missing", shortcut), FALSE);
    Expect("create AppsFolder shortcut",
           ShortcutShell_CreateOrUpdatePackaged(aumid, shortcut), TRUE);
    Expect("round-trip AppsFolder PIDL",
           ShortcutShell_CheckPackagedPath(
               aumid, shortcut, NULL, 0) == SHORTCUT_POINTS_TO_CURRENT,
           TRUE);
    Expect("different AUMID is not current",
           ShortcutShell_CheckPackagedPath(
               L"invalid.package!missing", shortcut, NULL, 0) ==
               SHORTCUT_CHECK_ERROR,
           TRUE);

    static const wchar_t legacyTarget[] =
        L"D:\\WindowsApps\\"
        L"vladelaina.Catime_1.5.0.0_x86__hnew8t3b8e0t6\\catime.exe";
    char legacyShortcut[MAX_PATH] = {0};
    char detectedTarget[MAX_PATH] = {0};
    Expect("prepare legacy direct-executable shortcut",
           StartupShortcut_Write(legacyShortcutW, legacyTarget, L""), TRUE);
    Expect("convert legacy shortcut path",
           WideToUtf8(legacyShortcutW, legacyShortcut,
                      sizeof(legacyShortcut)), TRUE);
    Expect("legacy shortcut is reported as another target",
           ShortcutShell_CheckPackagedPath(
               aumid, legacyShortcut, detectedTarget,
               sizeof(detectedTarget)) == SHORTCUT_POINTS_TO_OTHER,
           TRUE);
    char legacyTargetUtf8[MAX_PATH] = {0};
    Expect("convert expected legacy target",
           WideToUtf8(legacyTarget, legacyTargetUtf8,
                      sizeof(legacyTargetUtf8)), TRUE);
    Expect("legacy target is preserved for migration policy",
           _stricmp(detectedTarget, legacyTargetUtf8) == 0, TRUE);

    HANDLE corrupt = CreateFileW(shortcutW, GENERIC_WRITE, 0, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (corrupt != INVALID_HANDLE_VALUE) {
        const char bytes[] = "not a shortcut";
        DWORD written = 0;
        WriteFile(corrupt, bytes, sizeof(bytes), &written, NULL);
        CloseHandle(corrupt);
        Expect("corrupt shortcut reports inspection error",
               ShortcutShell_CheckPackagedPath(
                   aumid, shortcut, NULL, 0) == SHORTCUT_CHECK_ERROR,
               TRUE);
    } else {
        failures++;
    }

    DeleteFileW(shortcutW);
    DeleteFileW(legacyShortcutW);
    if (SUCCEEDED(init)) CoUninitialize();
    if (failures == 0) puts("packaged shortcut tests passed");
    return failures == 0 ? 0 : 1;
}
