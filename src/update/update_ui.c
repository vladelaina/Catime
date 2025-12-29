/**
 * @file update_ui.c
 * @brief Update dialogs and notification UI
 */
#include "update/update_internal.h"
#include "markdown/markdown_parser.h"
#include "dialog/dialog_procedure.h"
#include "dialog/dialog_language.h"
#include "dialog/dialog_common.h"
#include "language.h"
#include "log.h"
#include "utils/string_convert.h"
#include "../../resource/resource.h"
#include <strsafe.h>
#include <commctrl.h>

/* Global dialog handles for modeless dialogs */
static HWND g_hwndUpdateDialog = NULL;
static HWND g_hwndNoUpdateDialog = NULL;
static HWND g_hwndExitMsgDialog = NULL;

/* Store version info for modeless update dialog */
static VersionInfo g_updateVersionInfo = {0};

/* Store download URL copy for async update */
static char g_downloadUrlCopy[512] = {0};

/* Thin wrappers using utils/string_convert.h */
static inline wchar_t* LocalUtf8ToWideAlloc(const char* utf8Str) {
    return Utf8ToWideAlloc(utf8Str);
}

/** @brief Initialize dialog (center, localize) */
static void InitializeDialog(HWND hwndDlg, int dialogId) {
    ApplyDialogLanguage(hwndDlg, dialogId);
    MoveDialogToPrimaryScreen(hwndDlg);
}

/** @brief Exit notification dialog */
INT_PTR CALLBACK ExitMsgDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_EXIT_MSG, hwndDlg);
            g_hwndExitMsgDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_EXIT_DIALOG);
            
            SetDlgItemTextW(hwndDlg, IDC_EXIT_TEXT, 
                GetLocalizedString(NULL, L"The application will exit now"));
            SetDlgItemTextW(hwndDlg, IDOK, 
                GetLocalizedString(NULL, L"OK"));
            SetWindowTextW(hwndDlg, 
                GetLocalizedString(NULL, L"Catime - Update Notice"));
            return TRUE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstance(DIALOG_INSTANCE_EXIT_MSG);
            g_hwndExitMsgDialog = NULL;
            break;
    }
    return FALSE;
}

