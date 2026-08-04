#include "config_misc_internal.h"

static BOOL FileExistsUtf8(const char* utf8Path) {
    if (!utf8Path) {
        return FALSE;
    }
    wchar_t widePath[MAX_PATH] = {0};
    if (MultiByteToWideChar(
            CP_UTF8, 0, utf8Path, -1, widePath, MAX_PATH) <= 0) {
        return FALSE;
    }
    return GetFileAttributesW(widePath) != INVALID_FILE_ATTRIBUTES;
}

void LoadRecentFiles(void) {
    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    g_AppConfig.recent_files.count = 0;

    for (int i = 1; i <= MAX_RECENT_FILES; ++i) {
        char key[32];
        char path[MAX_PATH] = {0};
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        if (!ReadIniStringExact(
                INI_SECTION_RECENTFILES, key, "", path,
                sizeof(path), configPath)) {
            LOG_WARNING(
                "Ignoring recent file entry %d because the config value is too long",
                i);
            continue;
        }
        if (path[0] == '\0' || !FileExistsUtf8(path)) {
            continue;
        }

        int index = g_AppConfig.recent_files.count;
        strncpy(g_AppConfig.recent_files.files[index].path,
                path, MAX_PATH - 1);
        g_AppConfig.recent_files.files[index].path[MAX_PATH - 1] = '\0';
        char* filename = strrchr(
            g_AppConfig.recent_files.files[index].path, '\\');
        filename = filename ? filename + 1 :
            g_AppConfig.recent_files.files[index].path;
        strncpy(g_AppConfig.recent_files.files[index].name,
                filename, MAX_PATH - 1);
        g_AppConfig.recent_files.files[index].name[MAX_PATH - 1] = '\0';
        g_AppConfig.recent_files.count++;
    }
}

BOOL SaveRecentFile(const char* filePath) {
    if (!filePath || filePath[0] == '\0' || !FileExistsUtf8(filePath)) {
        return FALSE;
    }

    char configPath[MAX_PATH];
    GetConfigPath(configPath, MAX_PATH);
    const int maxRecent = MAX_RECENT_FILES;
    char currentValues[MAX_RECENT_FILES][MAX_PATH] = {{0}};
    char items[MAX_RECENT_FILES][MAX_PATH] = {{0}};
    int count = 0;
    for (int i = 1; i <= maxRecent; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "CLOCK_RECENT_FILE_%d", i);
        if (!ReadIniStringExact(
                INI_SECTION_RECENTFILES, key, "",
                currentValues[i - 1], MAX_PATH, configPath)) {
            LOG_WARNING("Dropping recent file entry %d because the config value is too long", i);
            currentValues[i - 1][0] = '\0';
            continue;
        }
        if (currentValues[i - 1][0] != '\0') {
            strncpy(items[count], currentValues[i - 1], MAX_PATH - 1);
            items[count][MAX_PATH - 1] = '\0';
            count++;
        }
    }

    char newList[MAX_RECENT_FILES][MAX_PATH] = {{0}};
    int writeIndex = 0;
    strncpy(newList[writeIndex++], filePath, MAX_PATH - 1);
    newList[0][MAX_PATH - 1] = '\0';
    for (int i = 0; i < count && writeIndex < maxRecent; ++i) {
        if (strcmp(items[i], filePath) == 0) continue;
        strncpy(newList[writeIndex], items[i], MAX_PATH - 1);
        newList[writeIndex++][MAX_PATH - 1] = '\0';
    }

    BOOL changed = FALSE;
    for (int i = 0; i < maxRecent; ++i) {
        const char* nextValue = i < writeIndex ? newList[i] : "";
        if (strcmp(currentValues[i], nextValue) != 0) {
            changed = TRUE;
            break;
        }
    }
    if (!changed) return TRUE;

    char keys[MAX_RECENT_FILES][32];
    IniKeyValue updates[MAX_RECENT_FILES];
    for (int i = 0; i < maxRecent; ++i) {
        snprintf(keys[i], sizeof(keys[i]), "CLOCK_RECENT_FILE_%d", i + 1);
        updates[i].section = INI_SECTION_RECENTFILES;
        updates[i].key = keys[i];
        updates[i].value = i < writeIndex ? newList[i] : "";
    }
    if (!WriteIniMultipleAtomic(configPath, updates, MAX_RECENT_FILES)) {
        LOG_WARNING("Failed to persist recent file list after adding: %s",
                    filePath);
        return FALSE;
    }
    return TRUE;
}

char* UTF8ToANSI(const char* utf8String) {
    int wideLength = MultiByteToWideChar(
        CP_UTF8, 0, utf8String, -1, NULL, 0);
    if (wideLength == 0) return _strdup(utf8String);
    wchar_t* wideString = malloc(sizeof(wchar_t) * (size_t)wideLength);
    if (!wideString) return _strdup(utf8String);
    if (MultiByteToWideChar(
            CP_UTF8, 0, utf8String, -1, wideString, wideLength) == 0) {
        free(wideString);
        return _strdup(utf8String);
    }
    int ansiLength = WideCharToMultiByte(
        936, 0, wideString, -1, NULL, 0, NULL, NULL);
    if (ansiLength == 0) {
        free(wideString);
        return _strdup(utf8String);
    }
    char* result = malloc((size_t)ansiLength);
    if (!result) {
        free(wideString);
        return _strdup(utf8String);
    }
    if (WideCharToMultiByte(
            936, 0, wideString, -1, result,
            ansiLength, NULL, NULL) == 0) {
        free(wideString);
        free(result);
        return _strdup(utf8String);
    }
    free(wideString);
    return result;
}
