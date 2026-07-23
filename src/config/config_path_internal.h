#ifndef CONFIG_PATH_INTERNAL_H
#define CONFIG_PATH_INTERNAL_H

#include <windows.h>
#include <stddef.h>

BOOL ConfigPath_IsDirectoryCreateResultOk(int result, const wchar_t* path);
BOOL ConfigPath_BuildFromLocalAppData(const wchar_t* root, char* output,
                                      size_t outputSize);
BOOL ConfigPath_ResolveCiRootW(wchar_t* output, size_t outputSize);
BOOL ConfigPath_BuildFromUserProfile(char* output, size_t outputSize);
BOOL ConfigPath_ResolveEffectiveLocalAppDataW(wchar_t* output,
                                              size_t outputSize);

#endif
