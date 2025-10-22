/**
 * @file window_procedure.c
 * @brief Main window procedure handling all messages and user interactions
 * Implements comprehensive message processing for timer, hotkeys, dialogs, and menu commands
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <dwmapi.h>
#include "../resource/resource.h"
#include <winnls.h>
#include <commdlg.h>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlguid.h>
#include "../include/language.h"
#include "../include/font.h"
#include "../include/color.h"
#include "../include/tray.h"
#include "../include/tray_menu.h"
#include "../include/timer.h"
#include "../include/window.h"
#include "../include/startup.h"
#include "../include/config.h"
#include "../include/window_procedure.h"
#include "../include/window_events.h"
#include "../include/drag_scale.h"
#include "../include/drawing.h"
#include "../include/timer_events.h"
#include "../include/tray_events.h"
#include "../include/dialog_procedure.h"
#include "../include/pomodoro.h"
#include "../include/update_checker.h"
#include "../include/async_update_checker.h"
#include "../include/hotkey.h"
#include "../include/notification.h"
#include "../include/cli.h"
#include "../include/tray_animation.h"

/** @brief Represents a file or folder entry for sorting animation menus. */
typedef struct {
    wchar_t name[MAX_PATH];
    char rel_path_utf8[MAX_PATH]; /** Relative path from animations root */
    BOOL is_dir;
} AnimationEntry;

/** @brief Natural string compare for wide-char names: compare numeric substrings by value. */
static int NaturalCompareW(const wchar_t* a, const wchar_t* b) {
    const wchar_t* pa = a;
    const wchar_t* pb = b;
    while (*pa && *pb) {
        if (iswdigit(*pa) && iswdigit(*pb)) {
            const wchar_t* za = pa; while (*za == L'0') za++;
            const wchar_t* zb = pb; while (*zb == L'0') zb++;
            /** Primary: more leading zeros first */
            size_t leadA = (size_t)(za - pa);
            size_t leadB = (size_t)(zb - pb);
            if (leadA != leadB) return (leadA > leadB) ? -1 : 1;
            const wchar_t* ea = za; while (iswdigit(*ea)) ea++;
            const wchar_t* eb = zb; while (iswdigit(*eb)) eb++;
            size_t lena = (size_t)(ea - za);
            size_t lenb = (size_t)(eb - zb);
            if (lena != lenb) return (lena < lenb) ? -1 : 1;
            int dcmp = wcsncmp(za, zb, lena);
            if (dcmp != 0) return (dcmp < 0) ? -1 : 1;
            pa = ea;
            pb = eb;
            continue;
        }
        wchar_t ca = towlower(*pa);
        wchar_t cb = towlower(*pb);
        if (ca != cb) return (ca < cb) ? -1 : 1;
        pa++; pb++;
    }
    if (*pa) return 1;
    if (*pb) return -1;
    return 0;
}

/** @brief qsort comparator for AnimationEntry, directories first, then natural order. */
static int CompareAnimationEntries(const void* a, const void* b) {
    const AnimationEntry* entryA = (const AnimationEntry*)a;
    const AnimationEntry* entryB = (const AnimationEntry*)b;
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir; // Directories first
    }
    return NaturalCompareW(entryA->name, entryB->name);
}

/** @brief Checks if a folder contains no sub-folders or animated images, making it a leaf. */
static BOOL IsAnimationLeafFolderW(const wchar_t* folderPathW) {
    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return TRUE; // Empty is a leaf

    BOOL hasSubItems = FALSE;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            hasSubItems = TRUE;
            break;
        }
        wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
        if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0)) {
            hasSubItems = TRUE;
            break;
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);
    
    return !hasSubItems;
}

/**
 * @brief Recursively find animation by menu ID and trigger preview
 * @param folderPathW Wide-char path to search folder
 * @param folderPathUtf8 UTF-8 relative path for animation reference
 * @param nextIdPtr Pointer to next available menu ID
 * @param targetId Target menu ID to find
 * @return TRUE if animation found and preview started, FALSE otherwise
 */
static BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* folderPathUtf8, UINT* nextIdPtr, UINT targetId) {
    AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_TRAY_FRAMES);
    if (!entries) return FALSE;
    int entryCount = 0;

    wchar_t wSearch[MAX_PATH] = {0};
    _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
    
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(wSearch, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(entries);
        return FALSE;
    }

    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
        if (entryCount >= MAX_TRAY_FRAMES) break;

        AnimationEntry* e = &entries[entryCount];
        e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
        e->name[MAX_PATH - 1] = L'\0';

        char itemUtf8[MAX_PATH] = {0};
        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
        if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
            _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
        } else {
            _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
        }
        
        if (e->is_dir) {
            entryCount++;
        } else {
            wchar_t* ext = wcsrchr(e->name, L'.');
            if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                        _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                        _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                        _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                        _wcsicmp(ext, L".tiff") == 0)) {
                entryCount++;
            }
        }
    } while (FindNextFileW(hFind, &ffd));
    FindClose(hFind);

    if (entryCount == 0) {
        free(entries);
        return FALSE;
    }
    qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);

    for (int i = 0; i < entryCount; ++i) {
        AnimationEntry* e = &entries[i];
        if (e->is_dir) {
            wchar_t wSubFolderPath[MAX_PATH] = {0};
            _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);

            if (IsAnimationLeafFolderW(wSubFolderPath)) {
                if (*nextIdPtr == targetId) {
                    StartAnimationPreview(e->rel_path_utf8);
                    free(entries);
                    return TRUE;
                }
                (*nextIdPtr)++;
            } else {
                if (FindAnimationByIdRecursive(wSubFolderPath, e->rel_path_utf8, nextIdPtr, targetId)) {
                    free(entries);
                    return TRUE;
                }
            }
        } else {
            if (*nextIdPtr == targetId) {
                StartAnimationPreview(e->rel_path_utf8);
                free(entries);
                return TRUE;
            }
            (*nextIdPtr)++;
        }
    }
    free(entries);
    return FALSE;
}

/** @brief Global input text buffer for dialog operations */
extern wchar_t inputText[256];
extern int elapsed_time;
extern int message_shown;
extern TimeFormatType CLOCK_TIME_FORMAT;
extern BOOL CLOCK_SHOW_MILLISECONDS;
extern BOOL IS_MILLISECONDS_PREVIEWING;
extern BOOL PREVIEW_SHOW_MILLISECONDS;

extern void ShowNotification(HWND hwnd, const wchar_t* message);
extern void PauseMediaPlayback(void);

/**
 * @brief Recursively find font file by ID in fonts folder (Unicode-safe)
 * @param folderPathW Wide-char path to search folder
 * @param targetId Target font menu ID
 * @param currentId Pointer to current ID counter
 * @param foundRelativePathW Output buffer for relative font path
 * @param fontsFolderRootW Root fonts folder path for relative path calculation
 * @return TRUE if font found, FALSE otherwise
 */
static BOOL FindFontByIdRecursiveW(const wchar_t* folderPathW, int targetId, int* currentId,
                                   wchar_t* foundRelativePathW, const wchar_t* fontsFolderRootW) {
    wchar_t* searchPathW = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));
    WIN32_FIND_DATAW* findDataW = (WIN32_FIND_DATAW*)malloc(sizeof(WIN32_FIND_DATAW));
    if (!searchPathW || !findDataW) {
        if (searchPathW) free(searchPathW);
        if (findDataW) free(findDataW);
        return FALSE;
    }

    _snwprintf_s(searchPathW, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);

    HANDLE hFind = FindFirstFileW(searchPathW, findDataW);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            /** Skip . and .. entries */
            if (wcscmp(findDataW->cFileName, L".") == 0 || wcscmp(findDataW->cFileName, L"..") == 0) {
                continue;
            }

            wchar_t fullItemPathW[MAX_PATH];
            _snwprintf_s(fullItemPathW, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, findDataW->cFileName);

            /** Handle regular font files */
            if (!(findDataW->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                wchar_t* extW = wcsrchr(findDataW->cFileName, L'.');
                if (extW && (_wcsicmp(extW, L".ttf") == 0 || _wcsicmp(extW, L".otf") == 0)) {
                    if (*currentId == targetId) {
                        /** Calculate relative path from fonts folder root */
                        size_t rootLen = wcslen(fontsFolderRootW);
                        if (_wcsnicmp(fullItemPathW, fontsFolderRootW, rootLen) == 0) {
                            const wchar_t* relativeW = fullItemPathW + rootLen;
                            if (*relativeW == L'\\') relativeW++;
                            wcsncpy(foundRelativePathW, relativeW, MAX_PATH - 1);
                            foundRelativePathW[MAX_PATH - 1] = L'\0';
                        } else {
                            /** Fallback to filename only */
                            wcsncpy(foundRelativePathW, findDataW->cFileName, MAX_PATH - 1);
                            foundRelativePathW[MAX_PATH - 1] = L'\0';
                        }
                        FindClose(hFind);
                        free(searchPathW);
                        free(findDataW);
                        return TRUE;
                    }
                    (*currentId)++;
                }
            } else {
                /** Handle subdirectories recursively */
                if (FindFontByIdRecursiveW(fullItemPathW, targetId, currentId, foundRelativePathW, fontsFolderRootW)) {
                    FindClose(hFind);
                    free(searchPathW);
                    free(findDataW);
                    return TRUE;
                }
            }
        } while (FindNextFileW(hFind, findDataW));
        FindClose(hFind);
    }
    free(searchPathW);
    free(findDataW);
    return FALSE;
}

/** @brief Pomodoro timer configuration */
extern int POMODORO_TIMES[10];
extern int POMODORO_TIMES_COUNT;
extern int current_pomodoro_time_index;
extern int complete_pomodoro_cycles;

extern BOOL ShowInputDialog(HWND hwnd, wchar_t* text);

extern void WriteConfigPomodoroTimeOptions(int* times, int count);

/**
 * @brief Input dialog parameter structure for custom dialogs
 * Encapsulates dialog state for modal input operations
 */
typedef struct {
    const wchar_t* title;          /**< Dialog window title */
    const wchar_t* prompt;         /**< Prompt text displayed to user */
    const wchar_t* defaultText;    /**< Default input text */
    wchar_t* result;               /**< Buffer to store user input */
    size_t maxLen;                 /**< Maximum length of input */
} INPUTBOX_PARAMS;

extern void ShowPomodoroLoopDialog(HWND hwndParent);

extern void OpenUserGuide(void);

/**
 * @brief Get %LOCALAPPDATA%\Catime\resources\fonts in wide-char using config path (Unicode-safe)
 */
static BOOL GetFontsFolderWideFromConfig(wchar_t* out, size_t size) {
    if (!out || size == 0) return FALSE;
    char configPathUtf8[MAX_PATH] = {0};
    GetConfigPath(configPathUtf8, MAX_PATH);
    if (configPathUtf8[0] == '\0') return FALSE;
    wchar_t wConfigPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wConfigPath, MAX_PATH);
    wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
    if (!lastSep) return FALSE;
    *lastSep = L'\0';
    wchar_t wFonts[MAX_PATH] = {0};
    _snwprintf_s(wFonts, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts", wConfigPath);
    wcsncpy(out, wFonts, size - 1);
    out[size - 1] = L'\0';
    return TRUE;
}

extern void OpenSupportPage(void);

extern void OpenFeedbackPage(void);

/**
 * @brief Check if string is NULL, empty or contains only whitespace characters
 * @param str String to check (can be NULL)
 * @return TRUE if string is NULL, empty or contains only whitespace, FALSE otherwise
 */
static BOOL isAllSpacesOnly(const wchar_t* str) {
    if (!str || str[0] == L'\0') {
        return TRUE;
    }
    for (int i = 0; str[i]; i++) {
        if (!iswspace(str[i])) {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Dialog procedure for custom input box dialog
 * @param hwndDlg Dialog window handle
 * @param uMsg Message identifier
 * @param wParam Message parameter
 * @param lParam Message parameter containing INPUTBOX_PARAMS on init
 * @return Message processing result
 *
 * Handles initialization, input validation, and OK/Cancel responses
 */
INT_PTR CALLBACK InputBoxProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static wchar_t* result;
    static size_t maxLen;
    
    switch (uMsg) {
        case WM_INITDIALOG: {
            /** Extract parameters and initialize dialog controls */
            INPUTBOX_PARAMS* params = (INPUTBOX_PARAMS*)lParam;
            result = params->result;
            maxLen = params->maxLen;
            
            SetWindowTextW(hwndDlg, params->title);
            
            SetDlgItemTextW(hwndDlg, IDC_STATIC_PROMPT, params->prompt);
            
            SetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, params->defaultText);
            
            /** Select all text for easy replacement */
            SendDlgItemMessageW(hwndDlg, IDC_EDIT_INPUT, EM_SETSEL, 0, -1);
            
            SetFocus(GetDlgItem(hwndDlg, IDC_EDIT_INPUT));
            
            /** Move dialog to primary screen */
            MoveDialogToPrimaryScreen(hwndDlg);
            
            return FALSE;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    /** Retrieve user input and close dialog */
                    GetDlgItemTextW(hwndDlg, IDC_EDIT_INPUT, result, (int)maxLen);
                    EndDialog(hwndDlg, TRUE);
                    return TRUE;
                }
                
                case IDCANCEL:
                    /** Cancel operation without saving input */
                    EndDialog(hwndDlg, FALSE);
                    return TRUE;
            }
            break;
    }
    
    return FALSE;
}