/** @brief Update available dialog */
INT_PTR CALLBACK UpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* g_notesDisplayText = NULL;
    static MarkdownLink* g_notesLinks = NULL;
    static int g_notesLinkCount = 0;
    static MarkdownHeading* g_notesHeadings = NULL;
    static int g_notesHeadingCount = 0;
    static MarkdownStyle* g_notesStyles = NULL;
    static int g_notesStyleCount = 0;
    static MarkdownListItem* g_notesListItems = NULL;
    static int g_notesListItemCount = 0;
    static MarkdownBlockquote* g_notesBlockquotes = NULL;
    static int g_notesBlockquoteCount = 0;
    static int g_textHeight = 0;

    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_UPDATE, hwndDlg);
            g_hwndUpdateDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_UPDATE_DIALOG);
            g_textHeight = 0;

            VersionInfo* versionInfo = &g_updateVersionInfo;
            if (versionInfo->currentVersion) {
                wchar_t* currentVerW = LocalUtf8ToWideAlloc(versionInfo->currentVersion);
                wchar_t* latestVerW = LocalUtf8ToWideAlloc(versionInfo->latestVersion);

                if (currentVerW && latestVerW) {
                    wchar_t displayText[256];
                    StringCbPrintfW(displayText, sizeof(displayText), L"%s %s\n%s %s",
                        GetLocalizedString(NULL, L"Current version:"), currentVerW,
                        GetLocalizedString(NULL, L"New version:"), latestVerW);
                    SetDlgItemTextW(hwndDlg, IDC_UPDATE_TEXT, displayText);
                }

                free(currentVerW);
                free(latestVerW);

                wchar_t* notesW = LocalUtf8ToWideAlloc(versionInfo->releaseNotes);
                if (notesW && notesW[0]) {
                    /* Wrap release notes with <md> tags for markdown parsing */
                    size_t notesLen = wcslen(notesW);
                    size_t bufSize = notesLen + 16;
                    wchar_t* wrappedNotes = (wchar_t*)malloc(bufSize * sizeof(wchar_t));
                    if (wrappedNotes) {
                        wcscpy_s(wrappedNotes, bufSize, L"<md>\n");
                        wcscat_s(wrappedNotes, bufSize, notesW);
                        wcscat_s(wrappedNotes, bufSize, L"\n</md>");
                        ParseMarkdownLinks(wrappedNotes, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
                                           &g_notesHeadings, &g_notesHeadingCount,
                                           &g_notesStyles, &g_notesStyleCount,
                                           &g_notesListItems, &g_notesListItemCount,
                                           &g_notesBlockquotes, &g_notesBlockquoteCount);
                        free(wrappedNotes);
                    }
                    free(notesW);
                } else {
                    free(notesW);
                    const wchar_t* noNotes = GetLocalizedString(NULL, L"No release notes available.");
                    ParseMarkdownLinks(noNotes, &g_notesDisplayText, &g_notesLinks, &g_notesLinkCount,
                                       &g_notesHeadings, &g_notesHeadingCount,
                                       &g_notesStyles, &g_notesStyleCount,
                                       &g_notesListItems, &g_notesListItemCount,
                                       &g_notesBlockquotes, &g_notesBlockquoteCount);
                }

                HWND hwndNotes = GetDlgItem(hwndDlg, IDC_UPDATE_NOTES);

                if (hwndNotes && g_notesDisplayText) {
                    SetWindowSubclass(hwndNotes, NotesControlProc, 0, 0);
                    SetProp(hwndNotes, L"ScrollPos", (HANDLE)0);
                    SetProp(hwndNotes, L"MarkdownLinks", (HANDLE)g_notesLinks);
                    SetProp(hwndNotes, L"LinkCount", (HANDLE)(INT_PTR)g_notesLinkCount);

                    HDC hdc = GetDC(hwndNotes);
                    HFONT hFont = (HFONT)SendMessage(hwndNotes, WM_GETFONT, 0, 0);
                    if (!hFont) hFont = GetStockObject(DEFAULT_GUI_FONT);
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                    RECT rect;
                    GetClientRect(hwndNotes, &rect);
                    rect.left += 5;
                    rect.top += 5;
                    rect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + 5;

                    RECT drawRect = rect;
                    g_textHeight = CalculateMarkdownTextHeight(hdc, g_notesDisplayText,
                                                                g_notesHeadings, g_notesHeadingCount,
                                                                g_notesStyles, g_notesStyleCount,
                                                                g_notesListItems, g_notesListItemCount,
                                                                g_notesBlockquotes, g_notesBlockquoteCount,
                                                                drawRect);

                    SelectObject(hdc, hOldFont);
                    ReleaseDC(hwndNotes, hdc);

                    int clientHeight = rect.bottom - rect.top;
                    SetProp(hwndNotes, L"ScrollMax", (HANDLE)(INT_PTR)g_textHeight);
                    SetProp(hwndNotes, L"ScrollPage", (HANDLE)(INT_PTR)clientHeight);
                }

                SetDlgItemTextW(hwndDlg, IDYES, GetLocalizedString(NULL, L"Update Now"));
                SetDlgItemTextW(hwndDlg, IDNO, GetLocalizedString(NULL, L"Later"));
                SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"Update Available"));

                ShowWindow(GetDlgItem(hwndDlg, IDYES), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDNO), SW_SHOW);
                ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_HIDE);
            }
            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO) {
                FreeMarkdownLinks(g_notesLinks, g_notesLinkCount);
                g_notesLinks = NULL;
                g_notesLinkCount = 0;
                if (g_notesHeadings) {
                    free(g_notesHeadings);
                    g_notesHeadings = NULL;
                }
                g_notesHeadingCount = 0;
                if (g_notesStyles) {
                    free(g_notesStyles);
                    g_notesStyles = NULL;
                }
                g_notesStyleCount = 0;
                if (g_notesListItems) {
                    free(g_notesListItems);
                    g_notesListItems = NULL;
                }
                g_notesListItemCount = 0;
                if (g_notesBlockquotes) {
                    free(g_notesBlockquotes);
                    g_notesBlockquotes = NULL;
                }
                g_notesBlockquoteCount = 0;
                if (g_notesDisplayText) {
                    free(g_notesDisplayText);
                    g_notesDisplayText = NULL;
                }
                g_textHeight = 0;
                
                /* Store the result and notify parent before destroying */
                HWND hwndParent = GetParent(hwndDlg);
                int result = LOWORD(wParam);
                DestroyWindow(hwndDlg);
                
                /* If user chose to update, trigger the update process */
                if (result == IDYES && hwndParent) {
                    PostMessage(hwndParent, WM_DIALOG_UPDATE, IDYES, 0);
                }
                return TRUE;
            }
            break;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_UPDATE_NOTES) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;

                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
                HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

                HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
                FillRect(hdcMem, &rect, hBrush);
                DeleteObject(hBrush);

                if (g_notesDisplayText) {
                    int scrollPos = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollPos");
                    int scrollMax = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollMax");
                    int scrollPage = (int)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ScrollPage");
                    BOOL thumbDragging = (BOOL)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ThumbDragging");
                    BOOL thumbHovered = (BOOL)(INT_PTR)GetProp(lpDrawItem->hwndItem, L"ThumbHovered");

                    SetBkMode(hdcMem, TRANSPARENT);

                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

                    RECT drawRect = rect;
                    drawRect.left += 5;
                    drawRect.top += 5;
                    drawRect.right -= MODERN_SCROLLBAR_WIDTH + MODERN_SCROLLBAR_MARGIN + 5;

                    POINT oldOrg;
                    SetViewportOrgEx(hdcMem, 0, -scrollPos, &oldOrg);

                    RenderMarkdownText(hdcMem, g_notesDisplayText, g_notesLinks, g_notesLinkCount,
                                       g_notesHeadings, g_notesHeadingCount,
                                       g_notesStyles, g_notesStyleCount,
                                       g_notesListItems, g_notesListItemCount,
                                       g_notesBlockquotes, g_notesBlockquoteCount,
                                       drawRect, MARKDOWN_DEFAULT_LINK_COLOR, MARKDOWN_DEFAULT_TEXT_COLOR);

                    SetViewportOrgEx(hdcMem, oldOrg.x, oldOrg.y, NULL);

                    SelectObject(hdcMem, hOldFont);

                    if (scrollMax > scrollPage) {
                        RECT thumbRect;
                        CalculateScrollbarThumbRect(rect, scrollPos, scrollMax, scrollPage, &thumbRect);

                        if (!IsRectEmpty(&thumbRect)) {
                            COLORREF thumbColor = thumbDragging ? MODERN_SCROLLBAR_THUMB_DRAG_COLOR :
                                                  (thumbHovered ? MODERN_SCROLLBAR_THUMB_HOVER_COLOR :
                                                   MODERN_SCROLLBAR_THUMB_COLOR);
                            DrawRoundedRect(hdcMem, thumbRect, 4, thumbColor);
                        }
                    }
                }

                BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hdcMem, 0, 0, SRCCOPY);

                SelectObject(hdcMem, hbmOld);
                DeleteObject(hbmMem);
                DeleteDC(hdcMem);

                return TRUE;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                FreeMarkdownLinks(g_notesLinks, g_notesLinkCount);
                g_notesLinks = NULL;
                g_notesLinkCount = 0;
                if (g_notesHeadings) {
                    free(g_notesHeadings);
                    g_notesHeadings = NULL;
                }
                g_notesHeadingCount = 0;
                if (g_notesStyles) {
                    free(g_notesStyles);
                    g_notesStyles = NULL;
                }
                g_notesStyleCount = 0;
                if (g_notesListItems) {
                    free(g_notesListItems);
                    g_notesListItems = NULL;
                }
                g_notesListItemCount = 0;
                if (g_notesBlockquotes) {
                    free(g_notesBlockquotes);
                    g_notesBlockquotes = NULL;
                }
                g_notesBlockquoteCount = 0;
                if (g_notesDisplayText) {
                    free(g_notesDisplayText);
                    g_notesDisplayText = NULL;
                }
                g_textHeight = 0;
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;

        case WM_CLOSE:
            FreeMarkdownLinks(g_notesLinks, g_notesLinkCount);
            g_notesLinks = NULL;
            g_notesLinkCount = 0;
            if (g_notesHeadings) {
                free(g_notesHeadings);
                g_notesHeadings = NULL;
            }
            g_notesHeadingCount = 0;
            if (g_notesStyles) {
                free(g_notesStyles);
                g_notesStyles = NULL;
            }
            g_notesStyleCount = 0;
            if (g_notesListItems) {
                free(g_notesListItems);
                g_notesListItems = NULL;
            }
            g_notesListItemCount = 0;
            if (g_notesBlockquotes) {
                free(g_notesBlockquotes);
                g_notesBlockquotes = NULL;
            }
            g_notesBlockquoteCount = 0;
            if (g_notesDisplayText) {
                free(g_notesDisplayText);
                g_notesDisplayText = NULL;
            }
            g_textHeight = 0;
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstance(DIALOG_INSTANCE_UPDATE);
            g_hwndUpdateDialog = NULL;
            break;
    }
    return FALSE;
}

