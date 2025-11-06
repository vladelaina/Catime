/**
 * @file main_initialization.h
 * @brief Application initialization and window setup
 */

#ifndef MAIN_INITIALIZATION_H
#define MAIN_INITIALIZATION_H

#include <windows.h>

/**
 * Initialize core subsystems (COM, logging, exception handling)
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeSubsystems(void);

/**
 * Initialize application subsystem
 * @param hInstance Application instance handle
 * @return TRUE on success, FALSE on failure
 */
BOOL InitializeApplicationSubsystem(HINSTANCE hInstance);

/**
 * Setup desktop shortcut for package manager installs
 */
void SetupDesktopShortcut(void);

/**
 * Initialize dialog multi-language support
 */
void InitializeDialogLanguages(void);

/**
 * Setup main window (CLI parsing, timers, startup mode)
 * @param hInstance Application instance handle
 * @param hwnd Window handle
 * @param nCmdShow Show window flag
 * @return TRUE on success, FALSE on failure
 */
BOOL SetupMainWindow(HINSTANCE hInstance, HWND hwnd, int nCmdShow);

/**
 * Run main message loop
 * @param hwnd Window handle
 * @return Exit code
 */
int RunMessageLoop(HWND hwnd);

/**
 * Cleanup resources before exit
 * @param hMutex Mutex handle to release
 */
void CleanupResources(HANDLE hMutex);

#endif /* MAIN_INITIALIZATION_H */