/**
 * @brief Display custom input dialog box with specified parameters
 * @param hwndParent Parent window handle
 * @param title Dialog window title
 * @param prompt Text prompt for user input
 * @param defaultText Default value in input field
 * @param result Buffer to store user input
 * @param maxLen Maximum length of input buffer
 * @return TRUE if user clicked OK, FALSE if cancelled
 *
 * Creates modal dialog for user text input with customizable appearance
 */
BOOL InputBox(HWND hwndParent, const wchar_t* title, const wchar_t* prompt, 
              const wchar_t* defaultText, wchar_t* result, size_t maxLen) {
    INPUTBOX_PARAMS params;
    params.title = title;
    params.prompt = prompt;
    params.defaultText = defaultText;
    params.result = result;
    params.maxLen = maxLen;
    
    return DialogBoxParamW(GetModuleHandle(NULL), 
                          MAKEINTRESOURCEW(IDD_INPUTBOX), 
                          hwndParent, 
                          InputBoxProc, 
                          (LPARAM)&params) == TRUE;
}

/**
 * @brief Gracefully exit the application
 * @param hwnd Main window handle
 *
 * Removes tray icon and posts quit message to message loop
 */
void ExitProgram(HWND hwnd) {
    RemoveTrayIcon();

    PostQuitMessage(0);
}

/** @brief Global hotkey identifiers for system-wide keyboard shortcuts */
#define HOTKEY_ID_SHOW_TIME       100    /**< Toggle system time display */
#define HOTKEY_ID_COUNT_UP        101    /**< Start count-up timer */
#define HOTKEY_ID_COUNTDOWN       102    /**< Start default countdown */
#define HOTKEY_ID_QUICK_COUNTDOWN1 103   /**< Quick countdown option 1 */
#define HOTKEY_ID_QUICK_COUNTDOWN2 104   /**< Quick countdown option 2 */
#define HOTKEY_ID_QUICK_COUNTDOWN3 105   /**< Quick countdown option 3 */
#define HOTKEY_ID_POMODORO        106    /**< Start Pomodoro timer */
#define HOTKEY_ID_TOGGLE_VISIBILITY 107  /**< Toggle window visibility */
#define HOTKEY_ID_EDIT_MODE       108    /**< Toggle edit mode */
#define HOTKEY_ID_PAUSE_RESUME    109    /**< Pause/resume current timer */
#define HOTKEY_ID_RESTART_TIMER   110    /**< Restart current timer */
#define HOTKEY_ID_CUSTOM_COUNTDOWN 111   /**< Custom countdown input */

/** @brief Timer ID for debouncing menu selection changes to avoid preview flicker. */
#define IDT_MENU_DEBOUNCE 500

/**
 * @brief Helper function to register a single hotkey
 * @param hwnd Window handle to receive hotkey messages
 * @param hotkeyId Hotkey identifier
 * @param hotkeyValue Hotkey value (WORD containing VK and modifiers)
 * @return TRUE if registration successful, FALSE otherwise
 */
static BOOL RegisterSingleHotkey(HWND hwnd, int hotkeyId, WORD hotkeyValue) {
    if (hotkeyValue == 0) {
        return FALSE;
    }
    
    BYTE vk = LOBYTE(hotkeyValue);
    BYTE mod = HIBYTE(hotkeyValue);
    
    /** Convert modifier flags to Windows API format */
    UINT fsModifiers = 0;
    if (mod & HOTKEYF_ALT) fsModifiers |= MOD_ALT;
    if (mod & HOTKEYF_CONTROL) fsModifiers |= MOD_CONTROL;
    if (mod & HOTKEYF_SHIFT) fsModifiers |= MOD_SHIFT;
    
    return RegisterHotKey(hwnd, hotkeyId, fsModifiers, vk);
}

/**
 * @brief Register all global hotkeys with the system
 * @param hwnd Window handle to receive hotkey messages
 * @return TRUE if any hotkeys were successfully registered, FALSE if none
 *
 * Loads hotkey configuration and attempts registration with conflict handling
 */
BOOL RegisterGlobalHotkeys(HWND hwnd) {
    UnregisterGlobalHotkeys(hwnd);
    
    /** Hotkey configuration variables */
    WORD showTimeHotkey = 0;
    WORD countUpHotkey = 0;
    WORD countdownHotkey = 0;
    WORD quickCountdown1Hotkey = 0;
    WORD quickCountdown2Hotkey = 0;
    WORD quickCountdown3Hotkey = 0;
    WORD pomodoroHotkey = 0;
    WORD toggleVisibilityHotkey = 0;
    WORD editModeHotkey = 0;
    WORD pauseResumeHotkey = 0;
    WORD restartTimerHotkey = 0;
    WORD customCountdownHotkey = 0;
    
    /** Load hotkey configuration from config file */
    ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                     &quickCountdown1Hotkey, &quickCountdown2Hotkey, &quickCountdown3Hotkey,
                     &pomodoroHotkey, &toggleVisibilityHotkey, &editModeHotkey,
                     &pauseResumeHotkey, &restartTimerHotkey);
    
    BOOL success = FALSE;
    BOOL configChanged = FALSE;
    
    /** Hotkey registration table */
    struct {
        int id;
        WORD* value;
    } hotkeys[] = {
        {HOTKEY_ID_SHOW_TIME, &showTimeHotkey},
        {HOTKEY_ID_COUNT_UP, &countUpHotkey},
        {HOTKEY_ID_COUNTDOWN, &countdownHotkey},
        {HOTKEY_ID_QUICK_COUNTDOWN1, &quickCountdown1Hotkey},
        {HOTKEY_ID_QUICK_COUNTDOWN2, &quickCountdown2Hotkey},
        {HOTKEY_ID_QUICK_COUNTDOWN3, &quickCountdown3Hotkey},
        {HOTKEY_ID_POMODORO, &pomodoroHotkey},
        {HOTKEY_ID_TOGGLE_VISIBILITY, &toggleVisibilityHotkey},
        {HOTKEY_ID_EDIT_MODE, &editModeHotkey},
        {HOTKEY_ID_PAUSE_RESUME, &pauseResumeHotkey},
        {HOTKEY_ID_RESTART_TIMER, &restartTimerHotkey}
    };
    
    /** Register each hotkey with conflict detection */
    for (int i = 0; i < sizeof(hotkeys) / sizeof(hotkeys[0]); i++) {
        if (*hotkeys[i].value != 0) {
            if (RegisterSingleHotkey(hwnd, hotkeys[i].id, *hotkeys[i].value)) {
                success = TRUE;
            } else {
                /** Clear conflicting hotkey configuration */
                *hotkeys[i].value = 0;
                configChanged = TRUE;
            }
        }
    }
    
    if (configChanged) {
        WriteConfigHotkeys(showTimeHotkey, countUpHotkey, countdownHotkey,
                           quickCountdown1Hotkey, quickCountdown2Hotkey, quickCountdown3Hotkey,
                           pomodoroHotkey, toggleVisibilityHotkey, editModeHotkey,
                           pauseResumeHotkey, restartTimerHotkey);
        
        if (customCountdownHotkey == 0) {
            WriteConfigKeyValue("HOTKEY_CUSTOM_COUNTDOWN", "None");
        }
    }
    
    /** Handle custom countdown hotkey separately */
    ReadCustomCountdownHotkey(&customCountdownHotkey);
    
    if (customCountdownHotkey != 0) {
        if (RegisterSingleHotkey(hwnd, HOTKEY_ID_CUSTOM_COUNTDOWN, customCountdownHotkey)) {
            success = TRUE;
        } else {
            customCountdownHotkey = 0;
            configChanged = TRUE;
        }
    }
    
    return success;
}

/**
 * @brief Unregister all global hotkeys from the system
 * @param hwnd Window handle that owns the hotkeys
 *
 * Safely removes all registered hotkeys to prevent conflicts on exit
 */
void UnregisterGlobalHotkeys(HWND hwnd) {
    UnregisterHotKey(hwnd, HOTKEY_ID_SHOW_TIME);
    UnregisterHotKey(hwnd, HOTKEY_ID_COUNT_UP);
    UnregisterHotKey(hwnd, HOTKEY_ID_COUNTDOWN);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN1);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN2);
    UnregisterHotKey(hwnd, HOTKEY_ID_QUICK_COUNTDOWN3);
    UnregisterHotKey(hwnd, HOTKEY_ID_POMODORO);
    UnregisterHotKey(hwnd, HOTKEY_ID_TOGGLE_VISIBILITY);
    UnregisterHotKey(hwnd, HOTKEY_ID_EDIT_MODE);
    UnregisterHotKey(hwnd, HOTKEY_ID_PAUSE_RESUME);
    UnregisterHotKey(hwnd, HOTKEY_ID_RESTART_TIMER);
    UnregisterHotKey(hwnd, HOTKEY_ID_CUSTOM_COUNTDOWN);
}

