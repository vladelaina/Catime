#ifndef MONITOR_TYPES_H
#define MONITOR_TYPES_H

#include <windows.h>

// Supported platform types
typedef enum {
    MONITOR_PLATFORM_UNKNOWN = 0,
    MONITOR_PLATFORM_GITHUB,
    MONITOR_PLATFORM_BILIBILI,
    // Future platforms...
} MonitorPlatformType;

// Monitor configuration
typedef struct {
    BOOL enabled;               // Is monitoring enabled
    wchar_t label[32];          // Display label (e.g. "Star", "Fans")
    char sourceString[256];     // Raw format string (e.g. "GitHub-user/repo-star")
    char token[128];            // Optional auth token
    MonitorPlatformType type;   // Parsed platform type
    
    // Parsed parameters
    char param1[128];           // e.g. "vladelaina/Catime" or "UID"
    char param2[64];            // e.g. "star" or "fans"
    
    int refreshInterval;        // Refresh interval in seconds
} MonitorConfig;

// Monitor data state
typedef struct {
    wchar_t displayText[64];    // Final display text (e.g. "Star: 1.2k")
    long long rawValue;         // Raw numeric value
    BOOL isError;               // Is in error state
    BOOL isLoading;             // Is currently loading
    DWORD lastUpdateTick;       // Last update timestamp (GetTickCount)
} MonitorState;

#endif
