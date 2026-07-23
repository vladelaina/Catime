#ifndef STARTUP_INTERNAL_H
#define STARTUP_INTERNAL_H

#include <windows.h>
#include <stddef.h>

#define STARTUP_LINK_FILENAME L"Catime.lnk"
#define STARTUP_CMD_ARG L"--startup"

typedef struct {
    HANDLE handle;
    BOOL acquired;
} StartupShortcutLock;

BOOL StartupPaths_GetShortcutPath(wchar_t* output, size_t outputSize);
BOOL StartupPaths_GetExecutablePath(wchar_t* output, size_t outputSize);
void StartupPaths_RemoveLegacyMarker(void);
BOOL StartupPaths_AcquireLock(StartupShortcutLock* lock);
void StartupPaths_ReleaseLock(StartupShortcutLock* lock);

BOOL StartupState_QueryWindowsDisabled(BOOL* disabled);
BOOL StartupState_ClearWindowsApproval(void);
BOOL StartupState_WritePreference(const char* preference);
void StartupState_ReadPreference(char* output, size_t outputSize);

void StartupMode_ApplyConfigured(HWND hwnd);

#endif /* STARTUP_INTERNAL_H */
