/**
 * @file config_ini_model.c
 * @brief Ordered in-memory INI section and entry model.
 */

#include "config_ini_internal.h"

static void FreeEntry(IniEntry* entry) {
    if (!entry) return;
    free(entry->key);
    free(entry->value);
    free(entry);
}

static void FreeSection(IniSection* section) {
    if (!section) return;
    IniEntry* entry = section->entries;
    while (entry) {
        IniEntry* next = entry->next;
        FreeEntry(entry);
        entry = next;
    }
    free(section->name);
    free(section);
}

void FreeIniFile(IniFile* ini) {
    if (!ini) return;
    IniSection* section = ini->sections;
    while (section) {
        IniSection* next = section->next;
        FreeSection(section);
        section = next;
    }
    free(ini);
}

IniSection* FindSection(IniFile* ini, const char* name) {
    if (!ini || !name) return NULL;
    for (IniSection* section = ini->sections;
         section; section = section->next) {
        if (StrEqualNoCase(section->name, name)) return section;
    }
    return NULL;
}

IniSection* CreateSection(IniFile* ini, const char* name) {
    if (!ini || !name) return NULL;
    IniSection* section = (IniSection*)calloc(1, sizeof(*section));
    if (!section) return NULL;
    section->name = StrDup(name);
    if (!section->name) {
        free(section);
        return NULL;
    }
    if (!ini->sections) {
        ini->sections = section;
        ini->lastSection = section;
    } else {
        IniSection* last = ini->lastSection;
        if (!last) {
            last = ini->sections;
            while (last->next) last = last->next;
        }
        last->next = section;
        ini->lastSection = section;
    }
    return section;
}

IniEntry* FindEntry(IniSection* section, const char* key) {
    if (!section || !key) return NULL;
    for (IniEntry* entry = section->entries; entry; entry = entry->next) {
        if (StrEqualNoCase(entry->key, key)) return entry;
    }
    return NULL;
}

IniEntry* CreateEntry(IniSection* section, const char* key,
                      const char* value) {
    if (!section || !key) return NULL;
    IniEntry* entry = (IniEntry*)calloc(1, sizeof(*entry));
    if (!entry) return NULL;
    entry->key = StrDup(key);
    entry->value = StrDup(value ? value : "");
    if (!entry->key || !entry->value) {
        FreeEntry(entry);
        return NULL;
    }
    if (!section->entries) {
        section->entries = entry;
        section->lastEntry = entry;
    } else {
        IniEntry* last = section->lastEntry;
        if (!last) {
            last = section->entries;
            while (last->next) last = last->next;
        }
        last->next = entry;
        section->lastEntry = entry;
    }
    return entry;
}

IniFile* CloneIniFile(const IniFile* source) {
    if (!source) return NULL;
    IniFile* clone = (IniFile*)calloc(1, sizeof(*clone));
    if (!clone) return NULL;
    safe_strncpy(clone->filePath, source->filePath, sizeof(clone->filePath));
    clone->dirty = source->dirty;
    clone->lastWriteTime = source->lastWriteTime;
    clone->lastStatCheckTick = source->lastStatCheckTick;

    for (const IniSection* section = source->sections;
         section; section = section->next) {
        IniSection* newSection = CreateSection(clone, section->name);
        if (!newSection) {
            FreeIniFile(clone);
            return NULL;
        }
        for (const IniEntry* entry = section->entries;
             entry; entry = entry->next) {
            if (!CreateEntry(newSection, entry->key, entry->value)) {
                FreeIniFile(clone);
                return NULL;
            }
        }
    }
    return clone;
}
