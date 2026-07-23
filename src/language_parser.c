#include "language_internal.h"
#include <stdlib.h>
#include <string.h>

static wchar_t* DuplicateWideSpan(const wchar_t* start, size_t length) {
    if (!start || length >= (size_t)-1 / sizeof(wchar_t)) return NULL;
    wchar_t* copy = (wchar_t*)malloc((length + 1) * sizeof(wchar_t));
    if (!copy) return NULL;
    memcpy(copy, start, length * sizeof(wchar_t));
    copy[length] = L'\0';
    return copy;
}

static const wchar_t* ExtractQuotedString(const wchar_t* start,
                                          wchar_t** output) {
    if (!start || !output) return NULL;
    *output = NULL;
    const wchar_t* first = wcschr(start, L'"');
    if (!first) return NULL;
    const wchar_t* last = wcschr(first + 1, L'"');
    if (!last) return NULL;
    size_t length = (size_t)(last - first - 1);
    if (length >= MAX_STRING_LENGTH) length = MAX_STRING_LENGTH - 1;
    *output = DuplicateWideSpan(first + 1, length);
    return *output ? last + 1 : NULL;
}

static void ProcessEscapeSequences(wchar_t* value) {
    wchar_t* source = value;
    wchar_t* destination = value;
    while (*source) {
        if (*source == L'\\' && source[1]) {
            if (source[1] == L'n') { *destination++ = L'\n'; source += 2; continue; }
            if (source[1] == L't') { *destination++ = L'\t'; source += 2; continue; }
            if (source[1] == L'\\') { *destination++ = L'\\'; source += 2; continue; }
        }
        *destination++ = *source++;
    }
    *destination = L'\0';
}

static BOOL ShouldSkipLine(const wchar_t* line) {
    return !line || line[0] == L'\0' || line[0] == L';' || line[0] == L'[';
}

static BOOL AddTranslationEntry(TranslationTable* table, wchar_t* english,
                                wchar_t* translation, BOOL ownsEnglish,
                                BOOL ownsTranslation) {
    if (!table || !english || !translation || table->count >= MAX_TRANSLATIONS) {
        if (ownsEnglish) free(english);
        if (ownsTranslation) free(translation);
        return FALSE;
    }
    LocalizedString* entry = &table->entries[table->count++];
    entry->english = english;
    entry->translation = translation;
    entry->ownsEnglish = ownsEnglish;
    entry->ownsTranslation = ownsTranslation;
    return TRUE;
}

static BOOL ParseIniLine(TranslationTable* table, const wchar_t* line) {
    if (!table || ShouldSkipLine(line) || table->count >= MAX_TRANSLATIONS)
        return FALSE;
    wchar_t* english = NULL;
    wchar_t* translation = NULL;
    const wchar_t* position = ExtractQuotedString(line, &english);
    if (!position) return FALSE;
    position = wcschr(position, L'=');
    if (!position || !ExtractQuotedString(position, &translation)) {
        free(english);
        return FALSE;
    }
    ProcessEscapeSequences(translation);
    return AddTranslationEntry(table, english, translation, TRUE, TRUE);
}

static BOOL ParseCompactValueLine(TranslationTable* table,
                                  const TranslationTable* keyTable,
                                  int* keyIndex, const wchar_t* line) {
    if (!table || !keyTable || !keyIndex || ShouldSkipLine(line) ||
        table->count >= MAX_TRANSLATIONS) return FALSE;
    while (*keyIndex < keyTable->count &&
           !keyTable->entries[*keyIndex].english) (*keyIndex)++;
    if (*keyIndex >= keyTable->count) return FALSE;
    wchar_t* translation = NULL;
    if (!ExtractQuotedString(line, &translation)) return FALSE;
    ProcessEscapeSequences(translation);
    wchar_t* english = keyTable->entries[(*keyIndex)++].english;
    return AddTranslationEntry(table, english, translation, FALSE, TRUE);
}

void Language_ParseBuffer(TranslationTable* table, char* buffer,
                          const TranslationTable* keyTable) {
    if (!table || !buffer) return;
    wchar_t wideBuffer[MAX_STRING_LENGTH];
    int compactKeyIndex = 0;
    char* context = NULL;
    for (char* line = strtok_s(buffer, "\r\n", &context); line;
         line = strtok_s(NULL, "\r\n", &context)) {
        if (table->count >= MAX_TRANSLATIONS) break;
        if (strncmp(line, "\xEF\xBB\xBF", 3) == 0) line += 3;
        int converted = MultiByteToWideChar(CP_UTF8, 0, line, -1,
                                             wideBuffer, MAX_STRING_LENGTH);
        if (converted > 1 && !ParseIniLine(table, wideBuffer) && keyTable)
            ParseCompactValueLine(table, keyTable, &compactKeyIndex, wideBuffer);
    }
}
