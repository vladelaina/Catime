#include "monitor/utils/credential_manager.h"
#include <wincred.h>
#include <stdio.h>

BOOL Cred_SaveToken(const char* targetName, const char* token) {
    CREDENTIALA cred = {0};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (char*)targetName;
    cred.CredentialBlobSize = (DWORD)strlen(token);
    cred.CredentialBlob = (LPBYTE)token;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteA(&cred, 0);
}

BOOL Cred_LoadToken(const char* targetName, char* tokenBuffer, DWORD bufferSize) {
    PCREDENTIALA pCred;
    if (CredReadA(targetName, CRED_TYPE_GENERIC, 0, &pCred)) {
        DWORD len = pCred->CredentialBlobSize;
        if (len >= bufferSize) len = bufferSize - 1;
        memcpy(tokenBuffer, pCred->CredentialBlob, len);
        tokenBuffer[len] = '\0';
        CredFree(pCred);
        return TRUE;
    }
    return FALSE;
}

BOOL Cred_DeleteToken(const char* targetName) {
    return CredDeleteA(targetName, CRED_TYPE_GENERIC, 0);
}
