#include "monitor/monitor_core.h"
#include "monitor/monitor_types.h"
#include "monitor/platforms/github.h"
#include "monitor/utils/credential_manager.h"
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_MONITORS 10

static MonitorConfig g_monitors[MAX_MONITORS];
static int g_monitorCount = 0;
static int g_activeIndex = -1;
static int g_previewIndex = -1;

static MonitorConfig g_activeConfig; 
static MonitorState g_state;
static MonitorState g_previewStateData;
static HANDLE g_hThread = NULL;
static volatile BOOL g_running = FALSE;
static volatile BOOL g_forceRefresh = FALSE;
static CRITICAL_SECTION g_cs;

extern void GetConfigPath(char* path, size_t size);

static void ParseSourceString(MonitorConfig* config) {
    char* str = config->sourceString;
    if (strncmp(str, "GitHub-", 7) == 0) {
        config->type = MONITOR_PLATFORM_GITHUB;
        char* p1 = str + 7;
        char* p2 = strrchr(p1, '-');
        if (p2) {
            size_t len1 = (size_t)(p2 - p1);
            if (len1 < sizeof(config->param1)) {
                strncpy(config->param1, p1, len1);
                config->param1[len1] = '\0';
            }
            strncpy(config->param2, p2 + 1, sizeof(config->param2) - 1);
            config->param2[sizeof(config->param2) - 1] = '\0';
        }
    } else if (strncmp(str, "Bilibili-", 9) == 0) {
        config->type = MONITOR_PLATFORM_BILIBILI;
    } else {
        config->type = MONITOR_PLATFORM_UNKNOWN;
    }
}

void Monitor_LoadConfig(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    g_monitorCount = GetPrivateProfileIntA("Monitor", "Count", 0, configPath);
    if (g_monitorCount > MAX_MONITORS) g_monitorCount = MAX_MONITORS;
    
    g_activeIndex = GetPrivateProfileIntA("Monitor", "Active", -1, configPath);
    
    for (int i = 0; i < g_monitorCount; i++) {
        char section[32];
        sprintf(section, "Monitor_%d", i);
        
        char labelUtf8[64];
        GetPrivateProfileStringA(section, "Label", "Item", labelUtf8, 64, configPath);
        MultiByteToWideChar(CP_UTF8, 0, labelUtf8, -1, g_monitors[i].label, 32);
        
        GetPrivateProfileStringA(section, "Source", "", g_monitors[i].sourceString, 256, configPath);
        GetPrivateProfileStringA(section, "Token", "", g_monitors[i].token, 128, configPath);
        
        // Try load secure credential
        if (strlen(g_monitors[i].sourceString) > 0) {
            if (strcmp(g_monitors[i].token, "[SECURE_CREDENTIAL]") == 0 || strlen(g_monitors[i].token) == 0) {
                char targetName[512];
                snprintf(targetName, sizeof(targetName), "Catime:Monitor:%s", g_monitors[i].sourceString);
                char secureToken[128];
                if (Cred_LoadToken(targetName, secureToken, sizeof(secureToken))) {
                    strncpy(g_monitors[i].token, secureToken, sizeof(g_monitors[i].token) - 1);
                    // Securely wipe the temporary stack buffer
                    SecureZeroMemory(secureToken, sizeof(secureToken));
                }
            }
        }
        
        g_monitors[i].refreshInterval = GetPrivateProfileIntA(section, "Refresh", 300, configPath);
        g_monitors[i].enabled = TRUE; 
        
        ParseSourceString(&g_monitors[i]);
    }
    
    Monitor_SetActiveIndex(g_activeIndex);
}