/** @brief Update error dialog */
INT_PTR CALLBACK UpdateErrorDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            Dialog_RegisterInstance(DIALOG_INSTANCE_UPDATE_ERROR, hwndDlg);
            InitializeDialog(hwndDlg, IDD_UPDATE_ERROR_DIALOG);
            if (lParam) {
                SetDlgItemTextW(hwndDlg, IDC_UPDATE_ERROR_TEXT, (const wchar_t*)lParam);
            }
            return TRUE;
            
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                EndDialog(hwndDlg, IDCANCEL);
                return TRUE;
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                EndDialog(hwndDlg, IDOK);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstance(DIALOG_INSTANCE_UPDATE_ERROR);
            break;
    }
    return FALSE;
}

/** @brief No update dialog */
static char g_noUpdateVersion[64] = {0};

INT_PTR CALLBACK NoUpdateDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            Dialog_RegisterInstance(DIALOG_INSTANCE_NO_UPDATE, hwndDlg);
            g_hwndNoUpdateDialog = hwndDlg;
            
            InitializeDialog(hwndDlg, IDD_NO_UPDATE_DIALOG);
            
            if (g_noUpdateVersion[0]) {
                const wchar_t* baseText = GetDialogLocalizedString(IDD_NO_UPDATE_DIALOG, IDC_NO_UPDATE_TEXT);
                if (!baseText) baseText = L"You are already using the latest version!";
                
                wchar_t fullMessage[256];
                StringCbPrintfW(fullMessage, sizeof(fullMessage), L"%s\n%s %hs",
                    baseText,
                    GetLocalizedString(NULL, L"Current version:"),
                    g_noUpdateVersion);
                SetDlgItemTextW(hwndDlg, IDC_NO_UPDATE_TEXT, fullMessage);
            }
            return TRUE;
        }
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            return TRUE;
            
        case WM_DESTROY:
            Dialog_UnregisterInstance(DIALOG_INSTANCE_NO_UPDATE);
            g_hwndNoUpdateDialog = NULL;
            break;
    }
    return FALSE;
}

