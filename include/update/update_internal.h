#ifndef UPDATE_INTERNAL_H
#define UPDATE_INTERNAL_H

#include <windows.h>
#include <wininet.h>

/* ============================================================================
 * Constants & Macros
 * ============================================================================ */

#define GITHUB_API_URL "https://api.github.com/repos/vladelaina/Catime/releases/latest"
#define USER_AGENT "Catime Update Checker"
#define VERSION_BUFFER_SIZE 32
#define URL_BUFFER_SIZE 512
#define NOTES_BUFFER_SIZE 16384
#define INITIAL_HTTP_BUFFER_SIZE 8192
#define ERROR_MSG_BUFFER_SIZE 256

#define MODERN_SCROLLBAR_WIDTH 8
#define MODERN_SCROLLBAR_MARGIN 2
#define MODERN_SCROLLBAR_MIN_THUMB 30
#define MODERN_SCROLLBAR_THUMB_COLOR RGB(150, 150, 150)
#define MODERN_SCROLLBAR_THUMB_HOVER_COLOR RGB(120, 120, 120)
#define MODERN_SCROLLBAR_THUMB_DRAG_COLOR RGB(100, 100, 100)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** @brief Version info for dialog display */
typedef struct {
    const char* currentVersion;
    const char* latestVersion;
    const char* downloadUrl;
    const char* releaseNotes;
} VersionInfo;

/** @brief Pre-release type priority (alpha < beta < rc < stable) */
typedef struct {
    const char* prefix;
    int prefixLen;
    int priority;
} PreReleaseType;

/** @brief HTTP resource handles for cleanup */
typedef struct {
    HINTERNET hInternet;
    HINTERNET hConnect;
    char* buffer;
} HttpResources;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

/* update_control.c */
LRESULT CALLBACK NotesControlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                 UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
void CalculateScrollbarThumbRect(RECT clientRect, int scrollPos, int scrollMax,
                                int scrollPage, RECT* outThumbRect);
void DrawRoundedRect(HDC hdc, RECT rect, int radius, COLORREF color);

/* update_parser.c */
BOOL ParseGitHubRelease(const char* jsonResponse, char* latestVersion, size_t versionMaxLen,
                       char* downloadUrl, size_t urlMaxLen, char* releaseNotes, size_t notesMaxLen);

/* update_ui.c */
int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion,
                          const char* downloadUrl, const char* releaseNotes);
void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg);
void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion);
void ShowExitMessageDialog(HWND hwnd);

#endif // UPDATE_INTERNAL_H
