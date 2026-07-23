/**
 * @file config_ini_parser.c
 * @brief Bounded UTF-8 INI parsing from disk.
 */

#include "config_ini_internal.h"

static void SkipBom(FILE* file) {
    int first = fgetc(file);
    int second = fgetc(file);
    int third = fgetc(file);
    if (first == 0xEF && second == 0xBB && third == 0xBF) return;
    if (third != EOF) ungetc(third, file);
    if (second != EOF) ungetc(second, file);
    if (first != EOF) ungetc(first, file);
}

static void DiscardRestOfLine(FILE* file) {
    int character;
    while ((character = fgetc(file)) != EOF && character != '\n') {
    }
}

static BOOL ParseEntry(IniFile* ini, IniSection* section, char* line,
                       DWORD* parsedEntries, const char* filePath) {
    char* equals = strchr(line, '=');
    if (!equals || !section) return TRUE;
    *equals = '\0';
    const char* key = TrimWhitespace(line);
    const char* value = TrimWhitespace(equals + 1);
    IniEntry* entry = FindEntry(section, key);
    if (entry) {
        char* newValue = StrDup(value);
        if (!newValue) return TRUE;
        free(entry->value);
        entry->value = newValue;
        return TRUE;
    }
    if (*parsedEntries >= INI_MAX_PARSE_ENTRIES) {
        LOG_WARNING("INI parse entry limit reached for %s (%lu entries)",
                    filePath, *parsedEntries);
        return FALSE;
    }
    if (CreateEntry(section, key, value)) ++*parsedEntries;
    (void)ini;
    return TRUE;
}

IniFile* ParseIniFile(const char* filePath) {
    if (!filePath) return NULL;
    IniFile* ini = (IniFile*)calloc(1, sizeof(*ini));
    if (!ini) return NULL;
    safe_strncpy(ini->filePath, filePath, sizeof(ini->filePath));
    ini->lastStatCheckTick = GetIniCacheTickMs();

    ULONGLONG fileSize = 0;
    if (GetFileSizeUtf8(filePath, &fileSize) &&
        fileSize > INI_MAX_FILE_BYTES) {
        LOG_WARNING("INI file too large, ignoring %s (%llu bytes, limit %llu)",
                    filePath, fileSize, (ULONGLONG)INI_MAX_FILE_BYTES);
        return ini;
    }
    FILE* file = OpenFileUtf8(filePath, L"rb");
    if (!file) return ini;
    SkipBom(file);
    GetFileTimeUtf8(filePath, &ini->lastWriteTime);

    char line[INI_MAX_LINE_LENGTH];
    IniSection* currentSection = NULL;
    DWORD parsedLines = 0;
    DWORD parsedEntries = 0;
    while (fgets(line, sizeof(line), file)) {
        if (++parsedLines > INI_MAX_PARSE_LINES) {
            LOG_WARNING("INI parse line limit reached for %s (%lu lines)",
                        filePath, parsedLines - 1);
            break;
        }
        size_t length = strlen(line);
        if (length && line[length - 1] != '\n' && !feof(file)) {
            DiscardRestOfLine(file);
            LOG_WARNING("INI line too long in %s at line %lu; skipped",
                        filePath, parsedLines);
            continue;
        }
        while (length &&
               (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        char* trimmed = TrimWhitespace(line);
        if (!*trimmed || *trimmed == ';' || *trimmed == '#') continue;
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                const char* name = TrimWhitespace(trimmed + 1);
                currentSection = FindSection(ini, name);
                if (!currentSection) currentSection = CreateSection(ini, name);
            }
            continue;
        }
        if (!ParseEntry(ini, currentSection, trimmed,
                        &parsedEntries, filePath)) {
            break;
        }
    }
    fclose(file);
    return ini;
}