void ShowExitMessageDialog(HWND hwnd) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_EXIT_MSG)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_EXIT_MSG);
        SetForegroundWindow(existing);
        return;
    }
    
    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_EXIT_DIALOG), hwnd, ExitMsgDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

int ShowUpdateNotification(HWND hwnd, const char* currentVersion, const char* latestVersion,
                                  const char* downloadUrl, const char* releaseNotes) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_UPDATE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_UPDATE);
        SetForegroundWindow(existing);
        return IDNO;
    }
    
    /* Store version info for modeless dialog */
    g_updateVersionInfo.currentVersion = currentVersion;
    g_updateVersionInfo.latestVersion = latestVersion;
    g_updateVersionInfo.downloadUrl = downloadUrl;
    g_updateVersionInfo.releaseNotes = releaseNotes;
    
    /* Store download URL copy for async update handling */
    if (downloadUrl) {
        strncpy(g_downloadUrlCopy, downloadUrl, sizeof(g_downloadUrlCopy) - 1);
        g_downloadUrlCopy[sizeof(g_downloadUrlCopy) - 1] = '\0';
    }
    
    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_DIALOG), 
                          hwnd, UpdateDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
    
    /* Return IDNO since we can't block for result in modeless mode */
    return IDNO;
}

void ShowUpdateErrorDialog(HWND hwnd, const wchar_t* errorMsg) {
    /* Don't show blocking error dialog for update failures
     * Update checks should be transparent to the user - failures should be silent
     * Only log errors for troubleshooting purposes */
    (void)hwnd;
    
    /* Log the error for debugging */
    char errorMsgUtf8[512] = {0};
    if (errorMsg) {
        WideCharToMultiByte(CP_UTF8, 0, errorMsg, -1, errorMsgUtf8, sizeof(errorMsgUtf8), NULL, NULL);
        LOG_WARNING("Update check failed: %s", errorMsgUtf8);
    }
    
    /* Original blocking dialog implementation commented out:
    DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_UPDATE_ERROR_DIALOG), 
                   hwnd, UpdateErrorDlgProc, (LPARAM)errorMsg);
    */
}

