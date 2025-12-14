/**
 * @file dialog_plugin_security.c
 * @brief Plugin security confirmation dialog
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../../resource/resource.h"
#include "dialog/dialog_info.h"
#include "dialog/dialog_common.h"
#include "utils/string_convert.h"
#include "markdown/markdown_parser.h"
#include "language.h"
#include "log.h"

/* Dialog state */
static wchar_t* g_displayText = NULL;
static MarkdownLink* g_links = NULL;
static int g_linkCount = 0;
static MarkdownHeading* g_headings = NULL;
static int g_headingCount = 0;
static MarkdownStyle* g_styles = NULL;
static int g_styleCount = 0;
static MarkdownListItem* g_listItems = NULL;
static int g_listItemCount = 0;
static MarkdownBlockquote* g_blockquotes = NULL;
static int g_blockquoteCount = 0;

/* Plugin info passed to dialog */
static char g_pluginPath[MAX_PATH] = {0};
static char g_pluginName[128] = {0};

/* Reentrancy guard - use atomic for thread safety */
static volatile LONG g_dialogActive = 0;

/**
 * @brief Set plugin info for dialog
 * @param pluginPath Full path to plugin file
 * @param pluginName Display name of plugin
 */
static void SetPluginSecurityDialogInfo(const char* pluginPath, const char* pluginName) {
    if (pluginPath) {
        strncpy(g_pluginPath, pluginPath, sizeof(g_pluginPath) - 1);
        g_pluginPath[sizeof(g_pluginPath) - 1] = '\0';
    } else {
        g_pluginPath[0] = '\0';  /* Clear if NULL */
    }
    
    if (pluginName) {
        strncpy(g_pluginName, pluginName, sizeof(g_pluginName) - 1);
        g_pluginName[sizeof(g_pluginName) - 1] = '\0';
    } else {
        g_pluginName[0] = '\0';  /* Clear if NULL */
    }
}

/**
 * @brief Cleanup markdown rendering resources
 */
static void CleanupMarkdownResources(void) {
    FreeMarkdownLinks(g_links, g_linkCount);
    g_links = NULL;
    g_linkCount = 0;
    
    if (g_headings) { free(g_headings); g_headings = NULL; }
    g_headingCount = 0;
    
    if (g_styles) { free(g_styles); g_styles = NULL; }
    g_styleCount = 0;
    
    if (g_listItems) { free(g_listItems); g_listItems = NULL; }
    g_listItemCount = 0;
    
    if (g_blockquotes) { free(g_blockquotes); g_blockquotes = NULL; }
    g_blockquoteCount = 0;
    
    if (g_displayText) { free(g_displayText); g_displayText = NULL; }
}

/**
 * @brief Plugin security dialog procedure
 */
