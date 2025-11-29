/**
 * @file plugin_manager.c
 * @brief Plugin manager implementation
 */

#include "plugin/plugin_manager.h"
#include "plugin/plugin_data.h"
#include "color/gradient.h"
#include "color/color_parser.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <shellapi.h>

/* External function for getting active color */
extern void GetActiveColor(char* outColor, size_t bufferSize);

static PluginInfo g_plugins[MAX_PLUGINS];
static int g_pluginCount = 0;
static CRITICAL_SECTION g_pluginCS;
static HANDLE g_hJob = NULL;

/* Hot-reload monitoring */
static HANDLE g_hHotReloadThread = NULL;
static HWND g_hNotifyWnd = NULL;
static volatile BOOL g_hotReloadRunning = FALSE;
static volatile int g_lastRunningPluginIndex = -1;  /* Track last plugin for continued monitoring */
static volatile int g_activePluginIndex = -1;       /* Currently active plugin (set by user action) */

// Structure to pass data to the launcher thread
typedef struct {
    PluginInfo* plugin;
    HANDLE hReadyEvent;
    BOOL success;
} PluginLauncherArgs;

/* Forward declaration */
static BOOL RestartPluginInternal(int index);

/**
 * @brief Get file modification time
 */
static BOOL GetFileModTime(const char* path, FILETIME* modTime) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    BOOL result = GetFileTime(hFile, NULL, NULL, modTime);
    CloseHandle(hFile);
    return result;
}

/**
 * @brief Hot-reload monitoring thread - checks plugin files every 1 second
 * Also monitors the last running plugin even if it stopped/crashed
 */
static DWORD WINAPI HotReloadThread(LPVOID lpParam) {
    (void)lpParam;
    LOG_INFO("Hot-reload monitoring thread started");
    
    while (g_hotReloadRunning) {
        Sleep(1000);  // Check every 1 second
        
        if (!g_hotReloadRunning) break;
        
        EnterCriticalSection(&g_pluginCS);
        
        BOOL foundRunning = FALSE;
        int indexToMonitor = -1;
        
        /* First, check for any running plugin */
        for (int i = 0; i < g_pluginCount; i++) {
            if (g_plugins[i].isRunning) {
                foundRunning = TRUE;
                indexToMonitor = i;
                g_lastRunningPluginIndex = i;  /* Update last running */
                break;
            }
        }
        
        /* If no running plugin, monitor the last one (for crash recovery) */
        if (!foundRunning && g_lastRunningPluginIndex >= 0 && 
            g_lastRunningPluginIndex < g_pluginCount) {
            indexToMonitor = g_lastRunningPluginIndex;
        }
        
        /* Monitor the selected plugin */
        if (indexToMonitor >= 0) {
            FILETIME currentModTime;
            if (GetFileModTime(g_plugins[indexToMonitor].path, &currentModTime)) {
                if (CompareFileTime(&currentModTime, &g_plugins[indexToMonitor].lastModTime) != 0) {
                    LOG_INFO("Hot-reload: Plugin '%s' file changed, reloading...", 
                             g_plugins[indexToMonitor].displayName);
                    
                    /* Update mod time first to prevent repeated triggers */
                    g_plugins[indexToMonitor].lastModTime = currentModTime;
                    
                    /* Need to leave CS before restart (which acquires CS) */
                    int idx = indexToMonitor;
                    LeaveCriticalSection(&g_pluginCS);
                    
                    RestartPluginInternal(idx);
                    
                    /* Re-enter to continue loop safely */
                    EnterCriticalSection(&g_pluginCS);
                }
            }
        }
        
        LeaveCriticalSection(&g_pluginCS);
    }
    
    LOG_INFO("Hot-reload monitoring thread stopped");
    return 0;
}

/**
 * @brief Thread function to launch and monitor the plugin process
 * 
 * Uses Job Objects to ensure plugin termination when Catime exits.
 */
