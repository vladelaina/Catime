/**
 * @file plugin_exit.h
 * @brief Plugin exit countdown management
 */

#ifndef PLUGIN_EXIT_H
#define PLUGIN_EXIT_H

#include <windows.h>

/**
 * @brief Initialize exit subsystem
 * @param hwnd Window handle for notifications
 * @param dataCS Critical section for data synchronization
 */
void PluginExit_Init(HWND hwnd, CRITICAL_SECTION* dataCS);

/**
 * @brief Shutdown exit subsystem
 */
void PluginExit_Shutdown(void);

/**
 * @brief Parse <exit> tag and start countdown if found
 * @param text Wide string to parse (will be modified)
 * @param textLen Current text length
 * @param maxLen Buffer capacity
 * @return TRUE if exit tag was processed and countdown started
 */
BOOL PluginExit_ParseTag(wchar_t* text, int* textLen, size_t maxLen);

/**
 * @brief Cancel any pending exit countdown
 */
void PluginExit_Cancel(void);

/**
 * @brief Check if exit countdown is in progress
 */
BOOL PluginExit_IsInProgress(void);

#endif /* PLUGIN_EXIT_H */
