#include <windows.h>
#include <string.h>
#include "dialog/dialog_common.h"
#include "dialog/dialog_plugin_security.h"
#include "dialog/dialog_plugin_security_internal.h"
static char g_pluginPath[MAX_PATH] = {0};
static char g_pluginName[128] = {0};
static int g_pluginIndex = -1;
static char g_pluginHash[65] = {0};  /* SHA256 hash at dialog show time */
void SetPluginSecurityDialogInfo(const char* pluginPath, const char* pluginName, int pluginIndex) {
    if (pluginPath) {
        strncpy(g_pluginPath, pluginPath, sizeof(g_pluginPath) - 1);
        g_pluginPath[sizeof(g_pluginPath) - 1] = '\0';
    } else {
        g_pluginPath[0] = '\0';
    }
    if (pluginName) {
        strncpy(g_pluginName, pluginName, sizeof(g_pluginName) - 1);
        g_pluginName[sizeof(g_pluginName) - 1] = '\0';
    } else {
        g_pluginName[0] = '\0';
    }
    g_pluginIndex = pluginIndex;
}
const char* GetPendingPluginPath(void) {
    return g_pluginPath;
}
int GetPendingPluginIndex(void) {
    return g_pluginIndex;
}
void ClearPendingPluginInfo(void) {
    g_pluginPath[0] = '\0';
    g_pluginName[0] = '\0';
    g_pluginIndex = -1;
    g_pluginHash[0] = '\0';
}
void SetPendingPluginHash(const char* hash) {
    if (hash && strlen(hash) == 64) {
        strncpy(g_pluginHash, hash, sizeof(g_pluginHash) - 1);
        g_pluginHash[sizeof(g_pluginHash) - 1] = '\0';
    } else {
        g_pluginHash[0] = '\0';
    }
}
const char* GetPendingPluginHash(void) {
    return g_pluginHash;
}
BOOL IsPluginSecurityDialogOpen(void) {
    return Dialog_IsOpen(DIALOG_INSTANCE_PLUGIN_SECURITY);
}