static DWORD WINAPI PluginLauncherThread(LPVOID lpParam) {
    PluginLauncherArgs* args = (PluginLauncherArgs*)lpParam;
    PluginInfo* plugin = args->plugin;
    
    LOG_INFO("[DEBUG] Launcher thread started for plugin: %s", plugin->displayName);
    LOG_INFO("[DEBUG] Target executable path: '%s'", plugin->path);
    
    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Only support Python scripts
    char cmdLine[MAX_PATH * 2 + 32];
    snprintf(cmdLine, sizeof(cmdLine), "pythonw.exe \"%s\"", plugin->path);
    
    LOG_INFO("[DEBUG] Launching Python script: %s", cmdLine);
    
    if (!CreateProcessA(
        NULL,           // Let system find pythonw.exe in PATH
        cmdLine,        // Command line
        NULL,
        NULL,
        FALSE,
        CREATE_NO_WINDOW, // Ensure no window is created
        NULL,
        NULL,
        &si,
        &plugin->pi
    )) {
        DWORD error = GetLastError();
        LOG_ERROR("[DEBUG] CreateProcess failed! Error: %lu (Path: %s)", error, plugin->path);
        
        if (error == 2) { // ERROR_FILE_NOT_FOUND
             LOG_ERROR("Tip: Make sure Python is installed and 'pythonw.exe' is in your PATH.");
        }

        args->success = FALSE;
        SetEvent(args->hReadyEvent);
        return 0;
    }

    LOG_INFO("[DEBUG] Process created successfully. PID: %lu", plugin->pi.dwProcessId);

    // Duplicate handle for the watcher thread to wait on safely
    // This ensures that even if StopPlugin closes the original handle, we can still wait safely
    HANDLE hWaitProcess = NULL;
    if (!DuplicateHandle(
        GetCurrentProcess(),
        plugin->pi.hProcess,
        GetCurrentProcess(),
        &hWaitProcess,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS
    )) {
        LOG_ERROR("Failed to duplicate process handle");
        hWaitProcess = plugin->pi.hProcess; // Fallback (risky but better than nothing)
    }

    // Assign to Job Object for lifecycle management
    if (g_hJob) {
        if (!AssignProcessToJobObject(g_hJob, plugin->pi.hProcess)) {
            LOG_WARNING("Failed to assign plugin to Job Object, error: %lu", GetLastError());
        }
    }

    // Process created successfully
    plugin->isRunning = TRUE;
    args->success = TRUE;
    SetEvent(args->hReadyEvent); // Signal main thread that PI is ready

    // Record start time for duration check
    DWORD startTime = GetTickCount();

    // Monitor process exit using our private handle
    WaitForSingleObject(hWaitProcess, INFINITE);
    
    // Calculate how long the process ran
    DWORD runDuration = GetTickCount() - startTime;
    
    // Process has exited. Now we need to determine if it was a crash or a manual stop.
    DWORD exitCode = 0;
    GetExitCodeProcess(hWaitProcess, &exitCode);
    LOG_INFO("[DEBUG] Plugin process exited (Exit Code: %lu, Duration: %lu ms)", exitCode, runDuration);

    // Clean up our private handle
    if (hWaitProcess != plugin->pi.hProcess) {
        CloseHandle(hWaitProcess);
    }

    // CRITICAL: Check if we need to clean up (Unexpected Exit)
    EnterCriticalSection(&g_pluginCS);
    if (plugin->isRunning) {
        // If isRunning is still TRUE, it means StopPlugin wasn't called.
        // Plugin exited on its own (finished execution or crashed).
        LOG_INFO("Plugin %s exited on its own", plugin->displayName);
        
        if (plugin->pi.hProcess) CloseHandle(plugin->pi.hProcess);
        if (plugin->pi.hThread) CloseHandle(plugin->pi.hThread);
        
        plugin->isRunning = FALSE;
        memset(&plugin->pi, 0, sizeof(plugin->pi));
        
        // If process ran for less than 5 seconds, it might be UAC elevation scenario.
        // Keep hot-reload active so user can retry after granting permission.
        if (runDuration < 5000) {
            LOG_INFO("Plugin exited quickly (%lu ms), keeping hot-reload active (possible UAC)", runDuration);
            // Don't clear indices - hot-reload will restart when file changes
        } else {
            // Normal completion - clear tracking indices to stop hot-reload monitoring
            g_lastRunningPluginIndex = -1;
            g_activePluginIndex = -1;
            
            // Clear plugin data and exit plugin mode
            PluginData_Clear();
        }
        
        // Force UI refresh
        if (g_hNotifyWnd) {
            InvalidateRect(g_hNotifyWnd, NULL, TRUE);
        }
    } else {
        // If isRunning is FALSE, StopPlugin handled it. We do nothing.
        LOG_INFO("Plugin monitor thread detected graceful stop.");
    }
    LeaveCriticalSection(&g_pluginCS);
    
    LOG_INFO("[DEBUG] Launcher thread exiting for plugin: %s", plugin->displayName);
    return 0;
}