/**
 * @brief Main window procedure handling all window messages
 * @param hwnd Window handle receiving the message
 * @param msg Message identifier
 * @param wp First message parameter
 * @param lp Second message parameter
 * @return Result of message processing
 *
 * Central message dispatcher for timer, UI, hotkey, and system events
 */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static wchar_t time_text[50];
    UINT uID;
    UINT uMouseMsg;

    /** Handle taskbar recreation (e.g., Explorer restart) */
    if (msg == WM_TASKBARCREATED) {
        RecreateTaskbarIcon(hwnd, GetModuleHandle(NULL));
        return 0;
    }

    switch(msg)
    {
        /** Custom application messages */
        case WM_APP_SHOW_CLI_HELP: {
            ShowCliHelpDialog(hwnd);
            return 0;
        }
        case WM_APP_ANIM_SPEED_CHANGED: {
            extern void ReloadAnimationSpeedFromConfig(void);
            ReloadAnimationSpeedFromConfig();
            extern void TrayAnimation_RecomputeTimerDelay(void);
            TrayAnimation_RecomputeTimerDelay();
            return 0;
        }
        case WM_APP_ANIM_PATH_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);
            char value[MAX_PATH] = {0};
            ReadIniString("Animation", "ANIMATION_PATH", "__logo__", value, sizeof(value), config_path);
            extern void ApplyAnimationPathValueNoPersist(const char* value);
            ApplyAnimationPathValueNoPersist(value);
            return 0;
        }
        /** Thread-safe tray icon update from multimedia timer callback */
        case WM_USER + 100: {  /** WM_TRAY_UPDATE_ICON */
            if (TrayAnimation_HandleUpdateMessage()) {
                return 0;
            }
            break;
        }
        case WM_APP_DISPLAY_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);

            /** Track if any display property changed */
            BOOL displayChanged = FALSE;

            /** CLOCK_TEXT_COLOR */
            char newColor[32] = {0};
            ReadIniString(INI_SECTION_DISPLAY, "CLOCK_TEXT_COLOR", CLOCK_TEXT_COLOR, newColor, sizeof(newColor), config_path);
            if (strcmp(newColor, CLOCK_TEXT_COLOR) != 0) {
                strncpy(CLOCK_TEXT_COLOR, newColor, sizeof(CLOCK_TEXT_COLOR) - 1);
                CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
                displayChanged = TRUE;
            }

            /** CLOCK_BASE_FONT_SIZE */
            int newBaseSize = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_BASE_FONT_SIZE", CLOCK_BASE_FONT_SIZE, config_path);
            if (newBaseSize != CLOCK_BASE_FONT_SIZE && newBaseSize > 0) {
                CLOCK_BASE_FONT_SIZE = newBaseSize;
                displayChanged = TRUE;
            }

            /** FONT_FILE_NAME => skip entirely, font should remain loaded from startup/manual changes only */
            /** This prevents unnecessary font reloads when unrelated config changes trigger WM_APP_DISPLAY_CHANGED */

            /** Trigger repaint only if any display property actually changed */
            if (displayChanged) {
                InvalidateRect(hwnd, NULL, TRUE);
            }

            /** WINDOW_POS + SCALE + TOPMOST */
            if (!CLOCK_EDIT_MODE) {
                int posX = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_X", CLOCK_WINDOW_POS_X, config_path);
                int posY = ReadIniInt(INI_SECTION_DISPLAY, "CLOCK_WINDOW_POS_Y", CLOCK_WINDOW_POS_Y, config_path);
                char scaleStr[16] = {0};
                ReadIniString(INI_SECTION_DISPLAY, "WINDOW_SCALE", "1.62", scaleStr, sizeof(scaleStr), config_path);
                float newScale = (float)atof(scaleStr);
                BOOL newTopmost = ReadIniBool(INI_SECTION_DISPLAY, "WINDOW_TOPMOST", CLOCK_WINDOW_TOPMOST, config_path);

                BOOL posChanged = (posX != CLOCK_WINDOW_POS_X) || (posY != CLOCK_WINDOW_POS_Y);
                BOOL scaleChanged = (newScale > 0.0f && fabsf(newScale - CLOCK_WINDOW_SCALE) > 0.0001f);
                BOOL topChanged = (newTopmost != CLOCK_WINDOW_TOPMOST);

                if (scaleChanged) {
                    extern float CLOCK_FONT_SCALE_FACTOR;
                    CLOCK_WINDOW_SCALE = newScale;
                    CLOCK_FONT_SCALE_FACTOR = newScale;
                }

                if (posChanged || scaleChanged) {
                    SetWindowPos(hwnd, NULL,
                        posX,
                        posY,
                        (int)(CLOCK_BASE_WINDOW_WIDTH * CLOCK_WINDOW_SCALE),
                        (int)(CLOCK_BASE_WINDOW_HEIGHT * CLOCK_WINDOW_SCALE),
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    CLOCK_WINDOW_POS_X = posX;
                    CLOCK_WINDOW_POS_Y = posY;
                }

                if (topChanged) {
                    SetWindowTopmost(hwnd, newTopmost);
                }
            }
            return 0;
        }
        case WM_APP_TIMER_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);

            /** Reload timer-related settings from [Timer] */
            int newDefaultStart = ReadIniInt(INI_SECTION_TIMER, "CLOCK_DEFAULT_START_TIME", CLOCK_DEFAULT_START_TIME, config_path);

            BOOL newUse24 = ReadIniBool(INI_SECTION_TIMER, "CLOCK_USE_24HOUR", CLOCK_USE_24HOUR, config_path);
            BOOL newShowSeconds = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_SECONDS", CLOCK_SHOW_SECONDS, config_path);

            char timeFormat[32] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_FORMAT", "DEFAULT", timeFormat, sizeof(timeFormat), config_path);
            TimeFormatType newFormat = TIME_FORMAT_DEFAULT;
            if (strcmp(timeFormat, "ZERO_PADDED") == 0) newFormat = TIME_FORMAT_ZERO_PADDED;
            else if (strcmp(timeFormat, "FULL_PADDED") == 0) newFormat = TIME_FORMAT_FULL_PADDED;

            BOOL newShowMs = ReadIniBool(INI_SECTION_TIMER, "CLOCK_SHOW_MILLISECONDS", CLOCK_SHOW_MILLISECONDS, config_path);

            char options[256] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIME_OPTIONS", "1500,600,300", options, sizeof(options), config_path);

            char timeoutText[50] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_TEXT", "0", timeoutText, sizeof(timeoutText), config_path);

            char actionStr[32] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_ACTION", "MESSAGE", actionStr, sizeof(actionStr), config_path);

            char timeoutFile[MAX_PATH] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_FILE", "", timeoutFile, sizeof(timeoutFile), config_path);

            char websiteUtf8[MAX_PATH] = {0};
            ReadIniString(INI_SECTION_TIMER, "CLOCK_TIMEOUT_WEBSITE", "", websiteUtf8, sizeof(websiteUtf8), config_path);

            char startupMode[20] = {0};
            ReadIniString(INI_SECTION_TIMER, "STARTUP_MODE", CLOCK_STARTUP_MODE, startupMode, sizeof(startupMode), config_path);

            /** Track if any display-affecting settings changed */
            BOOL timerDisplayChanged = FALSE;
            
            /** Apply basic flags that affect display */
            if (newUse24 != CLOCK_USE_24HOUR) {
                CLOCK_USE_24HOUR = newUse24;
                timerDisplayChanged = TRUE;
            }
            if (newShowSeconds != CLOCK_SHOW_SECONDS) {
                CLOCK_SHOW_SECONDS = newShowSeconds;
                timerDisplayChanged = TRUE;
            }
            if (newFormat != CLOCK_TIME_FORMAT) {
                CLOCK_TIME_FORMAT = newFormat;
                timerDisplayChanged = TRUE;
            }

            /** Handle milliseconds interval change */
            if (newShowMs != CLOCK_SHOW_MILLISECONDS) {
                CLOCK_SHOW_MILLISECONDS = newShowMs;
                ResetTimerWithInterval(hwnd);
                timerDisplayChanged = TRUE;
            }

            /** Update default start time (runtime cache, no display impact) */
            CLOCK_DEFAULT_START_TIME = newDefaultStart;

            /** Update timeout action fields (no display impact) */
            strncpy(CLOCK_TIMEOUT_TEXT, timeoutText, sizeof(CLOCK_TIMEOUT_TEXT) - 1);
            CLOCK_TIMEOUT_TEXT[sizeof(CLOCK_TIMEOUT_TEXT) - 1] = '\0';

            if (strcmp(actionStr, "MESSAGE") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
            else if (strcmp(actionStr, "LOCK") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_LOCK;
            else if (strcmp(actionStr, "OPEN_FILE") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_FILE;
            else if (strcmp(actionStr, "SHOW_TIME") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SHOW_TIME;
            else if (strcmp(actionStr, "COUNT_UP") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_COUNT_UP;
            else if (strcmp(actionStr, "OPEN_WEBSITE") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_OPEN_WEBSITE;
            else if (strcmp(actionStr, "SLEEP") == 0) CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_SLEEP;
            else CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;

            memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
            strncpy(CLOCK_TIMEOUT_FILE_PATH, timeoutFile, sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1);
            CLOCK_TIMEOUT_FILE_PATH[sizeof(CLOCK_TIMEOUT_FILE_PATH) - 1] = '\0';

            if (websiteUtf8[0] != '\0') {
                MultiByteToWideChar(CP_UTF8, 0, websiteUtf8, -1, CLOCK_TIMEOUT_WEBSITE_URL, MAX_PATH);
            } else {
                CLOCK_TIMEOUT_WEBSITE_URL[0] = L'\0';
            }

            /** Re-parse time options (no display impact - only affects menu) */
            time_options_count = 0;
            memset(time_options, 0, sizeof(time_options));
            char *tok = strtok(options, ",");
            while (tok && time_options_count < MAX_TIME_OPTIONS) {
                while (*tok == ' ') tok++;
                time_options[time_options_count++] = atoi(tok);
                tok = strtok(NULL, ",");
            }

            /** Update startup mode string in memory (no display impact) */
            strncpy(CLOCK_STARTUP_MODE, startupMode, sizeof(CLOCK_STARTUP_MODE) - 1);
            CLOCK_STARTUP_MODE[sizeof(CLOCK_STARTUP_MODE) - 1] = '\0';

            /** Only repaint if timer display settings actually changed */
            if (timerDisplayChanged) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_APP_POMODORO_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);

            char pomodoroTimeOptions[256] = {0};
            ReadIniString(INI_SECTION_POMODORO, "POMODORO_TIME_OPTIONS", "1500,300,1500,600", pomodoroTimeOptions, sizeof(pomodoroTimeOptions), config_path);

            extern int POMODORO_WORK_TIME;
            extern int POMODORO_SHORT_BREAK;
            extern int POMODORO_LONG_BREAK;

            int tmp[10] = {0};
            int cnt = 0;
            char* tok = strtok(pomodoroTimeOptions, ",");
            while (tok && cnt < 10) {
                while (*tok == ' ') tok++;
                tmp[cnt++] = atoi(tok);
                tok = strtok(NULL, ",");
            }
            if (cnt > 0) POMODORO_WORK_TIME = tmp[0];
            if (cnt > 1) POMODORO_SHORT_BREAK = tmp[1];
            if (cnt > 2) POMODORO_LONG_BREAK = tmp[2];

            extern int POMODORO_LOOP_COUNT;
            POMODORO_LOOP_COUNT = ReadIniInt(INI_SECTION_POMODORO, "POMODORO_LOOP_COUNT", 1, config_path);
            if (POMODORO_LOOP_COUNT < 1) POMODORO_LOOP_COUNT = 1;
            return 0;
        }
        case WM_APP_NOTIFICATION_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);

            ReadNotificationMessagesConfig();
            ReadNotificationTimeoutConfig();
            ReadNotificationOpacityConfig();
            ReadNotificationTypeConfig();
            ReadNotificationSoundConfig();
            ReadNotificationVolumeConfig();
            ReadNotificationDisabledConfig();
            return 0;
        }
        case WM_APP_HOTKEYS_CHANGED: {
            WORD showTimeHotkey = 0, countUpHotkey = 0, countdownHotkey = 0;
            WORD quick1 = 0, quick2 = 0, quick3 = 0, pomodoro = 0;
            WORD toggle = 0, edit = 0, pauseResume = 0, restart = 0;
            ReadConfigHotkeys(&showTimeHotkey, &countUpHotkey, &countdownHotkey,
                              &quick1, &quick2, &quick3,
                              &pomodoro, &toggle, &edit, &pauseResume, &restart);
            RegisterGlobalHotkeys(hwnd);
            return 0;
        }
        case WM_APP_RECENTFILES_CHANGED: {
            LoadRecentFiles();
            /** If current timeout action is OPEN_FILE but the selected file is invalid or not in recents,
             *  auto-align to the first recent file and persist, so menu check and action stay consistent. */
            if (CLOCK_TIMEOUT_ACTION == TIMEOUT_ACTION_OPEN_FILE) {
                BOOL match = FALSE;
                for (int i = 0; i < CLOCK_RECENT_FILES_COUNT; ++i) {
                    if (strcmp(CLOCK_RECENT_FILES[i].path, CLOCK_TIMEOUT_FILE_PATH) == 0) {
                        match = TRUE; break;
                    }
                }
                wchar_t wSel[MAX_PATH] = {0};
                MultiByteToWideChar(CP_UTF8, 0, CLOCK_TIMEOUT_FILE_PATH, -1, wSel, MAX_PATH);
                if (GetFileAttributesW(wSel) == INVALID_FILE_ATTRIBUTES) {
                    match = FALSE;
                }
                if (!match && CLOCK_RECENT_FILES_COUNT > 0) {
                    WriteConfigTimeoutFile(CLOCK_RECENT_FILES[0].path);
                }
            }
            return 0;
        }
        case WM_APP_COLORS_CHANGED: {
            char config_path[MAX_PATH] = {0};
            GetConfigPath(config_path, MAX_PATH);
            /** Reload color options list */
            char colorOptions[1024] = {0};
            ReadIniString(INI_SECTION_COLORS, "COLOR_OPTIONS",
                          "#FFFFFF,#F9DB91,#F4CAE0,#FFB6C1,#A8E7DF,#A3CFB3,#92CBFC,#BDA5E7,#9370DB,#8C92CF,#72A9A5,#EB99A7,#EB96BD,#FFAE8B,#FF7F50,#CA6174",
                          colorOptions, sizeof(colorOptions), config_path);
            ClearColorOptions();
            char* tok = strtok(colorOptions, ",");
            while (tok) {
                while (*tok == ' ') tok++;
                AddColorOption(tok);
                tok = strtok(NULL, ",");
            }
            /** Reload percent icon colors */
            extern void ReadPercentIconColorsConfig(void);
            extern void TrayAnimation_UpdatePercentIconIfNeeded(void);
            ReadPercentIconColorsConfig();
            TrayAnimation_UpdatePercentIconIfNeeded();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        /** Inter-process communication for CLI arguments */
        case WM_COPYDATA: {
            PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lp;
            if (pcds && pcds->dwData == COPYDATA_ID_CLI_TEXT && pcds->lpData && pcds->cbData > 0) {
                const size_t maxLen = 255;
                char buf[256];
                size_t n = (pcds->cbData > maxLen) ? maxLen : pcds->cbData;
                memcpy(buf, pcds->lpData, n);
                buf[maxLen] = '\0';
                buf[n] = '\0';
                HandleCliArguments(hwnd, buf);
                return 0;
            }
            break;
        }
        
        /** Quick countdown selection from CLI or other sources */
        case WM_APP_QUICK_COUNTDOWN_INDEX: {
            int idx = (int)lp;
            if (idx >= 1) {
                StartQuickCountdownByIndex(hwnd, idx);
            } else {
                StartDefaultCountDown(hwnd);
            }
            return 0;
        }
        
        /** Window lifecycle events */
        case WM_CREATE: {
            RegisterGlobalHotkeys(hwnd);
            HandleWindowCreate(hwnd);
            extern void ConfigWatcher_Start(HWND hwnd);
            ConfigWatcher_Start(hwnd);
            break;
        }

        /** Cursor management for edit mode */
        case WM_SETCURSOR: {
            if (CLOCK_EDIT_MODE && LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            
            if (LOWORD(lp) == HTCLIENT || msg == CLOCK_WM_TRAYICON) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
            break;
        }

        /** Mouse interaction for window dragging */
        case WM_LBUTTONDOWN: {
            StartDragWindow(hwnd);
            break;
        }

        case WM_LBUTTONUP: {
            EndDragWindow(hwnd);
            break;
        }

        /** Mouse wheel scaling */
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            HandleScaleWindow(hwnd, delta);
            break;
        }

        /** Window dragging during mouse movement */
        case WM_MOUSEMOVE: {
            if (HandleDragWindow(hwnd)) {
                return 0;
            }
            break;
        }

        /** Window painting and rendering */
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            HandleWindowPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }
        
        /** Timer events for countdown/countup functionality */
        case WM_TIMER: {
            if (wp == IDT_MENU_DEBOUNCE) {
                KillTimer(hwnd, IDT_MENU_DEBOUNCE);
                extern void CancelAnimationPreview(void);
                CancelAnimationPreview();
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                    if (IS_PREVIEWING) {
                        CancelFontPreview();
                    }
                    IS_COLOR_PREVIEWING = FALSE;
                    IS_TIME_FORMAT_PREVIEWING = FALSE;
                    if (IS_MILLISECONDS_PREVIEWING) {
                        IS_MILLISECONDS_PREVIEWING = FALSE;
                        ResetTimerWithInterval(hwnd);
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                return 0;
            }
            if (HandleTimerEvent(hwnd, wp)) {
                break;
            }
            break;
        }
        
        /** Window destruction cleanup */
        case WM_DESTROY: {
            UnregisterGlobalHotkeys(hwnd);
            HandleWindowDestroy(hwnd);
            extern void ConfigWatcher_Stop(void);
            ConfigWatcher_Stop();
            return 0;
        }
        
        /** System tray icon messages */
        case CLOCK_WM_TRAYICON: {
            HandleTrayIconMessage(hwnd, (UINT)wp, (UINT)lp);
            break;
        }
        
        /** Menu and command message processing */
        case WM_COMMAND: {
            WORD cmd = LOWORD(wp);

            BOOL isAnimationSelectionCommand = 
                (cmd >= CLOCK_IDM_ANIMATIONS_BASE && cmd < CLOCK_IDM_ANIMATIONS_BASE + 1000) ||
                cmd == CLOCK_IDM_ANIMATIONS_USE_LOGO ||
                cmd == CLOCK_IDM_ANIMATIONS_USE_CPU ||
                cmd == CLOCK_IDM_ANIMATIONS_USE_MEM;
            
            if (isAnimationSelectionCommand) {
                KillTimer(hwnd, IDT_MENU_DEBOUNCE);
            }
            
            /** Always cancel any transient previews when a command is about to execute */
            // Only cancel if it's not a direct animation selection, to prevent flicker.
            if (!isAnimationSelectionCommand) {
                extern void CancelAnimationPreview(void);
                CancelAnimationPreview();
            }

            /** Handle color selection from menu (IDs 201+) */
            if (cmd >= 201 && cmd < 201 + COLOR_OPTIONS_COUNT) {
                int colorIndex = cmd - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(CLOCK_TEXT_COLOR, COLOR_OPTIONS[colorIndex].hexColor, 
                            sizeof(CLOCK_TEXT_COLOR) - 1);
                    CLOCK_TEXT_COLOR[sizeof(CLOCK_TEXT_COLOR) - 1] = '\0';
                    
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    WriteConfig(config_path);
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }
            switch (cmd) {
                /** Custom countdown timer setup with user input */
                case 101: {
                    /** Stop current time display if active */
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_LAST_TIME_UPDATE = 0;
                        KillTimer(hwnd, 1);
                    }
                    
                    /** Input validation loop until valid time or cancellation */
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);

                        /** Exit if user cancelled or provided empty input */
                        if (inputText[0] == L'\0') {
                            break;
                        }

                        /** Check for whitespace-only input */
                        if (isAllSpacesOnly(inputText)) {
                            break;
                        }

                        /** Parse and validate time input */
                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            /** Valid input: setup countdown timer */
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            CloseAllNotifications();
                            
                            /** Initialize countdown state */
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = total_seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            /** Reset timer flags and counters */
                            CLOCK_IS_PAUSED = FALSE;      
                            elapsed_time = 0;             
                            message_shown = FALSE;        
                            countup_message_shown = FALSE;
                            
                            /** Reset Pomodoro state if active */
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            /** Start countdown display and timer */
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            ResetTimerWithInterval(hwnd);
                            break;
                        } else {
                            /** Invalid input: show error dialog */
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }

                /** Quick countdown timer options (5min, 10min, etc.) - Legacy and dynamic */
                case 102: case 103: case 104: case 105: case 106:
                case 107: case 108:
                default: {
                    /** Determine index based on command ID */
                    int index = -1;
                    BOOL isQuickTimeOption = FALSE;
                    
                    if (cmd >= 102 && cmd <= 108) {
                        index = cmd - 102;
                        isQuickTimeOption = TRUE;
                    } else if (cmd >= CLOCK_IDM_QUICK_TIME_BASE && cmd < CLOCK_IDM_QUICK_TIME_BASE + MAX_TIME_OPTIONS) {
                        index = cmd - CLOCK_IDM_QUICK_TIME_BASE;
                        isQuickTimeOption = TRUE;
                    }
                    
                    if (isQuickTimeOption && index >= 0 && index < time_options_count) {
                        extern void StopNotificationSound(void);
                        StopNotificationSound();
                        
                        CloseAllNotifications();
                        
                        int seconds = time_options[index];
                        if (seconds > 0) {
                            KillTimer(hwnd, 1);
                            CLOCK_TOTAL_TIME = seconds;
                            countdown_elapsed_time = 0;
                            countdown_message_shown = FALSE;
                            CLOCK_COUNT_UP = FALSE;
                            CLOCK_SHOW_CURRENT_TIME = FALSE;
                            
                            CLOCK_IS_PAUSED = FALSE;
                            elapsed_time = 0;
                            message_shown = FALSE;
                            countup_message_shown = FALSE;
                            
                            if (current_pomodoro_phase != POMODORO_PHASE_IDLE) {
                                current_pomodoro_phase = POMODORO_PHASE_IDLE;
                                current_pomodoro_time_index = 0;
                                complete_pomodoro_cycles = 0;
                            }
                            
                            ShowWindow(hwnd, SW_SHOW);
                            InvalidateRect(hwnd, NULL, TRUE);
                            ResetTimerWithInterval(hwnd);
                        }
                        return 0;
                    }

                    /** Handle animation submenu commands */
                    if (HandleAnimationMenuCommand(hwnd, cmd)) {
                        return 0;
                    }

                /** Handle animation speed metric switching */
                if (cmd == CLOCK_IDM_ANIM_SPEED_MEMORY || cmd == CLOCK_IDM_ANIM_SPEED_CPU || cmd == CLOCK_IDM_ANIM_SPEED_TIMER) {
                    AnimationSpeedMetric m = ANIMATION_SPEED_MEMORY;
                    if (cmd == CLOCK_IDM_ANIM_SPEED_CPU) m = ANIMATION_SPEED_CPU;
                    else if (cmd == CLOCK_IDM_ANIM_SPEED_TIMER) m = ANIMATION_SPEED_TIMER;
                    /** Persist to config */
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    const char* metricStr = (m == ANIMATION_SPEED_CPU ? "CPU" : (m == ANIMATION_SPEED_TIMER ? "TIMER" : "MEMORY"));
                    WriteIniString("Animation", "ANIMATION_SPEED_METRIC", metricStr, config_path);
                    return 0;
                }

                    /** Handle dynamic advanced font selection from fonts folder */
                    else if (cmd >= 2000 && cmd < 3000) {
                        /** Get font filename from fonts folder by ID using wide-char recursive search */
                        wchar_t fontsFolderRootW[MAX_PATH] = {0};
                        if (GetFontsFolderWideFromConfig(fontsFolderRootW, MAX_PATH)) {

                            int currentIndex = 2000;
                            wchar_t foundRelativePathW[MAX_PATH] = {0};

                            if (FindFontByIdRecursiveW(fontsFolderRootW, cmd, &currentIndex, foundRelativePathW, fontsFolderRootW)) {
                                /** Convert relative wide path to UTF-8 for SwitchFont */
                                char foundFontNameUTF8[MAX_PATH];
                                WideCharToMultiByte(CP_UTF8, 0, foundRelativePathW, -1, foundFontNameUTF8, MAX_PATH, NULL, NULL);

                                HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                                if (SwitchFont(hInstance, foundFontNameUTF8)) {
                                    InvalidateRect(hwnd, NULL, TRUE);
                                    UpdateWindow(hwnd);
                                    return 0;
                                }
                            }
                        }
                        return 0;
                    }
                    break;
                }
                
                /** Application exit command */
                case 109: {
                    ExitProgram(hwnd);
                    break;
                }
                
                /** Modify quick countdown time options */
                case CLOCK_IDC_MODIFY_TIME_OPTIONS: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_SHORTCUT_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_SHORTCUT_DIALOG);

                        if (isAllSpacesOnly(inputText)) {
                            break;
                        }

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        
                        char* token = strtok(inputTextA, " ");
                        char options[256] = {0};
                        int valid = 1;
                        int count = 0;
                        
                        while (token && count < MAX_TIME_OPTIONS) {
                            int seconds = 0;

                            extern BOOL ParseTimeInput(const char* input, int* seconds);
                            if (!ParseTimeInput(token, &seconds) || seconds <= 0) {
                                valid = 0;
                                break;
                            }
                            
                            if (count > 0) {
                                strcat(options, ",");
                            }
                            
                            char secondsStr[32];
                            snprintf(secondsStr, sizeof(secondsStr), "%d", seconds);
                            strcat(options, secondsStr);
                            count++;
                            token = strtok(NULL, " ");
                        }

                        if (valid && count > 0) {
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            WriteConfigTimeOptions(options);
                            break;
                        } else {
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }
                
                /** Modify default startup countdown time */
                case CLOCK_IDC_MODIFY_DEFAULT_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));
                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_STARTUP_DIALOG), NULL, DlgProc, (LPARAM)CLOCK_IDD_STARTUP_DIALOG);

                        if (isAllSpacesOnly(inputText)) {
                            break;
                        }

                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            extern void StopNotificationSound(void);
                            StopNotificationSound();
                            
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            break;
                        } else {
                            ShowErrorDialog(hwnd);
                        }
                    }
                    break;
                }
                
                /** Reset application to default settings */
                case 200: {   
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    /** Stop all timers and unregister hotkeys */
                    KillTimer(hwnd, 1);
                    
                    UnregisterGlobalHotkeys(hwnd);
                    
                    extern int elapsed_time;
                    extern int countdown_elapsed_time;
                    extern int countup_elapsed_time;
                    extern BOOL message_shown;
                    extern BOOL countdown_message_shown;
                    extern BOOL countup_message_shown;
                    
                    extern BOOL InitializeHighPrecisionTimer(void);
                    extern void ResetTimer(void);
                    extern void ReadNotificationMessagesConfig(void);
                    
                    /** Reset to default 25-minute Pomodoro timer */
                    CLOCK_TOTAL_TIME = 25 * 60;
                    elapsed_time = 0;
                    countdown_elapsed_time = 0;
                    countup_elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    /** Reset all timer and display modes */
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    current_pomodoro_phase = POMODORO_PHASE_IDLE;
                    current_pomodoro_time_index = 0;
                    complete_pomodoro_cycles = 0;
                    
                    ResetTimer();
                    
                    CLOCK_EDIT_MODE = FALSE;
                    SetClickThrough(hwnd, TRUE);
                    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
                    
                    memset(CLOCK_TIMEOUT_FILE_PATH, 0, sizeof(CLOCK_TIMEOUT_FILE_PATH));
                    
                    /** Detect and set system default language */
                    AppLanguage defaultLanguage;
                    LANGID langId = GetUserDefaultUILanguage();
                    WORD primaryLangId = PRIMARYLANGID(langId);
                    WORD subLangId = SUBLANGID(langId);
                    
                    switch (primaryLangId) {
                        case LANG_CHINESE:
                            defaultLanguage = (subLangId == SUBLANG_CHINESE_SIMPLIFIED) ? 
                                             APP_LANG_CHINESE_SIMP : APP_LANG_CHINESE_TRAD;
                            break;
                        case LANG_SPANISH:
                            defaultLanguage = APP_LANG_SPANISH;
                            break;
                        case LANG_FRENCH:
                            defaultLanguage = APP_LANG_FRENCH;
                            break;
                        case LANG_GERMAN:
                            defaultLanguage = APP_LANG_GERMAN;
                            break;
                        case LANG_RUSSIAN:
                            defaultLanguage = APP_LANG_RUSSIAN;
                            break;
                        case LANG_PORTUGUESE:
                            defaultLanguage = APP_LANG_PORTUGUESE;
                            break;
                        case LANG_JAPANESE:
                            defaultLanguage = APP_LANG_JAPANESE;
                            break;
                        case LANG_KOREAN:
                            defaultLanguage = APP_LANG_KOREAN;
                            break;
                        default:
                            defaultLanguage = APP_LANG_ENGLISH;
                            break;
                    }
                    
                    if (CURRENT_LANGUAGE != defaultLanguage) {
                        CURRENT_LANGUAGE = defaultLanguage;
                    }
                    
                    /** Remove existing config and create fresh default */
                    char config_path[MAX_PATH];
                    GetConfigPath(config_path, MAX_PATH);
                    
                    wchar_t wconfig_path[MAX_PATH];
                    MultiByteToWideChar(CP_UTF8, 0, config_path, -1, wconfig_path, MAX_PATH);
                    
                    FILE* test = _wfopen(wconfig_path, L"r");
                    if (test) {
                        fclose(test);
                        remove(config_path);
                    }
                    
                    CreateDefaultConfig(config_path);
                    
                    ReadNotificationMessagesConfig();
                    
                    /** Extract embedded fonts to restore missing default fonts */
                    extern BOOL ExtractEmbeddedFontsToFolder(HINSTANCE hInstance);
                    ExtractEmbeddedFontsToFolder(GetModuleHandle(NULL));
                    
                    /** Reload font after config reset to ensure immediate effect */
                    extern BOOL LoadFontByNameAndGetRealName(HINSTANCE hInstance, const char* fontFileName, char* realFontName, size_t realFontNameSize);
                    char actualFontFileName[MAX_PATH];
                    const char* localappdata_prefix = "%LOCALAPPDATA%\\Catime\\resources\\fonts\\";
                    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
                        /** Extract just the filename for loading */
                        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
                        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
                        LoadFontByNameAndGetRealName(GetModuleHandle(NULL), actualFontFileName, FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME));
                    }
                    
                    /** Force immediate window refresh with new font */
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    /** Reset window and font scaling to defaults */
                    CLOCK_WINDOW_SCALE = 1.0f;
                    CLOCK_FONT_SCALE_FACTOR = 1.0f;
                    
                    /** Calculate optimal window size based on text dimensions */
                    HDC hdc = GetDC(hwnd);
                    
                    wchar_t fontNameW[256];
                    MultiByteToWideChar(CP_UTF8, 0, FONT_INTERNAL_NAME, -1, fontNameW, 256);
                    
                    HFONT hFont = CreateFontW(
                        -CLOCK_BASE_FONT_SIZE,   
                        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, fontNameW
                    );
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    char time_text[50];
                    FormatTime(CLOCK_TOTAL_TIME, time_text);
                    
                    wchar_t time_textW[50];
                    MultiByteToWideChar(CP_UTF8, 0, time_text, -1, time_textW, 50);
                    
                    SIZE textSize;
                    GetTextExtentPoint32(hdc, time_textW, wcslen(time_textW), &textSize);
                    
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hwnd, hdc);
                    
                    /** Calculate screen-proportional default scaling */
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    float defaultScale = (screenHeight * 0.03f) / 20.0f;
                    CLOCK_WINDOW_SCALE = defaultScale;
                    CLOCK_FONT_SCALE_FACTOR = defaultScale;
                    
                    SetWindowPos(hwnd, NULL, 
                        CLOCK_WINDOW_POS_X, CLOCK_WINDOW_POS_Y,
                        textSize.cx * defaultScale, textSize.cy * defaultScale,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    );
                    
                    /** Complete reset: show window, restart timer, refresh display */
                    ShowWindow(hwnd, SW_SHOW);
                    
                    ResetTimerWithInterval(hwnd);
                    
                    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
                    RedrawWindow(hwnd, NULL, NULL, 
                        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
                    
                    RegisterGlobalHotkeys(hwnd);
                    
                    return 0;
                }
                
                /** Timer control commands */
                case CLOCK_IDM_TIMER_PAUSE_RESUME: {
                    PauseResumeTimer(hwnd);
                    break;
                }
                case CLOCK_IDM_TIMER_RESTART: {
                    CloseAllNotifications();
                    RestartTimer(hwnd);
                    break;
                }
                
                /** Language selection menu handlers */
                case CLOCK_IDM_LANG_CHINESE:
                case CLOCK_IDM_LANG_CHINESE_TRAD:
                case CLOCK_IDM_LANG_ENGLISH:
                case CLOCK_IDM_LANG_SPANISH:
                case CLOCK_IDM_LANG_FRENCH:
                case CLOCK_IDM_LANG_GERMAN:
                case CLOCK_IDM_LANG_RUSSIAN:
                case CLOCK_IDM_LANG_PORTUGUESE:
                case CLOCK_IDM_LANG_JAPANESE:
                case CLOCK_IDM_LANG_KOREAN: {
                    /** Language mapping table */
                    static const struct {
                        WORD menuId;
                        AppLanguage language;
                    } languageMap[] = {
                        {CLOCK_IDM_LANG_CHINESE, APP_LANG_CHINESE_SIMP},
                        {CLOCK_IDM_LANG_CHINESE_TRAD, APP_LANG_CHINESE_TRAD},
                        {CLOCK_IDM_LANG_ENGLISH, APP_LANG_ENGLISH},
                        {CLOCK_IDM_LANG_SPANISH, APP_LANG_SPANISH},
                        {CLOCK_IDM_LANG_FRENCH, APP_LANG_FRENCH},
                        {CLOCK_IDM_LANG_GERMAN, APP_LANG_GERMAN},
                        {CLOCK_IDM_LANG_RUSSIAN, APP_LANG_RUSSIAN},
                        {CLOCK_IDM_LANG_PORTUGUESE, APP_LANG_PORTUGUESE},
                        {CLOCK_IDM_LANG_JAPANESE, APP_LANG_JAPANESE},
                        {CLOCK_IDM_LANG_KOREAN, APP_LANG_KOREAN}
                    };
                    
                    /** Find and set the selected language */
                    for (int i = 0; i < sizeof(languageMap) / sizeof(languageMap[0]); i++) {
                        if (cmd == languageMap[i].menuId) {
                            SetLanguage(languageMap[i].language);
                            WriteConfigLanguage(languageMap[i].language);
                            InvalidateRect(hwnd, NULL, TRUE);
                            
                            extern void UpdateTrayIcon(HWND hwnd);
                            UpdateTrayIcon(hwnd);
                            break;
                        }
                    }
                    break;
                }
                
                /** About dialog */
                case CLOCK_IDM_ABOUT:
                    ShowAboutDialog(hwnd);
                    return 0;
                
                /** Toggle window always-on-top state */
                case CLOCK_IDM_TOPMOST: {
                    BOOL newTopmost = !CLOCK_WINDOW_TOPMOST;
                    /** Unified path: write to INI and let watcher apply */
                    WriteConfigTopmost(newTopmost ? "TRUE" : "FALSE");
                    break;
                }
                
                /** Time format selection */
                case CLOCK_IDM_TIME_FORMAT_DEFAULT: {
                    WriteConfigTimeFormat(TIME_FORMAT_DEFAULT);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_ZERO_PADDED: {
                    WriteConfigTimeFormat(TIME_FORMAT_ZERO_PADDED);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_FULL_PADDED: {
                    WriteConfigTimeFormat(TIME_FORMAT_FULL_PADDED);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                case CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS: {
                    WriteConfigShowMilliseconds(!CLOCK_SHOW_MILLISECONDS);
                    /** Interval will be updated by watcher (WM_APP_TIMER_CHANGED) */
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Reset countdown timer to initial state */
                case CLOCK_IDM_COUNTDOWN_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    if (CLOCK_COUNT_UP) {
                        CLOCK_COUNT_UP = FALSE;
                    }
                    
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    ResetTimerWithInterval(hwnd);
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    HandleWindowReset(hwnd);
                    break;
                }
                
                /** Toggle edit mode for window positioning */
                case CLOCK_IDC_EDIT_MODE: {
                    if (CLOCK_EDIT_MODE) {
                        EndEditMode(hwnd);
                    } else {
                        StartEditMode(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
                
                /** Toggle window visibility */
                case CLOCK_IDC_TOGGLE_VISIBILITY: {
                    PostMessage(hwnd, WM_HOTKEY, HOTKEY_ID_TOGGLE_VISIBILITY, 0);
                    return 0;
                }
                
                /** Custom color picker dialog */
                case CLOCK_IDC_CUSTOMIZE_LEFT: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        char hex_color[10];
                        snprintf(hex_color, sizeof(hex_color), "#%02X%02X%02X", 
                                GetRValue(color), GetGValue(color), GetBValue(color));
                        WriteConfigColor(hex_color);
                    }
                    break;
                }
                

                
                /** Font license agreement dialog */
                case CLOCK_IDC_FONT_LICENSE_AGREE: {
                    extern INT_PTR ShowFontLicenseDialog(HWND hwndParent);
                    extern void SetFontLicenseAccepted(BOOL accepted);
                    extern void SetFontLicenseVersionAccepted(const char* version);
                    extern const char* GetCurrentFontLicenseVersion(void);
                    
                    INT_PTR result = ShowFontLicenseDialog(hwnd);
                    if (result == IDOK) {
                        /** User agreed to license terms, save to config with version and refresh menu */
                        SetFontLicenseAccepted(TRUE);
                        SetFontLicenseVersionAccepted(GetCurrentFontLicenseVersion());
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                /** Advanced font selection - open fonts folder in explorer */
                case CLOCK_IDC_FONT_ADVANCED: {
                    /** Build fonts folder path via config path to be Unicode-safe */
                    char configPathUtf8[MAX_PATH] = {0};
                    wchar_t wConfigPath[MAX_PATH] = {0};
                    GetConfigPath(configPathUtf8, MAX_PATH);
                    MultiByteToWideChar(CP_UTF8, 0, configPathUtf8, -1, wConfigPath, MAX_PATH);

                    /** Trim file name and append resources\fonts */
                    wchar_t* lastSep = wcsrchr(wConfigPath, L'\\');
                    if (lastSep) {
                        *lastSep = L'\0';
                        wchar_t wFontsFolderPath[MAX_PATH] = {0};
                        _snwprintf_s(wFontsFolderPath, MAX_PATH, _TRUNCATE, L"%s\\resources\\fonts", wConfigPath);

                        /** Ensure directory exists (Unicode-safe) */
                        SHCreateDirectoryExW(NULL, wFontsFolderPath, NULL);

                        /** Open fonts folder in Windows Explorer */
                        ShellExecuteW(hwnd, L"open", wFontsFolderPath, NULL, NULL, SW_SHOWNORMAL);
                    }
                    break;
                }
                
                /** Toggle between timer and current time display */
                case CLOCK_IDM_SHOW_CURRENT_TIME: {  
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    CLOCK_SHOW_CURRENT_TIME = !CLOCK_SHOW_CURRENT_TIME;
                    if (CLOCK_SHOW_CURRENT_TIME) {
                        /** Switch to current time mode with faster refresh */
                        ShowWindow(hwnd, SW_SHOW);  
                        
                        CLOCK_COUNT_UP = FALSE;
                        KillTimer(hwnd, 1);   
                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0;
                        CLOCK_LAST_TIME_UPDATE = time(NULL);
                        SetTimer(hwnd, 1, GetTimerInterval(), NULL);
                    } else {
                        /** Switch back to timer mode */
                        KillTimer(hwnd, 1);   

                        elapsed_time = 0;
                        countdown_elapsed_time = 0;
                        CLOCK_TOTAL_TIME = 0;
                        message_shown = 0;

                        ResetTimerWithInterval(hwnd); 
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Toggle 12/24 hour time format */
                case CLOCK_IDM_24HOUR_FORMAT: {  
                    WriteConfigKeyValue("CLOCK_USE_24HOUR", (!CLOCK_USE_24HOUR) ? "TRUE" : "FALSE");
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Toggle seconds display in time format */
                case CLOCK_IDM_SHOW_SECONDS: {  
                    WriteConfigKeyValue("CLOCK_SHOW_SECONDS", (!CLOCK_SHOW_SECONDS) ? "TRUE" : "FALSE");
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Recent file selection for timeout actions */
                case CLOCK_IDM_RECENT_FILE_1:
                case CLOCK_IDM_RECENT_FILE_2:
                case CLOCK_IDM_RECENT_FILE_3:
                case CLOCK_IDM_RECENT_FILE_4:
                case CLOCK_IDM_RECENT_FILE_5: {
                    int index = cmd - CLOCK_IDM_RECENT_FILE_1;
                    if (index < CLOCK_RECENT_FILES_COUNT) {
                        /** Validate selected recent file exists */
                        wchar_t wPath[MAX_PATH] = {0};
                        MultiByteToWideChar(CP_UTF8, 0, CLOCK_RECENT_FILES[index].path, -1, wPath, MAX_PATH);
                        
                        if (GetFileAttributesW(wPath) != INVALID_FILE_ATTRIBUTES) {
                            WriteConfigTimeoutFile(CLOCK_RECENT_FILES[index].path);
                            
                            SaveRecentFile(CLOCK_RECENT_FILES[index].path);
                        } else {
                            /** File no longer exists: show error and cleanup */
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                            
                            /** Reset timeout action to default via INI only; watcher will apply */
                            WriteConfigKeyValue("CLOCK_TIMEOUT_FILE", "");
                            WriteConfigTimeoutAction("MESSAGE");
                            
                            /** Remove invalid file from recent list */
                            for (int i = index; i < CLOCK_RECENT_FILES_COUNT - 1; i++) {
                                CLOCK_RECENT_FILES[i] = CLOCK_RECENT_FILES[i + 1];
                            }
                            CLOCK_RECENT_FILES_COUNT--;
                            
                            char config_path[MAX_PATH];
                            GetConfigPath(config_path, MAX_PATH);
                            WriteConfig(config_path);
                        }
                    }
                    break;
                }
                
                /** File browser dialog for timeout actions */
                case CLOCK_IDM_BROWSE_FILE: {
                    wchar_t szFile[MAX_PATH] = {0};
                    
                    OPENFILENAMEW ofn = {0};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
                    ofn.lpstrFilter = L"所有文件\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    
                    if (GetOpenFileNameW(&ofn)) {
                        char utf8Path[MAX_PATH * 3] = {0};
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
                        
                        if (GetFileAttributesW(szFile) != INVALID_FILE_ATTRIBUTES) {
                            /** Persist which file to open on timeout first */
                            WriteConfigTimeoutFile(utf8Path);
                            /** Update MRU list */
                            SaveRecentFile(utf8Path);
                            /** Ensure selected file remains consistent after MRU reload */
                            WriteConfigTimeoutFile(utf8Path);
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(L"所选文件不存在", L"Selected file does not exist"),
                                GetLocalizedString(L"错误", L"Error"),
                                MB_ICONERROR);
                        }
                    }
                    break;
                }
                
                /** Another file browser variant for timeout actions */
                case CLOCK_IDC_TIMEOUT_BROWSE: {
                    OPENFILENAMEW ofn;
                    wchar_t szFile[MAX_PATH] = L"";
                    
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameW(&ofn)) {
                        char utf8Path[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, szFile, -1, 
                                           utf8Path, 
                                           sizeof(utf8Path), 
                                           NULL, NULL);
                        
                        WriteConfigTimeoutFile(utf8Path);
                        
                        SaveRecentFile(utf8Path);
                    }
                    break;
                }
                
                /** Count-up timer mode controls */
                case CLOCK_IDM_COUNT_UP: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    CLOCK_COUNT_UP = !CLOCK_COUNT_UP;
                    if (CLOCK_COUNT_UP) {
                        /** Switch to count-up mode */
                        ShowWindow(hwnd, SW_SHOW);
                        
                        elapsed_time = 0;
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    }
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_START: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    if (!CLOCK_COUNT_UP) {
                        /** Start count-up timer from zero */
                        CLOCK_COUNT_UP = TRUE;
                        
                        countup_elapsed_time = 0;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    } else {
                        /** Toggle pause state if already counting up */
                        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                case CLOCK_IDM_COUNT_UP_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();

                    extern void ResetTimer(void);
                    ResetTimer();
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Configure startup countdown time */
                case CLOCK_IDC_SET_COUNTDOWN_TIME: {
                    while (1) {
                        memset(inputText, 0, sizeof(inputText));

                        DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), hwnd, DlgProc, (LPARAM)CLOCK_IDD_STARTUP_DIALOG);

                        if (inputText[0] == L'\0') {
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        }

                        int total_seconds = 0;

                        char inputTextA[256];
                        WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                        if (ParseInput(inputTextA, &total_seconds)) {
                            WriteConfigDefaultStartTime(total_seconds);
                            WriteConfigStartupMode("COUNTDOWN");
                            
                            HMENU hMenu = GetMenu(hwnd);
                            HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                            HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                            
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_CHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                            CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                            break;
                        } else {
                            MessageBoxW(hwnd, 
                                GetLocalizedString(
                                    L"25    = 25分钟\n"
                                    L"25h   = 25小时\n"
                                    L"25s   = 25秒\n"
                                    L"25 30 = 25分钟30秒\n"
                                    L"25 30m = 25小时30分钟\n"
                                    L"1 30 20 = 1小时30分钟20秒",
                                    
                                    L"25    = 25 minutes\n"
                                    L"25h   = 25 hours\n"
                                    L"25s   = 25 seconds\n"
                                    L"25 30 = 25 minutes 30 seconds\n"
                                    L"25 30m = 25 hours 30 minutes\n"
                                    L"1 30 20 = 1 hour 30 minutes 20 seconds"),
                                GetLocalizedString(L"输入格式", L"Input Format"),
                                MB_OK);
                        }
                    }
                    break;
                }
                
                /** Startup mode configuration options */
                case CLOCK_IDC_START_SHOW_TIME: {
                    WriteConfigStartupMode("SHOW_TIME");
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_CHECKED);
                    break;
                }
                case CLOCK_IDC_START_COUNT_UP: {
                    WriteConfigStartupMode("COUNT_UP");
                    break;
                }
                case CLOCK_IDC_START_NO_DISPLAY: {
                    WriteConfigStartupMode("NO_DISPLAY");
                    
                    HMENU hMenu = GetMenu(hwnd);
                    HMENU hTimeOptionsMenu = GetSubMenu(hMenu, GetMenuItemCount(hMenu) - 2);
                    HMENU hStartupSettingsMenu = GetSubMenu(hTimeOptionsMenu, 0);
                    
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_SET_COUNTDOWN_TIME, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_COUNT_UP, MF_UNCHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_NO_DISPLAY, MF_CHECKED);
                    CheckMenuItem(hStartupSettingsMenu, CLOCK_IDC_START_SHOW_TIME, MF_UNCHECKED);
                    break;
                }
                
                /** Toggle Windows startup shortcut */
                case CLOCK_IDC_AUTO_START: {
                    BOOL isEnabled = IsAutoStartEnabled();
                    if (isEnabled) {
                        if (RemoveShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_UNCHECKED);
                        }
                    } else {
                        if (CreateShortcut()) {
                            CheckMenuItem(GetMenu(hwnd), CLOCK_IDC_AUTO_START, MF_CHECKED);
                        }
                    }
                    break;
                }
                
                /** Color configuration dialogs */
                case CLOCK_IDC_COLOR_VALUE: {
                    DialogBoxW(GetModuleHandle(NULL), 
                             MAKEINTRESOURCEW(CLOCK_IDD_COLOR_DIALOG), 
                             hwnd, 
                             (DLGPROC)ColorDlgProc);
                    break;
                }
                case CLOCK_IDC_COLOR_PANEL: {
                    COLORREF color = ShowColorDialog(hwnd);
                    if (color != (COLORREF)-1) {
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                }
                
                /** Pomodoro timer functionality */
                case CLOCK_IDM_POMODORO_START: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    CloseAllNotifications();
                    
                    /** Ensure window is visible for Pomodoro session */
                    if (!IsWindowVisible(hwnd)) {
                        ShowWindow(hwnd, SW_SHOW);
                    }
                    
                    /** Initialize Pomodoro work session */
                    CLOCK_COUNT_UP = FALSE;
                    CLOCK_SHOW_CURRENT_TIME = FALSE;
                    countdown_elapsed_time = 0;
                    CLOCK_IS_PAUSED = FALSE;
                    
                    CLOCK_TOTAL_TIME = POMODORO_WORK_TIME;
                    
                    extern void InitializePomodoro(void);
                    InitializePomodoro();
                    
                    /** Force message notification for Pomodoro transitions */
                    TimeoutActionType originalAction = CLOCK_TIMEOUT_ACTION;
                    
                    CLOCK_TIMEOUT_ACTION = TIMEOUT_ACTION_MESSAGE;
                    
                    KillTimer(hwnd, 1);
                    ResetTimerWithInterval(hwnd);
                    
                    elapsed_time = 0;
                    message_shown = FALSE;
                    countdown_message_shown = FALSE;
                    countup_message_shown = FALSE;
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                }
                
                /** Configure Pomodoro session durations */
                case CLOCK_IDM_POMODORO_WORK:
                case CLOCK_IDM_POMODORO_BREAK:
                case CLOCK_IDM_POMODORO_LBREAK:
                /** Dynamic Pomodoro time configuration by index */
                case 600: case 601: case 602: case 603: case 604:
                case 605: case 606: case 607: case 608: case 609:
                    {
                        /** Map menu selection to Pomodoro phase index */
                        int selectedIndex = 0;
                        if (LOWORD(wp) == CLOCK_IDM_POMODORO_WORK) {
                            selectedIndex = 0;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_BREAK) {
                            selectedIndex = 1;
                        } else if (LOWORD(wp) == CLOCK_IDM_POMODORO_LBREAK) {
                            selectedIndex = 2;
                        } else {
                            /** Dynamic index from menu ID */
                            selectedIndex = LOWORD(wp) - CLOCK_IDM_POMODORO_TIME_BASE;
                        }
                        
                        if (selectedIndex >= 0 && selectedIndex < POMODORO_TIMES_COUNT) {
                            memset(inputText, 0, sizeof(inputText));
                            DialogBoxParamW(GetModuleHandle(NULL), 
                                     MAKEINTRESOURCEW(CLOCK_IDD_POMODORO_TIME_DIALOG),
                                     hwnd, DlgProc, (LPARAM)CLOCK_IDD_POMODORO_TIME_DIALOG);
                            
                            if (inputText[0] && !isAllSpacesOnly(inputText)) {
                                int total_seconds = 0;
        
                                char inputTextA[256];
                                WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                                if (ParseInput(inputTextA, &total_seconds)) {
                                    POMODORO_TIMES[selectedIndex] = total_seconds;
                                    
                                    WriteConfigPomodoroTimeOptions(POMODORO_TIMES, POMODORO_TIMES_COUNT);
                                    
                                    if (selectedIndex == 0) POMODORO_WORK_TIME = total_seconds;
                                    else if (selectedIndex == 1) POMODORO_SHORT_BREAK = total_seconds;
                                    else if (selectedIndex == 2) POMODORO_LONG_BREAK = total_seconds;
                                }
                            }
                        }
                    }
                    break;
                case CLOCK_IDM_POMODORO_LOOP_COUNT:
                    ShowPomodoroLoopDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_RESET: {
                    extern void StopNotificationSound(void);
                    StopNotificationSound();
                    
                    extern void ResetTimer(void);
                    ResetTimer();
                    
                    if (CLOCK_TOTAL_TIME == POMODORO_WORK_TIME || 
                        CLOCK_TOTAL_TIME == POMODORO_SHORT_BREAK || 
                        CLOCK_TOTAL_TIME == POMODORO_LONG_BREAK) {
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                    
                    HandleWindowReset(hwnd);
                    break;
                }
                
                /** Timer timeout action configurations */
                case CLOCK_IDM_TIMEOUT_SHOW_TIME: {
                    WriteConfigTimeoutAction("SHOW_TIME");
                    break;
                }
                case CLOCK_IDM_TIMEOUT_COUNT_UP: {
                    WriteConfigTimeoutAction("COUNT_UP");
                    break;
                }
                case CLOCK_IDM_SHOW_MESSAGE: {
                    WriteConfigTimeoutAction("MESSAGE");
                    break;
                }
                case CLOCK_IDM_LOCK_SCREEN: {
                    WriteConfigTimeoutAction("LOCK");
                    break;
                }
                case CLOCK_IDM_SHUTDOWN: {
                    WriteConfigTimeoutAction("SHUTDOWN");
                    break;
                }
                case CLOCK_IDM_RESTART: {
                    WriteConfigTimeoutAction("RESTART");
                    break;
                }
                case CLOCK_IDM_SLEEP: {
                    WriteConfigTimeoutAction("SLEEP");
                    break;
                }
                
                /** Application updates and external links */
                case CLOCK_IDM_CHECK_UPDATE: {
                    CheckForUpdateAsync(hwnd, FALSE);
                    break;
                }
                case CLOCK_IDM_OPEN_WEBSITE:
                    ShowWebsiteDialog(hwnd);
                    break;
                
                case CLOCK_IDM_CURRENT_WEBSITE:
                    ShowWebsiteDialog(hwnd);
                    break;
                case CLOCK_IDM_POMODORO_COMBINATION:
                    ShowPomodoroComboDialog(hwnd);
                    break;
                
                /** Notification configuration dialogs */
                case CLOCK_IDM_NOTIFICATION_CONTENT: {
                    ShowNotificationMessagesDialog(hwnd);
                    break;
                }
                case CLOCK_IDM_NOTIFICATION_DISPLAY: {
                    ShowNotificationDisplayDialog(hwnd);
                    break;
                }
                case CLOCK_IDM_NOTIFICATION_SETTINGS: {
                    ShowNotificationSettingsDialog(hwnd);
                    break;
                }
                
                /** Configuration dialogs and help functions */
                case CLOCK_IDM_HOTKEY_SETTINGS: {
                    ShowHotkeySettingsDialog(hwnd);

                    /** Re-register hotkeys after configuration changes */
                    RegisterGlobalHotkeys(hwnd);
                    break;
                }
                case CLOCK_IDM_HELP: {
                    OpenUserGuide();
                    break;
                }
                case CLOCK_IDM_SUPPORT: {
                    OpenSupportPage();
                    break;
                }
                case CLOCK_IDM_FEEDBACK: {
                    OpenFeedbackPage();
                    break;
                }
            }
            break;

        /** Common window refresh point for font changes */
refresh_window:
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }
        
        /** Window position and state change events */
        case WM_WINDOWPOSCHANGED: {
            if (CLOCK_EDIT_MODE) {
                SaveWindowSettings(hwnd);
            }
            break;
        }

        /** Handle display configuration changes (monitor enable/disable) */
        case WM_DISPLAYCHANGE: {
            /** Adjust window position if current monitor becomes inactive */
            AdjustWindowPosition(hwnd, TRUE);
            
            /** Force window repaint after display change */
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            
            return 0;
        }
        
        /** Right-click menu and edit mode handling */
        case WM_RBUTTONUP: {
            if (CLOCK_EDIT_MODE) {
                EndEditMode(hwnd);
                return 0;
            }
            break;
        }
        
        /** Owner-drawn menu item measurement */
        case WM_MEASUREITEM:
        {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lp;
            if (lpmis->CtlType == ODT_MENU) {
                lpmis->itemHeight = 25;
                lpmis->itemWidth = 100;
                return TRUE;
            }
            return FALSE;
        }
        
        /** Owner-drawn menu item rendering */
        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lp;
            if (lpdis->CtlType == ODT_MENU) {
                int colorIndex = lpdis->itemID - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    /** Draw color swatch for menu item */
                    const char* hexColor = COLOR_OPTIONS[colorIndex].hexColor;
                    int r, g, b;
                    sscanf(hexColor + 1, "%02x%02x%02x", &r, &g, &b);
                    
                    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
                    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
                    
                    HGDIOBJ oldBrush = SelectObject(lpdis->hDC, hBrush);
                    HGDIOBJ oldPen = SelectObject(lpdis->hDC, hPen);
                    
                    Rectangle(lpdis->hDC, lpdis->rcItem.left, lpdis->rcItem.top,
                             lpdis->rcItem.right, lpdis->rcItem.bottom);
                    
                    SelectObject(lpdis->hDC, oldPen);
                    SelectObject(lpdis->hDC, oldBrush);
                    DeleteObject(hPen);
                    DeleteObject(hBrush);
                    
                    if (lpdis->itemState & ODS_SELECTED) {
                        DrawFocusRect(lpdis->hDC, &lpdis->rcItem);
                    }
                    
                    return TRUE;
                }
            }
            return FALSE;
        }
        
        /** Menu item selection and preview handling */
        case WM_MENUSELECT: {
            UINT menuItem = LOWORD(wp);
            UINT flags = HIWORD(wp);
            HMENU hMenu = (HMENU)lp;

            /** If mouse moved outside any menu item (including outside menu window), cancel previews */
            if (menuItem == 0xFFFF) {
                KillTimer(hwnd, IDT_MENU_DEBOUNCE);
                SetTimer(hwnd, IDT_MENU_DEBOUNCE, 50, NULL);
                return 0;
            } else {
                KillTimer(hwnd, IDT_MENU_DEBOUNCE);
            }

            if (hMenu != NULL) {
                /** Handle animation preview on hover for fixed items (regardless of popup flag) */
                if (menuItem == CLOCK_IDM_ANIMATIONS_USE_LOGO) {
                    extern void StartAnimationPreview(const char* name);
                    StartAnimationPreview("__logo__");
                    return 0;
                }
                if (menuItem == CLOCK_IDM_ANIMATIONS_USE_CPU) {
                    StartAnimationPreview("__cpu__");
                    return 0;
                }
                if (menuItem == CLOCK_IDM_ANIMATIONS_USE_MEM) {
                    StartAnimationPreview("__mem__");
                    return 0;
                }
                
                /** Only handle other previews for non-popup items */
                if (!(flags & MF_POPUP)) {
                /** Handle color preview on hover */
                int colorIndex = menuItem - 201;
                if (colorIndex >= 0 && colorIndex < COLOR_OPTIONS_COUNT) {
                    strncpy(PREVIEW_COLOR, COLOR_OPTIONS[colorIndex].hexColor, sizeof(PREVIEW_COLOR) - 1);
                    PREVIEW_COLOR[sizeof(PREVIEW_COLOR) - 1] = '\0';
                    IS_COLOR_PREVIEWING = TRUE;
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }


                
                /** Handle fonts folder font preview on hover (IDs 2000+) */
                if (menuItem >= 2000 && menuItem < 3000) {
                    /** Find font name for preview (wide-char), then convert to UTF-8 */
                    wchar_t fontsFolderRootW[MAX_PATH] = {0};
                    if (GetFontsFolderWideFromConfig(fontsFolderRootW, MAX_PATH)) {

                        int currentIndex = 2000;
                        wchar_t foundRelativePathW[MAX_PATH] = {0};

                        if (FindFontByIdRecursiveW(fontsFolderRootW, menuItem, &currentIndex, foundRelativePathW, fontsFolderRootW)) {
                            /** Convert relative wide path to UTF-8 for SwitchFont */
                            char foundFontNameUTF8[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, foundRelativePathW, -1, foundFontNameUTF8, MAX_PATH, NULL, NULL);

                            /** Set up preview variables */
                            strncpy(PREVIEW_FONT_NAME, foundFontNameUTF8, sizeof(PREVIEW_FONT_NAME) - 1);
                            PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';

                            HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
                            LoadFontByNameAndGetRealName(hInstance, foundFontNameUTF8, PREVIEW_INTERNAL_NAME, sizeof(PREVIEW_INTERNAL_NAME));

                            IS_PREVIEWING = TRUE;
                            InvalidateRect(hwnd, NULL, TRUE);
                            return 0;
                        }
                    }
                    return 0;
                }


                /** Handle animation preview on hover (IDs CLOCK_IDM_ANIMATIONS_BASE..+999) */
                if (menuItem >= CLOCK_IDM_ANIMATIONS_BASE && menuItem < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
                    /** Resolve folder name by iterating animations root with a running index */
                    char animRootUtf8[MAX_PATH] = {0};
                    GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
                    wchar_t wRoot[MAX_PATH] = {0};
                    MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);

                    UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;

                    /** Recursive helper function to match menu items for hover preview */
                    BOOL FindAnimationByIdRecursive(const wchar_t* folderPathW, const char* folderPathUtf8, UINT* nextIdPtr, UINT targetId) {
                        AnimationEntry* entries = (AnimationEntry*)malloc(sizeof(AnimationEntry) * MAX_TRAY_FRAMES);
                        if (!entries) return FALSE;
                        int entryCount = 0;

                        wchar_t wSearch[MAX_PATH] = {0};
                        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", folderPathW);
                        
                        WIN32_FIND_DATAW ffd;
                        HANDLE hFind = FindFirstFileW(wSearch, &ffd);
                        if (hFind == INVALID_HANDLE_VALUE) {
                            free(entries);
                            return FALSE;
                        }

                        do {
                            if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                            if (entryCount >= MAX_TRAY_FRAMES) break;

                            AnimationEntry* e = &entries[entryCount];
                            e->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                             wcsncpy(e->name, ffd.cFileName, MAX_PATH - 1);
                            e->name[MAX_PATH - 1] = L'\0';

                            char itemUtf8[MAX_PATH] = {0};
                            WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, itemUtf8, MAX_PATH, NULL, NULL);
                            if (folderPathUtf8 && folderPathUtf8[0] != '\0') {
                                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s\\%s", folderPathUtf8, itemUtf8);
                            } else {
                                _snprintf_s(e->rel_path_utf8, MAX_PATH, _TRUNCATE, "%s", itemUtf8);
                            }
                            
                            if (e->is_dir) {
                                entryCount++;
                            } else {
                                wchar_t* ext = wcsrchr(e->name, L'.');
                                if (ext && (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
                                            _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
                                            _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
                                            _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".tif") == 0 ||
                                            _wcsicmp(ext, L".tiff") == 0)) {
                                    entryCount++;
                                }
                            }
                        } while (FindNextFileW(hFind, &ffd));
                        FindClose(hFind);

                        if (entryCount == 0) {
                            free(entries);
                            return FALSE;
                        }
                        qsort(entries, entryCount, sizeof(AnimationEntry), CompareAnimationEntries);

                        for (int i = 0; i < entryCount; ++i) {
                            AnimationEntry* e = &entries[i];
                            if (e->is_dir) {
                                wchar_t wSubFolderPath[MAX_PATH] = {0};
                                _snwprintf_s(wSubFolderPath, MAX_PATH, _TRUNCATE, L"%s\\%s", folderPathW, e->name);

                                if (IsAnimationLeafFolderW(wSubFolderPath)) {
                                    if (*nextIdPtr == targetId) {
                                        StartAnimationPreview(e->rel_path_utf8);
                                        free(entries);
                                        return TRUE;
                                    }
                                    (*nextIdPtr)++;
                                } else {
                                    if (FindAnimationByIdRecursive(wSubFolderPath, e->rel_path_utf8, nextIdPtr, targetId)) {
                                        free(entries);
                                        return TRUE;
                                    }
                                }
                            } else {
                                if (*nextIdPtr == targetId) {
                                    StartAnimationPreview(e->rel_path_utf8);
                                    free(entries);
                                    return TRUE;
                                }
                                (*nextIdPtr)++;
                            }
                        }
                        free(entries);
                        return FALSE;
                    }

                    if (FindAnimationByIdRecursive(wRoot, "", &nextId, menuItem)) {
                        return 0;
                    }
                }

                /** Handle time format preview on hover */
                if (menuItem == CLOCK_IDM_TIME_FORMAT_DEFAULT ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_ZERO_PADDED ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_FULL_PADDED ||
                    menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
                    
                    if (menuItem == CLOCK_IDM_TIME_FORMAT_SHOW_MILLISECONDS) {
                        /** Handle milliseconds preview */
                        PREVIEW_SHOW_MILLISECONDS = !CLOCK_SHOW_MILLISECONDS;
                        IS_MILLISECONDS_PREVIEWING = TRUE;
                        
                        /** Adjust timer frequency for smooth preview */
                        ResetTimerWithInterval(hwnd);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    } else {
                        /** Handle format preview */
                        TimeFormatType previewFormat = TIME_FORMAT_DEFAULT;
                        switch (menuItem) {
                            case CLOCK_IDM_TIME_FORMAT_DEFAULT:
                                previewFormat = TIME_FORMAT_DEFAULT;
                                break;
                            case CLOCK_IDM_TIME_FORMAT_ZERO_PADDED:
                                previewFormat = TIME_FORMAT_ZERO_PADDED;
                                break;
                            case CLOCK_IDM_TIME_FORMAT_FULL_PADDED:
                                previewFormat = TIME_FORMAT_FULL_PADDED;
                                break;
                        }
                        
                        PREVIEW_TIME_FORMAT = previewFormat;
                        IS_TIME_FORMAT_PREVIEWING = TRUE;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
                
                /** Clear preview if no matching item found */
                extern void CancelAnimationPreview(void);
                CancelAnimationPreview();
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                    if (IS_PREVIEWING) {
                        CancelFontPreview();
                    }
                    IS_COLOR_PREVIEWING = FALSE;
                    IS_TIME_FORMAT_PREVIEWING = FALSE;
                    
                    /** Reset timer frequency when exiting milliseconds preview */
                    if (IS_MILLISECONDS_PREVIEWING) {
                        IS_MILLISECONDS_PREVIEWING = FALSE;
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            } else if (flags & MF_POPUP) {
                extern void CancelAnimationPreview(void);
                CancelAnimationPreview();
                if (IS_PREVIEWING || IS_COLOR_PREVIEWING || IS_TIME_FORMAT_PREVIEWING || IS_MILLISECONDS_PREVIEWING) {
                    if (IS_PREVIEWING) {
                        CancelFontPreview();
                    }
                    IS_COLOR_PREVIEWING = FALSE;
                    IS_TIME_FORMAT_PREVIEWING = FALSE;
                    
                    /** Reset timer frequency when exiting milliseconds preview */
                    if (IS_MILLISECONDS_PREVIEWING) {
                        IS_MILLISECONDS_PREVIEWING = FALSE;
                        ResetTimerWithInterval(hwnd);
                    }
                    
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                
            }
            }
            break;
        }
        
        /** Menu loop exit cleanup */
        case WM_EXITMENULOOP: {
            KillTimer(hwnd, IDT_MENU_DEBOUNCE);
            SetTimer(hwnd, IDT_MENU_DEBOUNCE, 50, NULL);
            break;
        }
        
        /** Ctrl+Right-click edit mode toggle */
        case WM_RBUTTONDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
                
                if (CLOCK_EDIT_MODE) {
                    SetClickThrough(hwnd, FALSE);
                } else {
                    SetClickThrough(hwnd, TRUE);
                    SaveWindowSettings(hwnd);
                    WriteConfigColor(CLOCK_TEXT_COLOR);
                }
                
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            break;
        }
        
        /** Window lifecycle events */
        case WM_CLOSE: {
            SaveWindowSettings(hwnd);
            DestroyWindow(hwnd);
            break;
        }
        
        /** Double-click to enter edit mode */
        case WM_LBUTTONDBLCLK: {
            if (!CLOCK_EDIT_MODE) {
                StartEditMode(hwnd);
                return 0;
            }
            break;
        }
        
        /** Global hotkey message processing */
        case WM_HOTKEY: {
            if (wp == HOTKEY_ID_SHOW_TIME) {
                ToggleShowTimeMode(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_COUNT_UP) {
                StartCountUp(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_COUNTDOWN) {
                StartDefaultCountDown(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_CUSTOM_COUNTDOWN) {
                /** Close existing input dialog if open */
                if (g_hwndInputDialog != NULL && IsWindow(g_hwndInputDialog)) {
                    SendMessage(g_hwndInputDialog, WM_CLOSE, 0, 0);
                    return 0;
                }
                
                /** Reset notification state for new countdown */
                extern BOOL countdown_message_shown;
                countdown_message_shown = FALSE;
                
                extern void ReadNotificationTypeConfig(void);
                ReadNotificationTypeConfig();
                
                extern int elapsed_time;
                extern BOOL message_shown;
                
                memset(inputText, 0, sizeof(inputText));
                
                INT_PTR result = DialogBoxParamW(GetModuleHandle(NULL), 
                                         MAKEINTRESOURCEW(CLOCK_IDD_DIALOG1), 
                                         hwnd, DlgProc, (LPARAM)CLOCK_IDD_DIALOG1);
                
                if (inputText[0] != L'\0') {
                    int total_seconds = 0;

                    char inputTextA[256];
                    WideCharToMultiByte(CP_UTF8, 0, inputText, -1, inputTextA, sizeof(inputTextA), NULL, NULL);
                    if (ParseInput(inputTextA, &total_seconds)) {
                        extern void StopNotificationSound(void);
                        StopNotificationSound();
                        
                        CloseAllNotifications();
                        
                        CLOCK_TOTAL_TIME = total_seconds;
                        countdown_elapsed_time = 0;
                        elapsed_time = 0;
                        message_shown = FALSE;
                        countdown_message_shown = FALSE;
                        
                        CLOCK_COUNT_UP = FALSE;
                        CLOCK_SHOW_CURRENT_TIME = FALSE;
                        CLOCK_IS_PAUSED = FALSE;
                        
                        KillTimer(hwnd, 1);
                        ResetTimerWithInterval(hwnd);
                        
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN1) {
                StartQuickCountdown1(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN2) {
                StartQuickCountdown2(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_QUICK_COUNTDOWN3) {
                StartQuickCountdown3(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_POMODORO) {

                StartPomodoroTimer(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_TOGGLE_VISIBILITY) {
                /** Toggle window visibility */
                if (IsWindowVisible(hwnd)) {
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    ShowWindow(hwnd, SW_SHOW);
                    SetForegroundWindow(hwnd);
                }
                return 0;
            } else if (wp == HOTKEY_ID_EDIT_MODE) {

                ToggleEditMode(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_PAUSE_RESUME) {

                TogglePauseResume(hwnd);
                return 0;
            } else if (wp == HOTKEY_ID_RESTART_TIMER) {
                /** Restart current timer from beginning */
                CloseAllNotifications();

                RestartCurrentTimer(hwnd);
                return 0;
            }
            break;
        }

        /** Custom application message for hotkey re-registration */
        case WM_APP+1: {
            RegisterGlobalHotkeys(hwnd);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

/** @brief External timer state variables */
extern int CLOCK_DEFAULT_START_TIME;
extern int countdown_elapsed_time;
extern BOOL CLOCK_IS_PAUSED;
extern BOOL CLOCK_COUNT_UP;
extern BOOL CLOCK_SHOW_CURRENT_TIME;
extern int CLOCK_TOTAL_TIME;

/** @brief Menu manipulation helper functions */
void RemoveMenuItems(HMENU hMenu, int count);

void AddMenuItem(HMENU hMenu, UINT id, const wchar_t* text, BOOL isEnabled);

void ModifyMenuItemText(HMENU hMenu, UINT id, const wchar_t* text);



/**
 * @brief Toggle between timer display and current time display
 * @param hwnd Main window handle
 *
 * Switches to current time mode with 100ms refresh rate
 */
void ToggleShowTimeMode(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        CLOCK_SHOW_CURRENT_TIME = TRUE;
        
        ResetTimerWithInterval(hwnd);
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Start count-up timer from zero
 * @param hwnd Main window handle
 *
 * Initializes count-up mode and adjusts timer refresh rate if needed
 */
void StartCountUp(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern int countup_elapsed_time;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    countup_elapsed_time = 0;
    
    CLOCK_COUNT_UP = TRUE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    CLOCK_IS_PAUSED = FALSE;
    
    if (wasShowingTime) {
        ResetTimerWithInterval(hwnd);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start default countdown timer
 * @param hwnd Main window handle
 *
 * Uses configured default time or prompts for input if not set
 */
void StartDefaultCountDown(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (CLOCK_DEFAULT_START_TIME > 0) {
        CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
            } else {
            /** Prompt for time input if no default set */
            PostMessage(hwnd, WM_COMMAND, 101, 0);
        }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Start Pomodoro timer session
 * @param hwnd Main window handle
 *
 * Initializes Pomodoro mode with work phase timing
 */
void StartPomodoroTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    if (wasShowingTime) {
        KillTimer(hwnd, 1);
    }
    
    PostMessage(hwnd, WM_COMMAND, CLOCK_IDM_POMODORO_START, 0);
}

/**
 * @brief Toggle window edit mode for positioning and configuration
 * @param hwnd Main window handle
 *
 * Switches between transparent click-through mode and interactive edit mode
 */
void ToggleEditMode(HWND hwnd) {
    CLOCK_EDIT_MODE = !CLOCK_EDIT_MODE;
    
    if (CLOCK_EDIT_MODE) {
        /** Enter edit mode: make window interactive */
        PREVIOUS_TOPMOST_STATE = CLOCK_WINDOW_TOPMOST;
        
        if (!CLOCK_WINDOW_TOPMOST) {
            SetWindowTopmost(hwnd, TRUE);
        }
        
        SetBlurBehind(hwnd, TRUE);
        
        SetClickThrough(hwnd, FALSE);
        
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    } else {
        /** Exit edit mode: restore transparency and click-through */
        SetBlurBehind(hwnd, FALSE);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY);
        
        SetClickThrough(hwnd, TRUE);
        
        if (!PREVIOUS_TOPMOST_STATE) {
            SetWindowTopmost(hwnd, FALSE);
            
            InvalidateRect(hwnd, NULL, TRUE);
            RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
            KillTimer(hwnd, 1002);
            SetTimer(hwnd, 1002, 150, NULL);
            return;
        }
        
        SaveWindowSettings(hwnd);
        WriteConfigColor(CLOCK_TEXT_COLOR);
    }
    
    InvalidateRect(hwnd, NULL, TRUE);
}

/**
 * @brief Toggle pause/resume state of current timer
 * @param hwnd Main window handle
 *
 * Only affects countdown/count-up timers, not current time display
 */
void TogglePauseResume(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    if (!CLOCK_SHOW_CURRENT_TIME && (CLOCK_COUNT_UP || CLOCK_TOTAL_TIME > 0)) {
        if (!CLOCK_IS_PAUSED) {
            /** About to pause: save current milliseconds first */
            PauseTimerMilliseconds();
        }
        
        CLOCK_IS_PAUSED = !CLOCK_IS_PAUSED;
        
        if (CLOCK_IS_PAUSED) {
            /** Record pause timestamp and stop updates */
            CLOCK_LAST_TIME_UPDATE = time(NULL);
            KillTimer(hwnd, 1);
            
            extern BOOL PauseNotificationSound(void);
            PauseNotificationSound();
        } else {
            /** Resume timer updates and notification sounds */
            ResetMillisecondAccumulator();  /** Reset millisecond timing on resume */
            SetTimer(hwnd, 1, GetTimerInterval(), NULL);
            
            extern BOOL ResumeNotificationSound(void);
            ResumeNotificationSound();
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Restart current timer from beginning
 * @param hwnd Main window handle
 *
 * Resets elapsed time and notification state for active timer
 */
void RestartCurrentTimer(HWND hwnd) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    if (!CLOCK_SHOW_CURRENT_TIME) {
        extern int elapsed_time;
        extern BOOL message_shown;
        
        message_shown = FALSE;
        countdown_message_shown = FALSE;
        countup_message_shown = FALSE;
        
        if (CLOCK_COUNT_UP) {
            countdown_elapsed_time = 0;
            countup_elapsed_time = 0;
        } else {
            countdown_elapsed_time = 0;
            elapsed_time = 0;
        }
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on restart */
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

/**
 * @brief Start configured quick countdown timer by index (0-based)
 * @param hwnd Main window handle
 * @param index Zero-based index into time_options array
 *
 * Uses specified time option or falls back to default countdown
 */
static void StartQuickCountdownByZeroBasedIndex(HWND hwnd, int index) {
    extern void StopNotificationSound(void);
    StopNotificationSound();
    
    CloseAllNotifications();
    
    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;
    
    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();
    
    extern int time_options[];
    extern int time_options_count;
    
    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;
    
    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;
    
    if (index >= 0 && index < time_options_count) {
        CLOCK_TOTAL_TIME = time_options[index];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */
        
        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }
        
        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}

/**
 * @brief Start first configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses first time option or falls back to default countdown
 */
void StartQuickCountdown1(HWND hwnd) {
    StartQuickCountdownByZeroBasedIndex(hwnd, 0);
}

/**
 * @brief Start second configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses second time option or falls back to default countdown
 */
void StartQuickCountdown2(HWND hwnd) {
    StartQuickCountdownByZeroBasedIndex(hwnd, 1);
}

/**
 * @brief Start third configured quick countdown timer
 * @param hwnd Main window handle
 *
 * Uses third time option or falls back to default countdown
 */
void StartQuickCountdown3(HWND hwnd) {
    StartQuickCountdownByZeroBasedIndex(hwnd, 2);
}

/**
 * @brief Start quick countdown timer by index
 * @param hwnd Main window handle
 * @param index 1-based index of time option to use
 *
 * Generic function to start any configured quick countdown option
 */
void StartQuickCountdownByIndex(HWND hwnd, int index) {
    if (index <= 0) return;

    extern void StopNotificationSound(void);
    StopNotificationSound();

    CloseAllNotifications();

    extern BOOL countdown_message_shown;
    countdown_message_shown = FALSE;

    extern void ReadNotificationTypeConfig(void);
    ReadNotificationTypeConfig();

    extern int time_options[];
    extern int time_options_count;

    BOOL wasShowingTime = CLOCK_SHOW_CURRENT_TIME;

    CLOCK_COUNT_UP = FALSE;
    CLOCK_SHOW_CURRENT_TIME = FALSE;

    /** Convert to zero-based index for array access */
    int zeroBased = index - 1;
    if (zeroBased >= 0 && zeroBased < time_options_count) {
        CLOCK_TOTAL_TIME = time_options[zeroBased];
        countdown_elapsed_time = 0;
        CLOCK_IS_PAUSED = FALSE;
        ResetMillisecondAccumulator();  /** Reset millisecond timing on new countdown */

        if (wasShowingTime) {
            KillTimer(hwnd, 1);
            ResetTimerWithInterval(hwnd);
        }

        InvalidateRect(hwnd, NULL, TRUE);
    } else {
        StartDefaultCountDown(hwnd);
    }
}
