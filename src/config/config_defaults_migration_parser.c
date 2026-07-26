/**
 * @file config_defaults_migration_parser.c
 * @brief Bounded parser for legacy configuration files.
 */

#include "config_defaults_internal.h"

static void DiscardRestOfMigrationLine(FILE* f) {
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        /* Skip the remainder of an overlong physical line. */
    }
}

ConfigEntry* ConfigDefaults_ReadAllConfigEntries(const char* configPath) {
    ULONGLONG fileSize = 0;
    if (ConfigDefaults_GetMigrationFileSizeUtf8(configPath, &fileSize) &&
        fileSize > CONFIG_MIGRATION_MAX_FILE_BYTES) {
        LOG_WARNING("Config migration skipped oversized config file: %s (%llu bytes, limit %llu bytes)",
                    configPath, fileSize, (ULONGLONG)CONFIG_MIGRATION_MAX_FILE_BYTES);
        return NULL;
    }

    /* Open file for reading (UTF-8) */
    wchar_t wConfigPath[MAX_PATH] = {0};
    if (MultiByteToWideChar(CP_UTF8, 0, configPath, -1, wConfigPath, MAX_PATH) == 0) {
        return NULL;
    }

    FILE* f = _wfopen(wConfigPath, L"rb");
    if (!f) return NULL;

    ConfigEntry* head = NULL;
    ConfigEntry* tail = NULL;
    char currentSection[64] = "";
    char line[4096];
    int parsedLines = 0;
    int parsedEntries = 0;

    /* Skip UTF-8 BOM if present */
    int c1 = fgetc(f);
    int c2 = fgetc(f);
    int c3 = fgetc(f);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        /* Not a BOM, rewind */
        if (c3 != EOF) ungetc(c3, f);
        if (c2 != EOF) ungetc(c2, f);
        if (c1 != EOF) ungetc(c1, f);
    }

    while (fgets(line, sizeof(line), f)) {
        if (++parsedLines > CONFIG_MIGRATION_MAX_PARSE_LINES) {
            LOG_WARNING("Config migration parse line limit reached (%d)",
                        CONFIG_MIGRATION_MAX_PARSE_LINES);
            break;
        }

        size_t rawLen = strlen(line);
        if (rawLen > 0 && line[rawLen - 1] != '\n' && !feof(f)) {
            DiscardRestOfMigrationLine(f);
            LOG_WARNING("Config migration skipped overlong line %d", parsedLines);
            continue;
        }

        /* Detect encoding: if not UTF-8, assume ANSI and convert */
        /* This handles migration from old ANSI config files to new UTF-8 ones */
        if (!ConfigDefaults_IsUtf8String(line)) {
            wchar_t wLine[4096];

            /* ANSI -> Wide */
            int wLen = MultiByteToWideChar(CP_ACP, 0, line, -1, wLine, 4096);
            if (wLen > 0) {
                char utf8Line[4096];
                /* Wide -> UTF-8 */
                int uLen = WideCharToMultiByte(CP_UTF8, 0, wLine, -1, utf8Line, 4096, NULL, NULL);
                if (uLen > 0) {
                    strncpy(line, utf8Line, sizeof(line) - 1);
                    line[sizeof(line) - 1] = '\0';
                }
            }
        }

        /* Remove newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        /* Trim leading whitespace */
        char* trimmed = line;
        while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;

        /* Skip empty lines and comments */
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#') {
            continue;
        }

        /* Section header */
        if (*trimmed == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(currentSection, trimmed + 1, sizeof(currentSection) - 1);
                currentSection[sizeof(currentSection) - 1] = '\0';
            }
            continue;
        }

        /* Key=Value */
        char* eq = strchr(trimmed, '=');
        if (eq && currentSection[0]) {
            if (parsedEntries >= CONFIG_MIGRATION_MAX_PARSE_ENTRIES) {
                LOG_WARNING("Config migration parse entry limit reached (%d)",
                            CONFIG_MIGRATION_MAX_PARSE_ENTRIES);
                break;
            }

            *eq = '\0';

            /* Create new entry */
            ConfigEntry* entry = (ConfigEntry*)calloc(1, sizeof(ConfigEntry));
            if (!entry) {
                fclose(f);
                ConfigDefaults_FreeConfigEntryList(head);
                return NULL;
            }

            /* Copy section, key, value */
            safe_strncpy(entry->section, currentSection, sizeof(entry->section));

            /* Trim key */
            const char* key = trimmed;
            const char* keyEnd = eq - 1;
            while (keyEnd > key && isspace((unsigned char)*keyEnd)) keyEnd--;
            size_t keyLen = keyEnd - key + 1;
            if (keyLen >= sizeof(entry->key)) keyLen = sizeof(entry->key) - 1;
            strncpy(entry->key, key, keyLen);
            entry->key[keyLen] = '\0';

            /* Trim value */
            const char* value = eq + 1;
            while (*value && isspace((unsigned char)*value)) value++;
            safe_strncpy(entry->value, value, sizeof(entry->value));

            /* Add to linked list */
            if (!head) {
                head = tail = entry;
            } else {
                tail->next = entry;
                tail = entry;
            }
            parsedEntries++;
        }
    }

    fclose(f);
    return head;
}
