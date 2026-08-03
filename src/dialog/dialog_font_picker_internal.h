/**
 * @file dialog_font_picker_internal.h
 * @brief Private state and helpers for the system font picker.
 */

#ifndef DIALOG_FONT_PICKER_INTERNAL_H
#define DIALOG_FONT_PICKER_INTERNAL_H

#include "dialog/dialog_font_picker.h"

typedef struct {
    char originalFontName[MAX_PATH];
    char originalFileName[MAX_PATH];
    char originalRuntimeFileName[MAX_PATH];
    BOOL closeHandled;
} FontDialogState;

typedef struct {
    wchar_t fontName[LF_FACESIZE];
    char fontPath[MAX_PATH];
} FontMapEntry;

typedef struct {
    HWND hdlg;
    HANDLE stopEvent;
    LONG generation;
} FontEnumerationThreadParams;

#define WM_APP_FONT_ENUM_COMPLETE (WM_APP + 410)
#define MAX_FONT_PICKER_ENTRIES 1024
#define FONT_ENUM_POLL_TIMER_ID 9997
#define FONT_PICKER_TOPMOST_TIMER_ID 9998
#define FONT_ENUM_DEFERRED_CLEANUP_TIMER_ID 9996
#define FONT_ENUM_START_RETRY_TIMER_ID 9995
#define FONT_ENUM_STOP_WAIT_MS 250
#define FONT_ENUM_SHUTDOWN_WAIT_MS 2000
#define FONT_ENUM_POLL_INTERVAL_MS 50
#define FONT_ENUM_DEFERRED_CLEANUP_INTERVAL_MS 1000
#define FONT_ENUM_START_RETRY_INTERVAL_MS 1000

extern FontDialogState g_fontState;
extern FontMapEntry* g_fontMap;
extern int g_fontMapCount;
extern int g_fontMapCapacity;
extern HANDLE g_fontEnumThread;
extern HANDLE g_fontEnumStopEvent;
extern BOOL g_fontListReady;
extern BOOL g_fontEnumRestartAfterCleanup;
extern BOOL g_fontEnumPrefetchActive;
extern volatile LONG g_fontMapCacheReady;
extern volatile LONG g_fontEnumGeneration;
extern int g_currentFontIndex;
extern int g_previewFontIndex;

void DialogFontPickerInternal_SaveOriginalFont(void);
void DialogFontPickerInternal_RestoreOriginalFont(void);
BOOL DialogFontPickerInternal_PreviewFont(
    const wchar_t* fontName, const char* cachedFontPath,
    HWND hdlg, HWND hwndList);
BOOL DialogFontPickerInternal_CommitSelection(HWND hwnd);

BOOL DialogFontPickerInternal_GetSystemFontPath(
    const wchar_t* fontName, char* outPath,
    size_t outPathSize, HANDLE stopEvent);
BOOL DialogFontPickerInternal_CheckRequiredGlyphs(
    HDC hdc, const wchar_t* fontName, HANDLE stopEvent);

void DialogFontPickerInternal_ResetFontMap(void);
void DialogFontPickerInternal_PopulateFontList(HWND hdlg);
void DialogFontPickerInternal_BuildFontMap(HANDLE stopEvent);

BOOL DialogFontPickerInternal_ShouldStopEnumeration(HANDLE stopEvent);
BOOL DialogFontPickerInternal_IsFontMapCacheReady(void);
BOOL DialogFontPickerInternal_CleanupCompletedEnumeration(void);
BOOL DialogFontPickerInternal_StopEnumeration(DWORD timeoutMs);
BOOL DialogFontPickerInternal_StartPollTimer(HWND hdlg);
void DialogFontPickerInternal_ScheduleDeferredCleanup(void);
BOOL DialogFontPickerInternal_StartEnumeration(HWND hdlg);

BOOL DialogFontPickerInternal_OnMeasureItem(HWND hdlg, LPARAM lp);
BOOL DialogFontPickerInternal_OnDrawItem(HWND hdlg, LPARAM lp);

INT_PTR DialogFontPickerInternal_OnInit(HWND hdlg);
INT_PTR DialogFontPickerInternal_OnEnumerationComplete(HWND hdlg, WPARAM wp);
INT_PTR DialogFontPickerInternal_OnTimer(HWND hdlg, WPARAM wp);
INT_PTR DialogFontPickerInternal_OnCommand(HWND hdlg, WPARAM wp);
INT_PTR DialogFontPickerInternal_OnDestroy(HWND hdlg);

#endif /* DIALOG_FONT_PICKER_INTERNAL_H */