/**
 * @brief Extract display name from plugin filename (keep extension)
 */
static void ExtractDisplayName(const char* filename, char* displayName, size_t bufferSize) {
    strncpy(displayName, filename, bufferSize - 1);
    displayName[bufferSize - 1] = '\0';
}

void PluginManager_Init(void) {
    InitializeCriticalSection(&g_pluginCS);
    memset(g_plugins, 0, sizeof(g_plugins));
    g_pluginCount = 0;

    // Create Job Object for automatic cleanup
    g_hJob = CreateJobObject(NULL, NULL);
    if (g_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(g_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
            LOG_ERROR("Failed to set Job Object info, error: %lu", GetLastError());
            CloseHandle(g_hJob);
            g_hJob = NULL;
        }
    } else {
        LOG_ERROR("Failed to create Job Object, error: %lu", GetLastError());
    }

    /* Start hot-reload monitoring thread */
    g_hotReloadRunning = TRUE;
    g_hHotReloadThread = CreateThread(NULL, 0, HotReloadThread, NULL, 0, NULL);
    if (!g_hHotReloadThread) {
        LOG_WARNING("Failed to start hot-reload thread");
    }

    LOG_INFO("Plugin manager initialized");
}

void PluginManager_Shutdown(void) {
    /* Stop hot-reload thread first */
    if (g_hHotReloadThread) {
        g_hotReloadRunning = FALSE;
        WaitForSingleObject(g_hHotReloadThread, 2000);
        CloseHandle(g_hHotReloadThread);
        g_hHotReloadThread = NULL;
    }

    EnterCriticalSection(&g_pluginCS);

    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            PluginManager_StopPlugin(i);
        }
    }

    if (g_hJob) {
        CloseHandle(g_hJob);
        g_hJob = NULL;
    }
    
    // Reset tracking indices
    g_activePluginIndex = -1;
    g_lastRunningPluginIndex = -1;
    g_pluginCount = 0;

    LeaveCriticalSection(&g_pluginCS);
    DeleteCriticalSection(&g_pluginCS);
    LOG_INFO("Plugin manager shutdown");
}

BOOL PluginManager_GetPluginDir(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }

    DWORD result = ExpandEnvironmentStringsA(PLUGIN_DIR_PATH, buffer, (DWORD)bufferSize);
    if (result == 0 || result > bufferSize) {
        LOG_ERROR("Failed to expand plugin directory path");
        return FALSE;
    }

    return TRUE;
}