void Monitor_SaveConfig(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    
    char buf[32];
    sprintf(buf, "%d", g_monitorCount);
    WritePrivateProfileStringA("Monitor", "Count", buf, configPath);
    
    sprintf(buf, "%d", g_activeIndex);
    WritePrivateProfileStringA("Monitor", "Active", buf, configPath);
    
    for (int i = 0; i < g_monitorCount; i++) {
        char section[32];
        sprintf(section, "Monitor_%d", i);
        
        char labelUtf8[64];
        WideCharToMultiByte(CP_UTF8, 0, g_monitors[i].label, -1, labelUtf8, 64, NULL, NULL);
        
        WritePrivateProfileStringA(section, "Label", labelUtf8, configPath);
        WritePrivateProfileStringA(section, "Source", g_monitors[i].sourceString, configPath);
        
        if (strlen(g_monitors[i].token) > 0) {
            char targetName[512];
            snprintf(targetName, sizeof(targetName), "Catime:Monitor:%s", g_monitors[i].sourceString);
            Cred_SaveToken(targetName, g_monitors[i].token);
        }
        
        sprintf(buf, "%d", g_monitors[i].refreshInterval);
        WritePrivateProfileStringA(section, "Refresh", buf, configPath);
    }
}

int Monitor_GetConfigCount(void) { return g_monitorCount; }

BOOL Monitor_GetConfigAt(int index, MonitorConfig* outConfig) {
    if (index < 0 || index >= g_monitorCount) return FALSE;
    *outConfig = g_monitors[index];
    return TRUE;
}

void Monitor_AddConfig(const MonitorConfig* config) {
    if (g_monitorCount >= MAX_MONITORS) return;
    g_monitors[g_monitorCount] = *config;
    ParseSourceString(&g_monitors[g_monitorCount]);
    g_monitorCount++;
    Monitor_SaveConfig();
}

void Monitor_UpdateConfigAt(int index, const MonitorConfig* config) {
    if (index < 0 || index >= g_monitorCount) return;
    g_monitors[index] = *config;
    ParseSourceString(&g_monitors[index]);
    Monitor_SaveConfig();
    if (index == g_activeIndex) Monitor_SetActiveIndex(index);
}

void Monitor_DeleteConfigAt(int index) {
    if (index < 0 || index >= g_monitorCount) return;
    
    // Delete credential
    char targetName[512];
    snprintf(targetName, sizeof(targetName), "Catime:Monitor:%s", g_monitors[index].sourceString);
    Cred_DeleteToken(targetName);
    
    // Securely wipe the configuration before deletion to remove token from memory
    SecureZeroMemory(&g_monitors[index], sizeof(MonitorConfig));

    for (int i = index; i < g_monitorCount - 1; i++) g_monitors[i] = g_monitors[i+1];
    
    // Wipe the last element after shifting to ensure no stale data remains at the end
    SecureZeroMemory(&g_monitors[g_monitorCount - 1], sizeof(MonitorConfig));
    
    g_monitorCount--;
    if (g_activeIndex == index) Monitor_SetActiveIndex(-1);
    else if (g_activeIndex > index) { g_activeIndex--; Monitor_SaveConfig(); }
    else Monitor_SaveConfig();
}

void Monitor_SetActiveIndex(int index) {
    g_activeIndex = index;
    
    EnterCriticalSection(&g_cs);
    if (index >= 0 && index < g_monitorCount) {
        g_activeConfig = g_monitors[index];
        g_activeConfig.enabled = TRUE;
        g_state.isLoading = TRUE;
        wcscpy(g_state.displayText, L"...");
    } else {
        g_activeConfig.enabled = FALSE;
    }
    LeaveCriticalSection(&g_cs);
    
    Monitor_SaveConfig();
}

void Monitor_ForceRefresh(void) {
    g_forceRefresh = TRUE;
    EnterCriticalSection(&g_cs);
    if (g_activeConfig.enabled) {
        g_state.isLoading = TRUE;
        wcscpy(g_state.displayText, L"...");
    }
    LeaveCriticalSection(&g_cs);
}

int Monitor_GetActiveIndex(void) { return g_activeIndex; }

