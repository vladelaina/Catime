/**
 * @file config_ini.c
 * @brief Public INI cache lifecycle and explicit flush operations.
 */

#include "config_ini_internal.h"

BOOL FileExists(const char* filePath) {
    return FileExistsUtf8(filePath);
}

BOOL FlushConfigToDisk(void) {
    if (!AcquireConfigWriteLock()) return FALSE;
    AcquireIniLock();

    BOOL result = TRUE;
    if (g_ConfigIni && g_ConfigIni->dirty) {
        result = WriteIniAtomically(g_ConfigIni);
        if (!result) {
            LOG_ERROR("Failed to flush config cache to disk: %s",
                      g_ConfigIni->filePath);
        }
    }
    ReleaseConfigWriteAndIniLocks();
    return result;
}

void InvalidateIniCache(void) {
    AcquireIniLock();
    if (g_ConfigIni) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = NULL;
    }
    ReleaseIniLock();
}
