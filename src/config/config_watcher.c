/**
 * @file config_watcher.c
 * @brief Shared state for the asynchronous configuration watcher.
 */

#include "config_watcher_internal.h"

HANDLE g_watcherThread = NULL;
HANDLE g_stopEvent = NULL;
HWND g_targetHwnd = NULL;
volatile LONG g_configReloadPending = 0;
volatile LONG g_configReloadDirty = 0;
volatile LONG g_acceptingChanges = 0;
