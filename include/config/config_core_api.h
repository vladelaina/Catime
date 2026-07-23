/**
 * @file config_core_api.h
 * @brief Configuration paths, loading, persistence, and recent files.
 */

#ifndef CATIME_CONFIG_CORE_API_H
#define CATIME_CONFIG_CORE_API_H

#include <stddef.h>
#include <windows.h>

void GetConfigPath(char* path, size_t size);
BOOL GetEffectiveLocalAppDataPath(char* path, size_t size);
BOOL ExpandEffectiveLocalAppDataPath(const char* value, char* expanded,
                                     size_t expandedSize);
BOOL FileExists(const char* filePath);
void ExtractFileName(const char* path, char* name, size_t nameSize);
void CheckAndCreateResourceFolders(void);

BOOL UpdateConfigKeyValueAtomic(const char* section, const char* key,
                                const char* value);
BOOL UpdateConfigIntAtomic(const char* section, const char* key, int value);
BOOL UpdateConfigBoolAtomic(const char* section, const char* key, BOOL value);

void ReadConfig(void);
void CheckAndCreateAudioFolder(void);
void GetAnimationsFolderPath(char* path, size_t size);
void GetPluginsFolderPath(char* path, size_t size);
BOOL WriteConfigTimeoutAction(const char* action);
BOOL WriteConfigTimeOptions(const char* options);
BOOL WriteConfigDefaultCountdownStartup(int seconds);
void LoadRecentFiles(void);
BOOL SaveRecentFile(const char* filePath);

char* UTF8ToANSI(const char* utf8String);
BOOL CreateDefaultConfig(const char* configPath);
BOOL WriteConfig(const char* configPath);

#endif /* CATIME_CONFIG_CORE_API_H */