static INT_PTR CALLBACK PluginSecurityDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            /* Set dialog title and button texts */
            SetWindowTextW(hwndDlg, GetLocalizedString(NULL, L"PluginSecDialogTitle"));
            SetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_CANCEL_BTN, GetLocalizedString(NULL, L"PluginSecBtnCancel"));
            SetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_RUN_ONCE_BTN, GetLocalizedString(NULL, L"PluginSecBtnRunOnce"));
            SetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_TRUST_BTN, GetLocalizedString(NULL, L"PluginSecBtnTrustRun"));
            
            /* Auto-resize buttons based on text width */
            HDC hdc = GetDC(hwndDlg);
            HFONT hFont = (HFONT)SendMessage(hwndDlg, WM_GETFONT, 0, 0);
            HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
            
            /* Get actual dialog client rect size */
            RECT dialogRect;
            GetClientRect(hwndDlg, &dialogRect);
            int dialogWidth = dialogRect.right;
            int dialogHeight = dialogRect.bottom;
            
            /* Button layout parameters */
            int btnSpacing = 5;
            int btnPaddingX = 24;
            int btnHeight = 23;  /* Standard height */
            int btnY = dialogHeight - btnHeight - 10;  /* 10px from bottom */
            int rightMargin = 10;
            
            /* Measure each button text */
            wchar_t cancelText[128], runOnceText[128], trustText[128];
            GetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_CANCEL_BTN, cancelText, 128);
            GetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_RUN_ONCE_BTN, runOnceText, 128);
            GetDlgItemTextW(hwndDlg, IDC_PLUGIN_SECURITY_TRUST_BTN, trustText, 128);
            
            SIZE cancelSize, runOnceSize, trustSize;
            GetTextExtentPoint32W(hdc, cancelText, (int)wcslen(cancelText), &cancelSize);
            GetTextExtentPoint32W(hdc, runOnceText, (int)wcslen(runOnceText), &runOnceSize);
            GetTextExtentPoint32W(hdc, trustText, (int)wcslen(trustText), &trustSize);
            
            int cancelWidth = cancelSize.cx + btnPaddingX;
            int runOnceWidth = runOnceSize.cx + btnPaddingX;
            int trustWidth = trustSize.cx + btnPaddingX;
            
            if (cancelWidth < 60) cancelWidth = 60;
            if (runOnceWidth < 60) runOnceWidth = 60;
            if (trustWidth < 60) trustWidth = 60;
            
            SelectObject(hdc, hOldFont);
            ReleaseDC(hwndDlg, hdc);
            
            /* Calculate positions from right to left */
            int trustX = dialogWidth - rightMargin - trustWidth;
            int runOnceX = trustX - btnSpacing - runOnceWidth;
            int cancelX = runOnceX - btnSpacing - cancelWidth;
            
            /* Set button positions */
            SetWindowPos(GetDlgItem(hwndDlg, IDC_PLUGIN_SECURITY_CANCEL_BTN), NULL, 
                       cancelX, btnY, cancelWidth, btnHeight, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwndDlg, IDC_PLUGIN_SECURITY_RUN_ONCE_BTN), NULL, 
                       runOnceX, btnY, runOnceWidth, btnHeight, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwndDlg, IDC_PLUGIN_SECURITY_TRUST_BTN), NULL, 
                       trustX, btnY, trustWidth, btnHeight, SWP_NOZORDER);
            
            /* Build security message with localized strings */
            wchar_t messageWide[4096];
            int written = swprintf(messageWide, 4096,
                L"<md>\n"
                L"%s\n\n"
                L"%s\n"
                L"`%hs`\n\n"
                L"%s\n"
                L"- %s\n"
                L"- %s (%s)\n"
                L"- %s\n"
                L"- %s\n\n"
                L"%s\n"
                L"- %s\n"
                L"- %s\n"
                L"- %s\n"
                L"- %s\n"
                L"</md>",
                GetLocalizedString(NULL, L"PluginSecTitle"),
                GetLocalizedString(NULL, L"PluginSecLocation"),
                g_pluginPath,
                GetLocalizedString(NULL, L"PluginSecWhatToKnow"),
                GetLocalizedString(NULL, L"PluginSecPermDefault"),
                GetLocalizedString(NULL, L"PluginSecPermAdmin"),
                GetLocalizedString(NULL, L"PluginSecUACPrompt"),
                GetLocalizedString(NULL, L"PluginSecScriptFile"),
                GetLocalizedString(NULL, L"PluginSecReview"),
                GetLocalizedString(NULL, L"PluginSecTips"),
                GetLocalizedString(NULL, L"PluginSecTip1"),
                GetLocalizedString(NULL, L"PluginSecTip2"),
                GetLocalizedString(NULL, L"PluginSecTip3"),
                GetLocalizedString(NULL, L"PluginSecTip4")
            );
            
            /* Check for truncation */
            if (written < 0 || written >= 4096) {
                LOG_WARNING("Plugin security message truncated (length: %d)", written);
            }
            
            /* Parse markdown */
            ParseMarkdownLinks(messageWide, &g_displayText, &g_links, &g_linkCount,
                             &g_headings, &g_headingCount,
                             &g_styles, &g_styleCount,
                             &g_listItems, &g_listItemCount,
                             &g_blockquotes, &g_blockquoteCount);
            
            /* Center dialog */
            Dialog_CenterOnPrimaryScreen(hwndDlg);
            
            return TRUE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PLUGIN_SECURITY_TRUST_BTN: {
                    /* "Trust & Run" button always adds to trust list */
                    CleanupMarkdownResources();
                    EndDialog(hwndDlg, IDYES);
                    return TRUE;
                }
                
                case IDC_PLUGIN_SECURITY_RUN_ONCE_BTN: {
                    /* "Run Once" button does not add to trust list */
                    CleanupMarkdownResources();
                    EndDialog(hwndDlg, IDOK);
                    return TRUE;
                }
                
                case IDC_PLUGIN_SECURITY_CANCEL_BTN:
                case IDCANCEL: {
                    CleanupMarkdownResources();
                    EndDialog(hwndDlg, IDCANCEL);
                    return TRUE;
                }
                
                case IDC_PLUGIN_SECURITY_TEXT:
                    if (HIWORD(wParam) == STN_CLICKED) {
                        POINT pt;
                        GetCursorPos(&pt);
                        ScreenToClient(GetDlgItem(hwndDlg, IDC_PLUGIN_SECURITY_TEXT), &pt);
                        
                        if (HandleMarkdownClick(g_links, g_linkCount, pt)) {
                            return TRUE;
                        }
                    }
                    return TRUE;
            }
            break;
        
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_PLUGIN_SECURITY_TEXT) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;
                
                /* Fill with dialog background color (same as font license dialog) */
                HBRUSH hBrush = GetSysColorBrush(COLOR_BTNFACE);
                FillRect(hdc, &rect, hBrush);
                
                if (g_displayText) {
                    SetBkMode(hdc, TRANSPARENT);
                    
                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    /* Add margins for content */
                    RECT drawRect = rect;
                    drawRect.left += 10;
                    drawRect.top += 10;
                    drawRect.right -= 10;
                    drawRect.bottom -= 10;
                    
                    RenderMarkdownText(hdc, g_displayText, g_links, g_linkCount,
                                     g_headings, g_headingCount,
                                     g_styles, g_styleCount,
                                     g_listItems, g_listItemCount,
                                     g_blockquotes, g_blockquoteCount,
                                     drawRect, MARKDOWN_DEFAULT_LINK_COLOR, MARKDOWN_DEFAULT_TEXT_COLOR);
                    
                    SelectObject(hdc, hOldFont);
                }
                
                return TRUE;
            }
            break;
        }
        
        case WM_CLOSE:
            CleanupMarkdownResources();
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Show plugin security confirmation dialog
 * @param hwndParent Parent window handle
 * @param pluginPath Full path to plugin file
 * @param pluginName Display name of plugin
 * @return IDYES (trust and remember), IDOK (run once), or IDCANCEL
 */
INT_PTR ShowPluginSecurityDialog(HWND hwndParent, const char* pluginPath, const char* pluginName) {
    /* Prevent reentrancy - only one dialog can be active at a time */
    /* Use atomic compare-exchange to prevent race condition */
    if (InterlockedCompareExchange(&g_dialogActive, 1, 0) != 0) {
        LOG_WARNING("Plugin security dialog already active, rejecting request");
        return IDCANCEL;
    }
    
    SetPluginSecurityDialogInfo(pluginPath, pluginName);
    
    INT_PTR result = DialogBoxW(GetModuleHandle(NULL),
                                MAKEINTRESOURCE(IDD_PLUGIN_SECURITY_DIALOG),
                                hwndParent,
                                PluginSecurityDlgProc);
    
    InterlockedExchange(&g_dialogActive, 0);
    
    /* Check for dialog creation failure */
    if (result == -1) {
        LOG_ERROR("Failed to create plugin security dialog (error: %lu)", GetLastError());
        return IDCANCEL;  /* Treat dialog failure as cancellation for safety */
    }
    
    return result;
}
