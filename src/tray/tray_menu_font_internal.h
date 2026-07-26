#ifndef CATIME_TRAY_MENU_FONT_INTERNAL_H
#define CATIME_TRAY_MENU_FONT_INTERNAL_H

#include "tray/tray_menu_font.h"

#include <stddef.h>
#include <wchar.h>
#include <windows.h>

#define MAX_FONT_ENTRIES FONT_MENU_MAX_ENTRIES
#define MAX_RECURSION_DEPTH 10
#define MAX_FONT_NAME_LENGTH 260
#define MAX_FONT_SCAN_ENTRIES 4096
#define FONT_MENU_SCAN_FAILED (-1)

typedef struct {
    wchar_t fileName[MAX_FONT_NAME_LENGTH];
    wchar_t relativePath[MAX_PATH];
    wchar_t displayName[MAX_FONT_NAME_LENGTH];
} FontEntry;

extern FontEntry g_fontMenuCache[MAX_FONT_ENTRIES];
extern int g_fontMenuCacheCount;
extern BOOL g_fontMenuCacheReady;
extern BOOL g_fontMenuCacheFailed;
extern SRWLOCK g_fontMenuCacheLock;
extern SRWLOCK g_fontScanThreadLock;
extern HANDLE g_hFontScanThread;
extern HANDLE g_hRetiredFontScanThread;
extern volatile LONG g_fontScanShuttingDown;
extern volatile LONG g_fontScanGeneration;
extern volatile LONG g_fontMenuLastScanTick;

BOOL FontMenuInternal_IsScanCanceled(LONG generation);
BOOL FontMenuInternal_CleanupRetiredScanThreadLocked(DWORD waitMs);
int FontMenuInternal_ScanFontsFolder(FontEntry* entries, int capacity,
                                     LONG generation);
void FontMenuInternal_BuildMenuFromEntries(
    HMENU hRootMenu, const FontEntry* entries, int count,
    const wchar_t* currentFontRelPath, int* fontId);
void FontMenuInternal_ResetIdMap(void);
BOOL FontMenuInternal_RememberId(UINT id, const wchar_t* relativePath);
void FontMenuInternal_ForgetLastId(UINT id);

#endif /* CATIME_TRAY_MENU_FONT_INTERNAL_H */