int PluginManager_ScanPlugins(void) {
    char pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDir(pluginDir, sizeof(pluginDir))) {
        return 0;
    }

    LOG_INFO("Scanning plugin directory: %s", pluginDir);

    EnterCriticalSection(&g_pluginCS);

    // Scan into temporary list to preserve state
    PluginInfo newPlugins[MAX_PLUGINS];
    int newPluginCount = 0;
    memset(newPlugins, 0, sizeof(newPlugins));

    // Scan for script files (source-visible, no compiled executables)
    // Sorted alphabetically by language name
    const char* extensions[] = {
        "*.agda",                   // Agda
        "*.apl",                    // APL
        "*.applescript",            // AppleScript
        "*.ahk", "*.ahk2",          // AutoHotkey
        "*.au3",                    // AutoIt
        "*.awk",                    // AWK
        "*.bal",                    // Ballerina
        "*.bat", "*.cmd",           // Batch
        "*.boo",                    // Boo
        "*.ck",                     // ChucK
        "*.clj", "*.cljs", "*.cljc",// Clojure
        "*.coffee",                 // CoffeeScript
        "*.lisp", "*.lsp", "*.cl",  // Common Lisp
        "*.cr",                     // Crystal
        "*.d",                      // D
        "*.dart",                   // Dart
        "*.ex", "*.exs",            // Elixir
        "*.elm",                    // Elm
        "*.el",                     // Emacs Lisp
        "*.erl", "*.escript",       // Erlang
        "*.fs", "*.fsx",            // F#
        "*.factor",                 // Factor
        "*.fnl",                    // Fennel
        "*.forth", "*.4th", "*.fth",// Forth
        "*.go",                     // Go
        "*.groovy", "*.gvy",        // Groovy
        "*.hack", "*.hh",           // Hack
        "*.hs", "*.lhs",            // Haskell
        "*.hx",                     // Haxe
        "*.hy",                     // Hy
        "*.idr",                    // Idris
        "*.io",                     // Io
        "*.ijs",                    // J
        "*.janet",                  // Janet
        "*.js", "*.mjs", "*.cjs",   // JavaScript
        "*.jl",                     // Julia
        "*.k", "*.q",               // K/Q
        "*.kts",                    // Kotlin Script
        "*.lean",                   // Lean
        "*.lua",                    // Lua
        "*.m", "*.wl",              // MATLAB/Mathematica
        "*.moon",                   // MoonScript
        "*.nims", "*.nimble",       // Nim
        "*.ml", "*.mli",            // OCaml
        "*.pl", "*.pm", "*.perl",   // Perl
        "*.p6", "*.pl6", "*.raku",  // Perl 6/Raku
        "*.php", "*.php5", "*.php7",// PHP
        "*.pike",                   // Pike
        "*.pony",                   // Pony
        "*.ps1",                    // PowerShell
        "*.pde",                    // Processing
        "*.pro",                    // Prolog
        "*.purs",                   // PureScript
        "*.py", "*.pyw",            // Python
        "*.r", "*.R", "*.Rscript",  // R
        "*.rkt", "*.scm", "*.ss",   // Racket/Scheme
        "*.red", "*.reds",          // Red
        "*.rexx", "*.rex",          // Rexx
        "*.rb", "*.rbw",            // Ruby
        "*.scala", "*.sc",          // Scala
        "*.sed",                    // sed
        "*.sh", "*.bash", "*.zsh",  // Shell
        "*.ksh", "*.csh", "*.fish", // Shell (more)
        "*.st",                     // Smalltalk
        "*.sml",                    // Standard ML
        "*.swift",                  // Swift
        "*.tcl", "*.tk",            // Tcl/Tk
        "*.ts", "*.mts",            // TypeScript
        "*.v", "*.vsh",             // V
        "*.vbs", "*.wsf",           // VBScript/WSH
        "*.wren",                   // Wren
        "*.zig"                     // Zig
    };
    int extCount = sizeof(extensions) / sizeof(extensions[0]);
    for (int ext = 0; ext < extCount; ext++) {
        char searchPath[MAX_PATH];
        snprintf(searchPath, sizeof(searchPath), "%s\\%s", pluginDir, extensions[ext]);

        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath, &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (newPluginCount >= MAX_PLUGINS) {
                    LOG_WARNING("Maximum plugin count reached (%d)", MAX_PLUGINS);
                    break;
                }

                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    PluginInfo* plugin = &newPlugins[newPluginCount];

                    strncpy(plugin->name, findData.cFileName, sizeof(plugin->name) - 1);
                    plugin->name[sizeof(plugin->name) - 1] = '\0';
                    ExtractDisplayName(findData.cFileName, plugin->displayName, sizeof(plugin->displayName));
                    snprintf(plugin->path, sizeof(plugin->path), "%s\\%s", pluginDir, findData.cFileName);
                    
                    plugin->isRunning = FALSE;
                    memset(&plugin->pi, 0, sizeof(plugin->pi));

                    // Preserve state from existing list
                    for (int i = 0; i < g_pluginCount; i++) {
                        if (strcmp(g_plugins[i].name, plugin->name) == 0) {
                            plugin->isRunning = g_plugins[i].isRunning;
                            plugin->pi = g_plugins[i].pi;
                            plugin->lastModTime = g_plugins[i].lastModTime;
                            break;
                        }
                    }

                    LOG_INFO("Found plugin: %s (%s)", plugin->displayName, plugin->name);
                    newPluginCount++;
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
    }

    // Clean up orphaned plugins (running but file removed)
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            BOOL found = FALSE;
            for (int j = 0; j < newPluginCount; j++) {
                if (strcmp(g_plugins[i].name, newPlugins[j].name) == 0) {
                    found = TRUE;
                    break;
                }
            }
            if (!found) {
                LOG_INFO("Plugin file %s removed, stopping process", g_plugins[i].name);
                TerminateProcess(g_plugins[i].pi.hProcess, 0);
                CloseHandle(g_plugins[i].pi.hProcess);
                CloseHandle(g_plugins[i].pi.hThread);
            }
        }
    }

    // Remember old plugin names for re-mapping indices
    char lastRunningName[64] = {0};
    char activePluginName[64] = {0};
    if (g_lastRunningPluginIndex >= 0 && g_lastRunningPluginIndex < g_pluginCount) {
        strncpy(lastRunningName, g_plugins[g_lastRunningPluginIndex].name, sizeof(lastRunningName) - 1);
    }
    if (g_activePluginIndex >= 0 && g_activePluginIndex < g_pluginCount) {
        strncpy(activePluginName, g_plugins[g_activePluginIndex].name, sizeof(activePluginName) - 1);
    }

    // Update global list
    memcpy(g_plugins, newPlugins, sizeof(g_plugins));
    g_pluginCount = newPluginCount;
    
    // Re-map g_lastRunningPluginIndex to new list
    if (lastRunningName[0]) {
        g_lastRunningPluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (strcmp(g_plugins[i].name, lastRunningName) == 0) {
                g_lastRunningPluginIndex = i;
                break;
            }
        }
    }
    
    // Re-map g_activePluginIndex to new list
    if (activePluginName[0]) {
        g_activePluginIndex = -1;  // Reset first
        for (int i = 0; i < g_pluginCount; i++) {
            if (strcmp(g_plugins[i].name, activePluginName) == 0) {
                g_activePluginIndex = i;
                break;
            }
        }
    }

    LeaveCriticalSection(&g_pluginCS);

    LOG_INFO("Plugin scan complete, found %d plugins", g_pluginCount);
    return g_pluginCount;
}