void ShowNoUpdateDialog(HWND hwnd, const char* currentVersion) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_NO_UPDATE)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_NO_UPDATE);
        SetForegroundWindow(existing);
        return;
    }
    
    /* Store version for modeless dialog */
    if (currentVersion) {
        strncpy(g_noUpdateVersion, currentVersion, sizeof(g_noUpdateVersion) - 1);
        g_noUpdateVersion[sizeof(g_noUpdateVersion) - 1] = '\0';
    }
    
    HWND hwndDlg = CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_NO_UPDATE_DIALOG), 
                   hwnd, NoUpdateDlgProc);
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    }
}

/**
 * @brief Get the pending update download URL
 * @return Pointer to stored download URL, or NULL if none
 */
const char* GetPendingUpdateDownloadUrl(void) {
    if (g_downloadUrlCopy[0] != '\0') {
        return g_downloadUrlCopy;
    }
    return NULL;
}

/**
 * @brief Trigger update download and exit
 * @param hwnd Parent window handle
 */
void TriggerUpdateDownload(HWND hwnd) {
    const char* url = GetPendingUpdateDownloadUrl();
    if (url && url[0] != '\0') {
        LOG_INFO("User chose to update now (from modeless dialog)");
        
        /* Open browser with download URL */
        wchar_t* urlW = Utf8ToWideAlloc(url);
        if (urlW) {
            ShellExecuteW(NULL, L"open", urlW, NULL, NULL, SW_SHOWNORMAL);
            free(urlW);
        }
        
        /* Show exit message and quit */
        ShowExitMessageDialog(hwnd);
        PostQuitMessage(0);
    }
}

/* ============================================================================
 * Thread-safe update result storage
 * ============================================================================ */

static char g_storedCurrentVersion[64] = {0};
static char g_storedLatestVersion[64] = {0};
static char g_storedDownloadUrl[512] = {0};
static char g_storedReleaseNotes[16384] = {0};
static BOOL g_storedHasUpdate = FALSE;

void StoreUpdateResult(BOOL hasUpdate, const char* currentVersion, const char* latestVersion,
                       const char* downloadUrl, const char* releaseNotes) {
    g_storedHasUpdate = hasUpdate;
    
    if (currentVersion) {
        strncpy(g_storedCurrentVersion, currentVersion, sizeof(g_storedCurrentVersion) - 1);
        g_storedCurrentVersion[sizeof(g_storedCurrentVersion) - 1] = '\0';
    }
    
    if (latestVersion) {
        strncpy(g_storedLatestVersion, latestVersion, sizeof(g_storedLatestVersion) - 1);
        g_storedLatestVersion[sizeof(g_storedLatestVersion) - 1] = '\0';
    }
    
    if (downloadUrl) {
        strncpy(g_storedDownloadUrl, downloadUrl, sizeof(g_storedDownloadUrl) - 1);
        g_storedDownloadUrl[sizeof(g_storedDownloadUrl) - 1] = '\0';
    }
    
    if (releaseNotes) {
        strncpy(g_storedReleaseNotes, releaseNotes, sizeof(g_storedReleaseNotes) - 1);
        g_storedReleaseNotes[sizeof(g_storedReleaseNotes) - 1] = '\0';
    }
}

void ShowStoredUpdateDialog(HWND hwnd) {
    ShowUpdateNotification(hwnd, g_storedCurrentVersion, g_storedLatestVersion,
                          g_storedDownloadUrl, g_storedReleaseNotes);
}

void ShowStoredNoUpdateDialog(HWND hwnd) {
    ShowNoUpdateDialog(hwnd, g_storedCurrentVersion);
}
