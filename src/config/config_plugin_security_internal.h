#ifndef CONFIG_PLUGIN_SECURITY_INTERNAL_H
#define CONFIG_PLUGIN_SECURITY_INTERNAL_H

#include "config.h"

#include <windows.h>
#include <stddef.h>

#define PLUGIN_TRUST_VALUE_BUFFER_SIZE (MAX_PATH + 65 + 2)

int PluginTrust_ClampCount(int count);
BOOL PluginTrust_IsValidHash(const char* hash);
BOOL PluginTrust_PathsEqual(const char* first, const char* second);
BOOL PluginTrust_EncodePath(const char* fullPath, char* storedPath,
                            size_t storedPathSize);
void PluginTrust_ExpandPath(const char* storedPath, char* expandedPath,
                            size_t expandedPathSize);
BOOL PluginTrust_CalculateHash(const char* filePath, char* hashHex);

void PluginTrustLock_EnterState(void);
void PluginTrustLock_LeaveState(void);
void PluginTrustLock_EnterMutation(void);
void PluginTrustLock_LeaveMutation(void);

BOOL PluginTrustStore_WriteEntry(int index, const PluginTrustEntry* entry);
BOOL PluginTrustStore_Rewrite(const PluginTrustState* trust);
BOOL PluginTrustStore_Load(PluginTrustState* trust);

#endif /* CONFIG_PLUGIN_SECURITY_INTERNAL_H */
