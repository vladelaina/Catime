#ifndef MONITOR_CORE_H
#define MONITOR_CORE_H

#include <windows.h>
#include "monitor_types.h"

// Initialize the monitor subsystem
void Monitor_Init(void);

// Shutdown and cleanup
void Monitor_Shutdown(void);

// Get the current display text (thread-safe)
// Returns FALSE if no text is available or monitor is disabled
BOOL Monitor_GetDisplayText(wchar_t* buffer, size_t maxLen);

// Check if monitor mode is active
BOOL Monitor_IsActive(void);

// --- Configuration Management ---

// Load/Save configurations
void Monitor_LoadConfig(void);
void Monitor_SaveConfig(void);

// List management
int Monitor_GetConfigCount(void);
BOOL Monitor_GetConfigAt(int index, MonitorConfig* outConfig);
void Monitor_AddConfig(const MonitorConfig* config);
void Monitor_UpdateConfigAt(int index, const MonitorConfig* config);
void Monitor_DeleteConfigAt(int index);

// Active selection
int Monitor_GetActiveIndex(void);
void Monitor_SetActiveIndex(int index); // -1 to disable
void Monitor_ForceRefresh(void);

// Preview support
void Monitor_SetPreviewIndex(int index); // -1 to disable
// Set a temporary config for preview (for edit dialog)
void Monitor_SetPreviewConfig(const MonitorConfig* config);
BOOL Monitor_GetPreviewText(wchar_t* buffer, size_t maxLen);
BOOL Monitor_ApplyPreviewIfMatching(int index);

// Metadata
int Monitor_GetPlatformOptions(MonitorPlatformType type, MonitorOption* outOptions, int maxCount);

#endif
