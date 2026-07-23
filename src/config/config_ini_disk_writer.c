/**
 * @file config_ini_disk_writer.c
 * @brief Durable same-directory temporary-file replacement for INI writes.
 */

#include "config_ini_internal.h"

static BOOL WriteIniToFile(const IniFile* ini, const char* filePath) {
    if (!ini || !filePath) return FALSE;
    FILE* file = OpenFileUtf8(filePath, L"wb");
    if (!file) return FALSE;

    BOOL success = TRUE;
    for (const IniSection* section = ini->sections;
         section; section = section->next) {
        if (fprintf(file, "[%s]\n", section->name) < 0) {
            success = FALSE;
            break;
        }
        for (const IniEntry* entry = section->entries;
             entry; entry = entry->next) {
            if (fprintf(file, "%s=%s\n", entry->key, entry->value) < 0) {
                success = FALSE;
                break;
            }
        }
        if (!success) break;
        if (section->next && fprintf(file, "\n") < 0) {
            success = FALSE;
            break;
        }
    }
    if (ferror(file)) success = FALSE;
    if (fclose(file) != 0) success = FALSE;
    if (!success) LOG_ERROR("Failed to write config file: %s", filePath);
    return success;
}

BOOL WriteIniAtomically(IniFile* ini) {
    if (!ini || !ini->filePath[0]) return FALSE;
    char tempPath[MAX_PATH];
    if (!CreateTempFilePathForTargetUtf8(
            ini->filePath, tempPath, sizeof(tempPath))) {
        return FALSE;
    }
    if (!WriteIniToFile(ini, tempPath)) {
        DeleteFileUtf8(tempPath);
        return FALSE;
    }
    if (!MoveFileUtf8(tempPath, ini->filePath)) {
        DeleteFileUtf8(tempPath);
        return FALSE;
    }
    ini->dirty = FALSE;
    GetFileTimeUtf8(ini->filePath, &ini->lastWriteTime);
    ini->lastStatCheckTick = GetIniCacheTickMs();
    return TRUE;
}
