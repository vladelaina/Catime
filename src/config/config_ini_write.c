/**
 * @file config_ini_write.c
 * @brief Thread-safe single and batched atomic INI writes.
 */

#include "config_ini_internal.h"

static BOOL BeginWrite(const char* filePath) {
    if (!AcquireConfigWriteLock()) return FALSE;
    AcquireIniLock();
    if (!RefreshCleanIniCacheForWrite(filePath)) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }
    return TRUE;
}

static BOOL FastSingleValueMatch(const char* filePath, const char* section,
                                 const char* key, const char* value) {
    AcquireIniLock();
    IniFile* current = RefreshCleanIniCacheForWrite(filePath)
        ? g_ConfigIni : NULL;
    BOOL matches = current && !current->dirty &&
                   IniValueMatches(current, section, key, value);
    ReleaseIniLock();
    return matches;
}

BOOL WriteIniString(const char* section, const char* key, const char* value,
                    const char* filePath) {
    if (!section || !key || !filePath) return FALSE;
    if (FastSingleValueMatch(filePath, section, key, value)) return TRUE;
    if (!BeginWrite(filePath)) return FALSE;

    if (g_ConfigIni && !g_ConfigIni->dirty &&
        IniValueMatches(g_ConfigIni, section, key, value)) {
        ReleaseConfigWriteAndIniLocks();
        return TRUE;
    }
    IniFile* pending = CloneIniFile(g_ConfigIni);
    if (!pending) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }

    BOOL result = SetIniValueInMemory(pending, section, key, value);
    if (result && pending->dirty) result = WriteIniAtomically(pending);
    if (result) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = pending;
        pending = NULL;
    }
    FreeIniFile(pending);
    ReleaseConfigWriteAndIniLocks();
    return result;
}

BOOL WriteIniInt(const char* section, const char* key, int value,
                 const char* filePath) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return WriteIniString(section, key, buffer, filePath);
}

BOOL WriteIniBool(const char* section, const char* key, BOOL value,
                  const char* filePath) {
    return WriteIniString(section, key, value ? "TRUE" : "FALSE", filePath);
}

BOOL UpdateConfigKeyValueAtomic(const char* section, const char* key,
                                const char* value) {
    if (!section || !key || !value) return FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return WriteIniString(section, key, value, configPath);
}

BOOL UpdateConfigIntAtomic(const char* section, const char* key, int value) {
    if (!section || !key) return FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return WriteIniInt(section, key, value, configPath);
}

BOOL UpdateConfigBoolAtomic(const char* section, const char* key, BOOL value) {
    if (!section || !key) return FALSE;
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    return WriteIniBool(section, key, value, configPath);
}

static BOOL FastBatchMatch(const char* filePath,
                           const IniKeyValue* updates, size_t count) {
    AcquireIniLock();
    IniFile* current = RefreshCleanIniCacheForWrite(filePath)
        ? g_ConfigIni : NULL;
    BOOL matches = current && !current->dirty &&
                   IniUpdatesMatch(current, updates, count);
    ReleaseIniLock();
    return matches;
}

BOOL WriteIniMultipleAtomic(const char* filePath,
                            const IniKeyValue* updates, size_t count) {
    if (!filePath || !updates || count == 0) return FALSE;
    if (FastBatchMatch(filePath, updates, count)) return TRUE;
    if (!BeginWrite(filePath)) return FALSE;

    if (g_ConfigIni && !g_ConfigIni->dirty &&
        IniUpdatesMatch(g_ConfigIni, updates, count)) {
        ReleaseConfigWriteAndIniLocks();
        return TRUE;
    }
    IniFile* pending = CloneIniFile(g_ConfigIni);
    if (!pending) {
        ReleaseConfigWriteAndIniLocks();
        return FALSE;
    }

    BOOL result = TRUE;
    for (size_t i = 0; i < count; ++i) {
        if (updates[i].section && updates[i].key && updates[i].value &&
            !SetIniValueInMemory(pending, updates[i].section,
                                 updates[i].key, updates[i].value)) {
            result = FALSE;
            break;
        }
    }
    if (result && pending->dirty) result = WriteIniAtomically(pending);
    if (result) {
        FreeIniFile(g_ConfigIni);
        g_ConfigIni = pending;
        pending = NULL;
    }
    FreeIniFile(pending);
    ReleaseConfigWriteAndIniLocks();
    return result;
}
