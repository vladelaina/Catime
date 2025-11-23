#ifndef CREDENTIAL_MANAGER_H
#define CREDENTIAL_MANAGER_H

#include <windows.h>

BOOL Cred_SaveToken(const char* targetName, const char* token);
BOOL Cred_LoadToken(const char* targetName, char* tokenBuffer, DWORD bufferSize);
BOOL Cred_DeleteToken(const char* targetName);

#endif