static DWORD WINAPI MonitorThreadProc(LPVOID lpParam) {
    LOG_INFO("Monitor thread started");
    Sleep(1000);
    
    static int lastFetchedPreviewIndex = -1;
    
    while (g_running) {
        // Check preview first
        int currentPreviewIdx = -1;
        EnterCriticalSection(&g_cs);
        currentPreviewIdx = g_previewIndex;
        LeaveCriticalSection(&g_cs);

        if (currentPreviewIdx != -1) {
            // Reset force refresh if it was just to wake us up for preview
            if (g_forceRefresh) g_forceRefresh = FALSE;

            if (currentPreviewIdx != lastFetchedPreviewIndex) {
                // If previewing active item, skip fetch
                BOOL isActive = FALSE;
                EnterCriticalSection(&g_cs);
                if (currentPreviewIdx == g_activeIndex && g_activeConfig.enabled) {
                    wcscpy(g_previewStateData.displayText, g_state.displayText);
                    g_previewStateData.isLoading = FALSE;
                    isActive = TRUE;
                }
                LeaveCriticalSection(&g_cs);
                
                if (isActive) {
                    lastFetchedPreviewIndex = currentPreviewIdx;
                    Sleep(100);
                    continue;
                }

                MonitorConfig previewConfig;
                // Safe copy
                EnterCriticalSection(&g_cs);
                previewConfig = g_monitors[currentPreviewIdx];
                LeaveCriticalSection(&g_cs);
                
                long long value = -1;
                switch (previewConfig.type) {
                    case MONITOR_PLATFORM_GITHUB:
                        value = GitHub_FetchValue(&previewConfig);
                        break;
                    default:
                        value = -1;
                        break;
                }
                
                EnterCriticalSection(&g_cs);
                if (g_previewIndex == currentPreviewIdx) {
                    if (value >= 0) {
                        swprintf(g_previewStateData.displayText, 64, L"%lld", value);
                    } else {
                        swprintf(g_previewStateData.displayText, 64, L"Error");
                    }
                    g_previewStateData.isLoading = FALSE;
                    lastFetchedPreviewIndex = currentPreviewIdx;
                }
                LeaveCriticalSection(&g_cs);
            }
            
            Sleep(100);
            continue;
        } else {
            lastFetchedPreviewIndex = -1;
        }

        // Make local copy to avoid holding lock during network op
        MonitorConfig currentConfig;
        BOOL active = FALSE;
        
        // Reset force refresh flag if it was set
        if (g_forceRefresh) g_forceRefresh = FALSE;
        
        EnterCriticalSection(&g_cs);
        if (g_activeConfig.enabled) {
            currentConfig = g_activeConfig;
            active = TRUE;
        }
        LeaveCriticalSection(&g_cs);
        
        if (active) {
            long long value = -1;
            
            switch (currentConfig.type) {
                case MONITOR_PLATFORM_GITHUB:
                    value = GitHub_FetchValue(&currentConfig);
                    break;
                default:
                    value = -1;
                    break;
            }
            
            EnterCriticalSection(&g_cs);
            // Verify config hasn't changed while we were fetching
            // We check sourceString and token to ensure consistency
            if (g_activeConfig.enabled && 
                strcmp(g_activeConfig.sourceString, currentConfig.sourceString) == 0 &&
                strcmp(g_activeConfig.token, currentConfig.token) == 0) {
                
                g_state.isLoading = FALSE;
                g_state.lastUpdateTick = GetTickCount();
                
                if (value >= 0) {
                    g_state.rawValue = value;
                    g_state.isError = FALSE;
                    
                    // User requested to only show the number
                    swprintf(g_state.displayText, 64, L"%lld", value);
                    
                    // Log the current display content (include label in log for clarity)
                    char logText[128];
                    WideCharToMultiByte(CP_UTF8, 0, g_state.displayText, -1, logText, 128, NULL, NULL);
                    LOG_INFO("Monitor updated [%ls]: %s", currentConfig.label, logText);
                } else {
                    g_state.isError = TRUE;
                    swprintf(g_state.displayText, 64, L"Error");
                }
            }
            LeaveCriticalSection(&g_cs);
            
            // Wait loop
            int sleepTime = currentConfig.refreshInterval * 1000;
            if (sleepTime <= 0) sleepTime = 60000;
            
            for (int i = 0; i < sleepTime / 100; i++) {
                if (!g_running) return 0;
                
                if (g_forceRefresh) break; // Break early on force refresh
                
                // Check if config changed mid-sleep to react faster
                EnterCriticalSection(&g_cs);
                BOOL changed = (strcmp(g_activeConfig.sourceString, currentConfig.sourceString) != 0) || 
                               (strcmp(g_activeConfig.token, currentConfig.token) != 0) ||
                               !g_activeConfig.enabled;
                LeaveCriticalSection(&g_cs);
                if (changed) break;
                
                Sleep(100);
            }
        } else {
            // Idle wait if no active monitor
            for (int i = 0; i < 10; i++) {
                if (!g_running) return 0;
                Sleep(100);
            }
        }
    }
    return 0;
}

