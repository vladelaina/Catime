/**
 * @file dialog_plugin_security.c
 * @brief Plugin security confirmation dialog (modeless)
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

/* Plugin info passed to dialog - accessible for result handling */
static char g_pluginPath[MAX_PATH] = {0};
static char g_pluginName[128] = {0};
static int g_pluginIndex = -1;
static char g_pluginHash[65] = {0};  /* SHA256 hash at dialog show time */

/* Parent window handle for posting results */
static HWND g_pluginSecurityParent = NULL;

/**
 * @brief Set plugin info for dialog
 * @param pluginPath Full path to plugin file
 * @param pluginName Display name of plugin
 * @param pluginIndex Index of plugin in plugin list
 */
static void SetPluginSecurityDialogInfo(const char* pluginPath, const char* pluginName, int pluginIndex) {
    if (pluginPath) {
        strncpy(g_pluginPath, pluginPath, sizeof(g_pluginPath) - 1);
        g_pluginPath[sizeof(g_pluginPath) - 1] = '\0';
    } else {
        g_pluginPath[0] = '\0';
    }
    
    if (pluginName) {
        strncpy(g_pluginName, pluginName, sizeof(g_pluginName) - 1);
        g_pluginName[sizeof(g_pluginName) - 1] = '\0';
    } else {
        g_pluginName[0] = '\0';
    }
    
    g_pluginIndex = pluginIndex;
}

/**
 * @brief Get pending plugin path (for result handling)
 */
const char* GetPendingPluginPath(void) {
    return g_pluginPath;
}

/**
 * @brief Get pending plugin index (for result handling)
 */
int GetPendingPluginIndex(void) {
    return g_pluginIndex;
}

/**
 * @brief Clear pending plugin info
 */
void ClearPendingPluginInfo(void) {
    g_pluginPath[0] = '\0';
    g_pluginName[0] = '\0';
    g_pluginIndex = -1;
    g_pluginHash[0] = '\0';
}

/**
 * @brief Set the hash of plugin file at dialog show time
 */
void SetPendingPluginHash(const char* hash) {
    if (hash && strlen(hash) == 64) {
        strncpy(g_pluginHash, hash, sizeof(g_pluginHash) - 1);
        g_pluginHash[sizeof(g_pluginHash) - 1] = '\0';
    } else {
        g_pluginHash[0] = '\0';
    }
}

/**
 * @brief Get the hash of plugin file at dialog show time
 */
const char* GetPendingPluginHash(void) {
    return g_pluginHash;
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
            Dialog_RegisterInstance(DIALOG_INSTANCE_PLUGIN_SECURITY, hwndDlg);
            
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
            int btnHeight = 23;
            int btnY = dialogHeight - btnHeight - 10;
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
                    CleanupMarkdownResources();
                    if (g_pluginSecurityParent) {
                        PostMessage(g_pluginSecurityParent, WM_DIALOG_PLUGIN_SECURITY, IDYES, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }
                
                case IDC_PLUGIN_SECURITY_RUN_ONCE_BTN: {
                    CleanupMarkdownResources();
                    if (g_pluginSecurityParent) {
                        PostMessage(g_pluginSecurityParent, WM_DIALOG_PLUGIN_SECURITY, IDOK, 0);
                    }
                    DestroyWindow(hwndDlg);
                    return TRUE;
                }
                
                case IDC_PLUGIN_SECURITY_CANCEL_BTN:
                case IDCANCEL: {
                    CleanupMarkdownResources();
                    if (g_pluginSecurityParent) {
                        PostMessage(g_pluginSecurityParent, WM_DIALOG_PLUGIN_SECURITY, IDCANCEL, 0);
                    }
                    DestroyWindow(hwndDlg);
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
        
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CleanupMarkdownResources();
                if (g_pluginSecurityParent) {
                    PostMessage(g_pluginSecurityParent, WM_DIALOG_PLUGIN_SECURITY, IDCANCEL, 0);
                }
                DestroyWindow(hwndDlg);
                return TRUE;
            }
            break;
        
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT)lParam;
            if (lpDrawItem->CtlID == IDC_PLUGIN_SECURITY_TEXT) {
                HDC hdc = lpDrawItem->hDC;
                RECT rect = lpDrawItem->rcItem;
                
                HBRUSH hBrush = GetSysColorBrush(COLOR_BTNFACE);
                FillRect(hdc, &rect, hBrush);
                
                if (g_displayText) {
                    SetBkMode(hdc, TRANSPARENT);
                    
                    HFONT hFont = (HFONT)SendMessage(lpDrawItem->hwndItem, WM_GETFONT, 0, 0);
                    if (!hFont) {
                        hFont = GetStockObject(DEFAULT_GUI_FONT);
                    }
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
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
        
        case WM_DESTROY:
            CleanupMarkdownResources();
            Dialog_UnregisterInstance(DIALOG_INSTANCE_PLUGIN_SECURITY);
            break;
        
        case WM_CLOSE:
            CleanupMarkdownResources();
            if (g_pluginSecurityParent) {
                PostMessage(g_pluginSecurityParent, WM_DIALOG_PLUGIN_SECURITY, IDCANCEL, 0);
            }
            DestroyWindow(hwndDlg);
            return TRUE;
    }
    
    return FALSE;
}

/**
 * @brief Show plugin security confirmation dialog (modeless)
 * @param hwndParent Parent window handle
 * @param pluginPath Full path to plugin file
 * @param pluginName Display name of plugin
 * @param pluginIndex Index of plugin in plugin list
 * 
 * Results are sent via WM_DIALOG_PLUGIN_SECURITY message:
 * - wParam = IDYES (trust and remember)
 * - wParam = IDOK (run once)
 * - wParam = IDCANCEL (cancelled)
 */
void ShowPluginSecurityDialog(HWND hwndParent, const char* pluginPath, const char* pluginName, int pluginIndex) {
    if (Dialog_IsOpen(DIALOG_INSTANCE_PLUGIN_SECURITY)) {
        HWND existing = Dialog_GetInstance(DIALOG_INSTANCE_PLUGIN_SECURITY);
        SetForegroundWindow(existing);
        return;
    }
    
    g_pluginSecurityParent = hwndParent;
    SetPluginSecurityDialogInfo(pluginPath, pluginName, pluginIndex);
    
    HWND hwndDlg = CreateDialogW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_PLUGIN_SECURITY_DIALOG),
        hwndParent,
        PluginSecurityDlgProc
    );
    
    if (hwndDlg) {
        ShowWindow(hwndDlg, SW_SHOW);
    } else {
        LOG_ERROR("Failed to create plugin security dialog (error: %lu)", GetLastError());
        ClearPendingPluginInfo();
    }
}
