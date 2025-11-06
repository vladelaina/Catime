/**
 * @file window_initialization.h
 * @brief Application initialization (DPI, fonts, configuration)
 */

#ifndef WINDOW_INITIALIZATION_H
#define WINDOW_INITIALIZATION_H

#include <windows.h>

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize application subsystems
 * @param hInstance Application instance handle
 * @return TRUE if initialization succeeded, FALSE on critical error
 * 
 * @details Initializes in order:
 * 1. DPI awareness (fallback for old Windows)
 * 2. Configuration loading
 * 3. Font extraction and loading
 * 4. Language/startup settings
 * 
 * @note Call before CreateMainWindow
 */
BOOL InitializeApplication(HINSTANCE hInstance);

#endif /* WINDOW_INITIALIZATION_H */