void Monitor_Init(void) {
    InitializeCriticalSection(&g_cs);
    
    // Initialize defaults
    g_monitorCount = 0;
    g_activeIndex = -1;
    g_activeConfig.enabled = FALSE;
    
    Monitor_LoadConfig();
    
    g_running = TRUE;
    g_hThread = CreateThread(NULL, 0, MonitorThreadProc, NULL, 0, NULL);
    
    if (g_hThread) {
        LOG_INFO("Monitor subsystem initialized.");
    } else {
        LOG_ERROR("Failed to create monitor thread");
    }
}

void Monitor_Shutdown(void) {
    g_running = FALSE;
    if (g_hThread) {
        if (WaitForSingleObject(g_hThread, 2000) == WAIT_TIMEOUT) {
            LOG_WARNING("Monitor thread did not exit within timeout, forcing termination may leak resources.");
        }
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }
    DeleteCriticalSection(&g_cs);
}

BOOL Monitor_GetDisplayText(wchar_t* buffer, size_t maxLen) {
    BOOL hasText = FALSE;
    EnterCriticalSection(&g_cs);
    if (g_activeConfig.enabled) {
        wcsncpy(buffer, g_state.displayText, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasText = TRUE;
    }
    LeaveCriticalSection(&g_cs);
    return hasText && (wcslen(buffer) > 0);
}

void Monitor_SetPreviewIndex(int index) {
    EnterCriticalSection(&g_cs);
    g_previewIndex = index;
    if (index >= 0) {
        if (index == g_activeIndex && g_activeConfig.enabled) {
            // If hovering over active item, show current value immediately without reloading
            wcscpy(g_previewStateData.displayText, g_state.displayText);
            g_previewStateData.isLoading = FALSE;
        } else {
            wcscpy(g_previewStateData.displayText, L"...");
            g_previewStateData.isLoading = TRUE;
            g_forceRefresh = TRUE; // Signal thread to wake up
        }
    }
    LeaveCriticalSection(&g_cs);
}

BOOL Monitor_GetPreviewText(wchar_t* buffer, size_t maxLen) {
    BOOL hasText = FALSE;
    EnterCriticalSection(&g_cs);
    if (g_previewIndex >= 0 && g_previewIndex < g_monitorCount) {
        wcsncpy(buffer, g_previewStateData.displayText, maxLen - 1);
        buffer[maxLen - 1] = L'\0';
        hasText = TRUE;
    }
    LeaveCriticalSection(&g_cs);
    return hasText;
}

BOOL Monitor_ApplyPreviewIfMatching(int index) {
    BOOL applied = FALSE;
    EnterCriticalSection(&g_cs);
    
    // Must match index, and data must be loaded (not loading)
    if (g_previewIndex == index && !g_previewStateData.isLoading) {
        g_activeIndex = index;
        g_activeConfig = g_monitors[index];
        g_activeConfig.enabled = TRUE;
        
        // Copy preview state to active state
        g_state = g_previewStateData;
        
        applied = TRUE;
    }
    LeaveCriticalSection(&g_cs);
    
    if (applied) {
        Monitor_SaveConfig();
    }
    return applied;
}

BOOL Monitor_IsActive(void) {
    BOOL active;
    EnterCriticalSection(&g_cs);
    active = g_activeConfig.enabled;
    LeaveCriticalSection(&g_cs);
    return active;
}
