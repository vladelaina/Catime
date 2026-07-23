/**
 * @file config_ini_cache.c
 * @brief Lazy INI cache loading, external-change detection, and mutation.
 */

#include "config_ini_internal.h"

static IniFile* EnsureIniLoaded(const char* filePath) {
    if (!filePath) return NULL;
    if (g_ConfigIni && StrEqualNoCase(g_ConfigIni->filePath, filePath)) {
        return g_ConfigIni;
    }
    FreeIniFile(g_ConfigIni);
    g_ConfigIni = ParseIniFile(filePath);
    return g_ConfigIni;
}

static BOOL IsZeroFileTime(const FILETIME* time) {
    return time && time->dwLowDateTime == 0 && time->dwHighDateTime == 0;
}

static BOOL ReloadIniCacheFromDisk(const char* filePath) {
    IniFile* reloaded = ParseIniFile(filePath);
    if (!reloaded) return FALSE;
    FreeIniFile(g_ConfigIni);
    g_ConfigIni = reloaded;
    return TRUE;
}

static BOOL RefreshCleanIniCacheIfChangedInternal(
    const char* filePath, BOOL forceStatCheck) {
    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) return FALSE;
    if (ini->dirty) return TRUE;

    ULONGLONG now = GetIniCacheTickMs();
    if (!forceStatCheck && ini->lastStatCheckTick &&
        now - ini->lastStatCheckTick < INI_CACHE_STAT_THROTTLE_MS) {
        return TRUE;
    }

    FILETIME currentWriteTime;
    if (!GetFileTimeUtf8(filePath, &currentWriteTime)) {
        if (IsZeroFileTime(&ini->lastWriteTime)) {
            ini->lastStatCheckTick = now;
            return TRUE;
        }
        return ReloadIniCacheFromDisk(filePath);
    }
    if (CompareFileTime(&ini->lastWriteTime, &currentWriteTime) == 0) {
        ini->lastStatCheckTick = now;
        return TRUE;
    }
    return ReloadIniCacheFromDisk(filePath);
}

BOOL RefreshCleanIniCacheIfChanged(const char* filePath) {
    return RefreshCleanIniCacheIfChangedInternal(filePath, FALSE);
}

BOOL RefreshCleanIniCacheForWrite(const char* filePath) {
    return RefreshCleanIniCacheIfChangedInternal(filePath, TRUE);
}

const char* GetIniValue(const char* section, const char* key,
                        const char* filePath) {
    IniFile* ini = EnsureIniLoaded(filePath);
    if (!ini) return NULL;
    IniSection* foundSection = FindSection(ini, section);
    if (!foundSection) return NULL;
    IniEntry* entry = FindEntry(foundSection, key);
    return entry ? entry->value : NULL;
}

BOOL SetIniValueInMemory(IniFile* ini, const char* section,
                         const char* key, const char* value) {
    if (!ini || !section || !key) return FALSE;
    IniSection* foundSection = FindSection(ini, section);
    if (!foundSection) {
        foundSection = CreateSection(ini, section);
        if (!foundSection) return FALSE;
    }
    IniEntry* entry = FindEntry(foundSection, key);
    if (entry) {
        const char* newValue = value ? value : "";
        if (entry->value && strcmp(entry->value, newValue) == 0) return TRUE;
        char* copy = StrDup(newValue);
        if (!copy) return FALSE;
        free(entry->value);
        entry->value = copy;
    } else {
        entry = CreateEntry(foundSection, key, value);
    }
    if (entry) ini->dirty = TRUE;
    return entry != NULL;
}

BOOL IniValueMatches(IniFile* ini, const char* section,
                     const char* key, const char* value) {
    if (!ini || !section || !key) return FALSE;
    IniSection* foundSection = FindSection(ini, section);
    IniEntry* entry = foundSection ? FindEntry(foundSection, key) : NULL;
    return entry && entry->value &&
           strcmp(entry->value, value ? value : "") == 0;
}

BOOL IniUpdatesMatch(IniFile* ini, const IniKeyValue* updates, size_t count) {
    if (!ini || !updates) return FALSE;
    for (size_t i = 0; i < count; ++i) {
        if (!updates[i].section || !updates[i].key || !updates[i].value) {
            continue;
        }
        if (!IniValueMatches(ini, updates[i].section,
                             updates[i].key, updates[i].value)) {
            return FALSE;
        }
    }
    return TRUE;
}