/* Async scan thread */
static volatile LONG g_asyncScanPending = 0;

static DWORD WINAPI AsyncScanThread(LPVOID lpParam) {
    (void)lpParam;
    PluginManager_ScanPlugins();
    // Reset flag after scan completes
    InterlockedExchange(&g_asyncScanPending, 0);
    return 0;
}

void PluginManager_RequestScanAsync(void) {
    /* Avoid multiple concurrent scans */
    if (InterlockedCompareExchange(&g_asyncScanPending, 1, 0) != 0) {
        return;
    }
    
    HANDLE hThread = CreateThread(NULL, 0, AsyncScanThread, NULL, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        // Failed to create thread, reset flag
        InterlockedExchange(&g_asyncScanPending, 0);
    }
}

int PluginManager_GetPluginCount(void) {
    int count;
    EnterCriticalSection(&g_pluginCS);
    count = g_pluginCount;
    LeaveCriticalSection(&g_pluginCS);
    return count;
}

const PluginInfo* PluginManager_GetPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return NULL;
    }
    return &g_plugins[index];
}

BOOL PluginManager_StartPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    // Exclusive execution: Stop ALL other plugins first
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            // If it's the same plugin and it's already running, do nothing (or restart?)
            // User request implies "Switching", so if I click the same one, maybe just keep it running?
            // Let's assume clicking an active plugin does nothing or ensures it's running.
            if (i == index) {
                LOG_INFO("Plugin %s is already running", g_plugins[index].displayName);
                LeaveCriticalSection(&g_pluginCS);
                return TRUE;
            }
            // Stop others
            PluginManager_StopPlugin(i);
        }
    }

    PluginInfo* plugin = &g_plugins[index];
    LOG_INFO("[DEBUG] StartPlugin requested for: %s (Index: %d)", plugin->displayName, index);

    // Prepare arguments for the launcher thread
    PluginLauncherArgs args = {0};
    args.plugin = plugin;
    args.hReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    args.success = FALSE;

    if (!args.hReadyEvent) {
        LOG_ERROR("[DEBUG] Failed to create synchronization event. Error: %lu", GetLastError());
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    LOG_INFO("[DEBUG] Creating launcher thread...");

    // Spawn the launcher thread
    // We don't keep the thread handle because it runs its own loop until plugin exit
    HANDLE hThread = CreateThread(
        NULL, 
        0, 
        PluginLauncherThread, 
        &args, 
        0, 
        NULL
    );

    if (!hThread) {
        LOG_ERROR("[DEBUG] Failed to create plugin launcher thread. Error: %lu", GetLastError());
        CloseHandle(args.hReadyEvent);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    // Wait for the process to be created (or fail)
    LOG_INFO("[DEBUG] Waiting for plugin process to initialize...");
    WaitForSingleObject(args.hReadyEvent, INFINITE);
    CloseHandle(args.hReadyEvent);
    CloseHandle(hThread); // We don't need to control the thread, let it run

    if (!args.success) {
        LOG_ERROR("[DEBUG] Launcher thread reported failure.");
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    /* Record file modification time for hot-reload detection */
    GetFileModTime(plugin->path, &plugin->lastModTime);
    
    /* Mark this as the active plugin */
    g_activePluginIndex = index;

    LOG_INFO("[DEBUG] Plugin started successfully: %s (PID: %lu)", plugin->displayName, plugin->pi.dwProcessId);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

/**
 * @brief Internal restart function for hot-reload (shows Loading message)
 */
static BOOL RestartPluginInternal(int index) {
    if (index < 0 || index >= g_pluginCount) return FALSE;
    
    PluginInfo* plugin = &g_plugins[index];
    
    /* Stop the plugin first */
    PluginManager_StopPlugin(index);
    
    /* Show "Loading..." message */
    wchar_t loadingText[256];
    wchar_t displayNameW[128];
    MultiByteToWideChar(CP_UTF8, 0, plugin->displayName, -1, displayNameW, 128);
    _snwprintf(loadingText, 256, L"Loading %s...", displayNameW);
    PluginData_SetText(loadingText);
    PluginData_SetActive(TRUE);
    
    /* Force redraw to show loading message */
    if (g_hNotifyWnd) {
        InvalidateRect(g_hNotifyWnd, NULL, TRUE);
        
        /* Check if animated gradient needs timer for smooth animation */
        char activeColor[COLOR_HEX_BUFFER];
        GetActiveColor(activeColor, sizeof(activeColor));
        if (IsGradientAnimated(GetGradientTypeByName(activeColor))) {
            SetTimer(g_hNotifyWnd, 1, 66, NULL);  /* 15 FPS for smooth animation */
        }
    }
    
    /* Small delay to ensure process cleanup */
    Sleep(100);
    
    /* Start the plugin again */
    return PluginManager_StartPlugin(index);
}

BOOL PluginManager_StopPlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    if (!plugin->isRunning) {
        LOG_WARNING("Plugin %s is not running", plugin->displayName);
        LeaveCriticalSection(&g_pluginCS);
        return FALSE;
    }

    // Try graceful termination first
    if (!TerminateProcess(plugin->pi.hProcess, 0)) {
        DWORD error = GetLastError();
        LOG_WARNING("Failed to terminate plugin %s, error: %lu", plugin->displayName, error);
    }

    // Wait for process to exit (with timeout)
    WaitForSingleObject(plugin->pi.hProcess, 2000);

    CloseHandle(plugin->pi.hProcess);
    CloseHandle(plugin->pi.hThread);

    plugin->isRunning = FALSE;
    memset(&plugin->pi, 0, sizeof(plugin->pi));

    // Clear any data generated by this plugin
    PluginData_Clear();
    
    // Reset last running index to stop hot-reload monitoring
    g_lastRunningPluginIndex = -1;
    
    // Clear active plugin index
    g_activePluginIndex = -1;

    LOG_INFO("Stopped plugin: %s", plugin->displayName);

    LeaveCriticalSection(&g_pluginCS);
    return TRUE;
}

BOOL PluginManager_TogglePlugin(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    if (g_plugins[index].isRunning) {
        return PluginManager_StopPlugin(index);
    } else {
        return PluginManager_StartPlugin(index);
    }
}

BOOL PluginManager_IsPluginRunning(int index) {
    if (index < 0 || index >= g_pluginCount) {
        return FALSE;
    }

    BOOL isRunning;
    EnterCriticalSection(&g_pluginCS);

    PluginInfo* plugin = &g_plugins[index];

    if (plugin->isRunning) {
        // Check if process is still alive
        DWORD exitCode;
        if (GetExitCodeProcess(plugin->pi.hProcess, &exitCode)) {
            if (exitCode != STILL_ACTIVE) {
                plugin->isRunning = FALSE;
                CloseHandle(plugin->pi.hProcess);
                CloseHandle(plugin->pi.hThread);
                memset(&plugin->pi, 0, sizeof(plugin->pi));
                LOG_INFO("Plugin %s has exited", plugin->displayName);
            }
        }
    }

    isRunning = plugin->isRunning;
    LeaveCriticalSection(&g_pluginCS);

    return isRunning;
}

void PluginManager_StopAllPlugins(void) {
    EnterCriticalSection(&g_pluginCS);
    for (int i = 0; i < g_pluginCount; i++) {
        if (g_plugins[i].isRunning) {
            PluginManager_StopPlugin(i);
        }
    }
    // Always clear indices (even if no plugin was running)
    // This handles cases where plugin already exited (e.g., after <exit> tag)
    g_activePluginIndex = -1;
    g_lastRunningPluginIndex = -1;
    LeaveCriticalSection(&g_pluginCS);
}

BOOL PluginManager_OpenPluginFolder(void) {
    char pluginDir[MAX_PATH];
    if (!PluginManager_GetPluginDir(pluginDir, sizeof(pluginDir))) {
        return FALSE;
    }
    
    // Ensure directory exists
    CreateDirectoryA(pluginDir, NULL);
    
    // Open in explorer
    ShellExecuteA(NULL, "open", pluginDir, NULL, NULL, SW_SHOW);
    return TRUE;
}

void PluginManager_SetNotifyWindow(HWND hwnd) {
    g_hNotifyWnd = hwnd;
}

int PluginManager_GetActivePluginIndex(void) {
    return g_activePluginIndex;
}
