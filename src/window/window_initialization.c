/**
 * @file window_initialization.c
 * @brief Application initialization (DPI, fonts, configuration)
 */

#include "window/window_initialization.h"
#include "window/window_core.h"
#include "timer/timer.h"
#include "language.h"
#include "font.h"
#include "startup.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CONSOLE_CODEPAGE_GBK 936
#define SHCORE_DLL L"shcore.dll"
#define USER32_DLL L"user32.dll"

/* ============================================================================
 * DPI awareness types
 * ============================================================================ */

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

typedef enum {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * @brief Enable DPI awareness with multi-tier fallback for compatibility
 * @return TRUE if any DPI awareness level set
 */
static BOOL InitializeDpiAwareness(void) {
    LOG_INFO("Initializing DPI awareness");
    
    HMODULE hUser32 = GetModuleHandleW(USER32_DLL);
    if (!hUser32) {
        LOG_WARNING("Failed to get user32.dll handle");
        return FALSE;
    }
    
    /* Try Windows 10 1703+ per-monitor V2 (best quality) */
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    SetProcessDpiAwarenessContextFunc setDpiCtx = 
        (SetProcessDpiAwarenessContextFunc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    
    if (setDpiCtx) {
        if (setDpiCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            LOG_INFO("DPI awareness: Per-Monitor V2 (Windows 10 1703+)");
            return TRUE;
        }
    }
    
    /* Try Windows 8.1+ per-monitor (acceptable) */
    HMODULE hShcore = LoadLibraryW(SHCORE_DLL);
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        SetProcessDpiAwarenessFunc setDpiAwareness = 
            (SetProcessDpiAwarenessFunc)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        
        if (setDpiAwareness) {
            if (SUCCEEDED(setDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))) {
                LOG_INFO("DPI awareness: Per-Monitor (Windows 8.1+)");
                FreeLibrary(hShcore);
                return TRUE;
            }
        }
        FreeLibrary(hShcore);
    }
    
    /* Final fallback: basic system DPI awareness (Windows Vista+) */
    #ifndef _INC_WINUSER
    WINUSERAPI BOOL WINAPI SetProcessDPIAware(VOID);
    #endif
    
    if (SetProcessDPIAware()) {
        LOG_INFO("DPI awareness: System (legacy)");
        return TRUE;
    }
    
    LOG_WARNING("Failed to set any DPI awareness level");
    return FALSE;
}

/**
 * @brief Load fonts from configuration
 * @param hInstance Application instance for resource extraction
 * @return TRUE on success
 */
static BOOL InitializeFonts(HINSTANCE hInstance) {
    LOG_INFO("Initializing fonts");
    
    if (IsFirstRun()) {
        LOG_INFO("First run detected, extracting embedded fonts");
        if (ExtractEmbeddedFontsToFolder(hInstance)) {
            SetFirstRunCompleted();
            LOG_INFO("Embedded fonts extracted successfully");
        } else {
            LOG_WARNING("Failed to extract embedded fonts");
        }
    }
    
    if (NeedsFontLicenseVersionAcceptance()) {
        LOG_INFO("Font license acceptance required (will be handled in UI)");
    }
    
    CheckAndFixFontPath();
    
    /* Strip %LOCALAPPDATA% prefix if present for relative path access */
    char actualFontFileName[MAX_PATH];
    const char* localappdata_prefix = FONTS_PATH_PREFIX;
    if (_strnicmp(FONT_FILE_NAME, localappdata_prefix, strlen(localappdata_prefix)) == 0) {
        strncpy(actualFontFileName, FONT_FILE_NAME + strlen(localappdata_prefix), sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    } else {
        strncpy(actualFontFileName, FONT_FILE_NAME, sizeof(actualFontFileName) - 1);
        actualFontFileName[sizeof(actualFontFileName) - 1] = '\0';
    }
    
    if (!LoadFontByNameAndGetRealName(hInstance, actualFontFileName, 
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_ERROR("Failed to load font: %s", actualFontFileName);
        return FALSE;
    }
    
    LOG_INFO("Font loaded successfully: %s", FONT_INTERNAL_NAME);
    return TRUE;
}

/**
 * @brief Load configuration and initialize console encoding
 * @return TRUE on success
 */
static BOOL InitializeDefaultSettings(void) {
    LOG_INFO("Initializing default settings");
    
    SetConsoleOutputCP(CONSOLE_CODEPAGE_GBK);
    SetConsoleCP(CONSOLE_CODEPAGE_GBK);
    
    ReadConfig();
    CLOCK_FONT_SCALE_FACTOR = CLOCK_WINDOW_SCALE;
    
    UpdateStartupShortcut();
    InitializeDefaultLanguage();
    
    CLOCK_TOTAL_TIME = CLOCK_DEFAULT_START_TIME;
    
    LOG_INFO("Default settings initialized");
    return TRUE;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL InitializeApplication(HINSTANCE hInstance) {
    LOG_INFO("Application initialization started");
    
    if (!InitializeDpiAwareness()) {
        LOG_WARNING("DPI awareness initialization failed, continuing anyway");
    }
    
    if (!InitializeDefaultSettings()) {
        LOG_ERROR("Default settings initialization failed");
        return FALSE;
    }
    
    if (!InitializeFonts(hInstance)) {
        LOG_ERROR("Font initialization failed");
        return FALSE;
    }
    
    LOG_INFO("Application initialization completed successfully");
    return TRUE;
}

