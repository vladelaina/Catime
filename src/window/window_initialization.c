/**
 * @file window_initialization.c
 * @brief Application initialization (DPI, fonts, configuration)
 */

#include "window/window_initialization.h"
#include "window/window_core.h"
#include "timer/timer.h"
#include "language.h"
#include "font.h"
#include "font/font_config.h"
#include "startup.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* GBK (Code Page 936) is the standard Chinese character encoding used in Windows console */
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
 * @brief Load fonts from configuration with automatic fallback
 * @param hInstance Application instance for resource extraction
 * @return TRUE on success (always succeeds with fallback)
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
    
    /* Try to load configured font */
    if (!LoadFontByNameAndGetRealName(hInstance, actualFontFileName, 
                                      FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
        LOG_WARNING("Failed to load font: %s, attempting fallback to default font", actualFontFileName);
        
        /* Fallback 1: Try default font */
        const char* defaultFont = DEFAULT_FONT_NAME;
        if (LoadFontByNameAndGetRealName(hInstance, defaultFont,
                                        FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
            LOG_INFO("Successfully loaded default font: %s", FONT_INTERNAL_NAME);
            
            /* Update config to default font */
            snprintf(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), "%s%s", 
                    FONTS_PATH_PREFIX, defaultFont);
            
            WriteConfigFont(FONT_FILE_NAME, FALSE);
            FlushConfigToDisk();
            
            LOG_INFO("Font configuration auto-corrected to default font");
            return TRUE;
        }
        
        /* Fallback 2: Re-extract embedded fonts and retry */
        LOG_WARNING("Default font not found, re-extracting embedded fonts");
        if (ExtractEmbeddedFontsToFolder(hInstance)) {
            if (LoadFontByNameAndGetRealName(hInstance, defaultFont,
                                            FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
                LOG_INFO("Successfully loaded default font after extraction: %s", FONT_INTERNAL_NAME);
                
                snprintf(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), "%s%s", 
                        FONTS_PATH_PREFIX, defaultFont);
                
                WriteConfigFont(FONT_FILE_NAME, FALSE);
                FlushConfigToDisk();
                
                LOG_INFO("Font configuration auto-corrected after re-extraction");
                return TRUE;
            }
        }
        
        /* Fallback 3: Try any available embedded font */
        LOG_WARNING("Attempting to load any available embedded font");
        
        for (int i = 0; i < FONT_RESOURCES_COUNT; i++) {
            if (LoadFontByNameAndGetRealName(hInstance, fontResources[i].fontName,
                                            FONT_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME))) {
                LOG_INFO("Successfully loaded fallback font: %s", FONT_INTERNAL_NAME);
                
                snprintf(FONT_FILE_NAME, sizeof(FONT_FILE_NAME), "%s%s", 
                        FONTS_PATH_PREFIX, fontResources[i].fontName);
                
                WriteConfigFont(FONT_FILE_NAME, FALSE);
                FlushConfigToDisk();
                
                LOG_INFO("Font configuration auto-corrected to: %s", fontResources[i].fontName);
                return TRUE;
            }
        }
        
        /* Last resort: Use system default font name */
        LOG_ERROR("All font loading attempts failed, using system default font name");
        strncpy(FONT_INTERNAL_NAME, "Arial", sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
        
        return TRUE;  /* Continue even with system font */
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
    
    CLOCK_TOTAL_TIME = g_AppConfig.timer.default_start_time;
    
    LOG_INFO("Default settings initialized");
    return TRUE;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

BOOL InitializeApplication(HINSTANCE hInstance) {
    LOG_INFO("Application initialization started");
    
    /* DPI awareness is optional */
    if (!InitializeDpiAwareness()) {
        LOG_WARNING("DPI awareness initialization failed, continuing anyway");
    }
    
    /* Settings initialization with fallback */
    if (!InitializeDefaultSettings()) {
        LOG_WARNING("Default settings initialization failed, using built-in defaults");
        /* Continue with built-in defaults instead of failing */
    }
    
    /* Font initialization with automatic fallback (always succeeds) */
    if (!InitializeFonts(hInstance)) {
        LOG_WARNING("Font initialization encountered issues, but fallback succeeded");
    }
    
    LOG_INFO("Application initialization completed (with auto-correction if needed)");
    return TRUE;  /* Always succeed to prevent application crash */
}

