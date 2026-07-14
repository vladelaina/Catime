#include "startup_shortcut.h"

#include <stdio.h>

static int failures = 0;

static void Expect(const char* name, BOOL actual, BOOL expected) {
    if (!!actual != !!expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

int main(void) {
    wchar_t tempDirectory[MAX_PATH] = {0};
    wchar_t shortcutPath[MAX_PATH] = {0};
    wchar_t corruptPath[MAX_PATH] = {0};
    wchar_t executablePath[MAX_PATH] = {0};
    wchar_t oversizedArguments[200] = {0};
    DWORD tempLength = GetTempPathW(_countof(tempDirectory), tempDirectory);
    DWORD exeLength = GetModuleFileNameW(NULL, executablePath,
                                         _countof(executablePath));

    if (tempLength == 0 || tempLength >= _countof(tempDirectory) ||
        exeLength == 0 || exeLength >= _countof(executablePath)) {
        fputs("failed to resolve test paths\n", stderr);
        return 1;
    }

    if (swprintf_s(shortcutPath, _countof(shortcutPath),
                   L"%lsCatimeStartupTest-%lu.lnk.1.tmp", tempDirectory,
                   GetCurrentProcessId()) < 0 ||
        swprintf_s(corruptPath, _countof(corruptPath),
                   L"%lsCatimeStartupTest-%lu-corrupt.lnk", tempDirectory,
                   GetCurrentProcessId()) < 0) {
        fputs("failed to build test paths\n", stderr);
        return 1;
    }

    DeleteFileW(shortcutPath);
    DeleteFileW(corruptPath);
    for (size_t i = 0; i + 1 < _countof(oversizedArguments); ++i) {
        oversizedArguments[i] = L'A';
    }

    Expect("write valid shell link",
           StartupShortcut_Write(shortcutPath, executablePath, L"--startup"),
           TRUE);
    Expect("validate target arguments and working directory",
           StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                     L"--startup"),
           TRUE);
    Expect("reject stale arguments",
           StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                     L"--old-startup"),
           FALSE);
    Expect("reject stale executable target",
           StartupShortcut_IsCurrent(shortcutPath, L"C:\\missing-catime.exe",
                                     L"--startup"),
           FALSE);
    Expect("rewrite stale arguments",
           StartupShortcut_Write(shortcutPath, executablePath,
                                 L"--old-startup"),
           TRUE);
    Expect("detect rewritten stale shortcut",
           StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                     L"--startup"),
           FALSE);
    Expect("transactionally replace stale shortcut",
           StartupShortcut_ReplaceAtomically(shortcutPath, executablePath,
                                             L"--startup"),
           TRUE);
    Expect("validate transaction result",
           StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                     L"--startup"),
           TRUE);
    Expect("reject an unverifiable replacement",
           StartupShortcut_ReplaceAtomically(shortcutPath, executablePath,
                                             oversizedArguments),
           FALSE);
    Expect("preserve the previous shortcut after rejected replacement",
           StartupShortcut_IsCurrent(shortcutPath, executablePath,
                                     L"--startup"),
           TRUE);

    HANDLE corrupt = CreateFileW(corruptPath, GENERIC_WRITE, 0, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (corrupt != INVALID_HANDLE_VALUE) {
        const char bytes[] = "not a shell link";
        DWORD written = 0;
        WriteFile(corrupt, bytes, (DWORD)sizeof(bytes), &written, NULL);
        CloseHandle(corrupt);
        Expect("reject corrupt shell link",
               StartupShortcut_IsCurrent(corruptPath, executablePath,
                                         L"--startup"),
               FALSE);
    } else {
        fputs("failed to create corrupt test shortcut\n", stderr);
        failures++;
    }

    DeleteFileW(shortcutPath);
    DeleteFileW(corruptPath);

    if (failures == 0) puts("startup shortcut tests passed");
    return failures == 0 ? 0 : 1;
}
