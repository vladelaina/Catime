/**
 * @file i18n.c
 * @brief Runtime language loader implementation.
 */
#include "i18n.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "paths.h"

static const CatimeLanguageInfo kLanguages[] = {
    {"Chinese_Simplified", "zh_CN",   "zh_CN.ini",   "简体中文"},
    {"Chinese_Traditional","zh-Hant", "zh-Hant.ini", "繁體中文"},
    {"English",            "en",      "en.ini",      "English"},
    {"French",             "fr",      "fr.ini",      "Français"},
    {"German",             "de",      "de.ini",      "Deutsch"},
    {"Japanese",           "ja",      "ja.ini",      "日本語"},
    {"Korean",             "ko",      "ko.ini",      "한국어"},
    {"Portuguese",         "pt",      "pt.ini",      "Português"},
    {"Russian",            "ru",      "ru.ini",      "Русский"},
    {"Spanish",            "es",      "es.ini",      "Español"},
};
static const int kLanguageCount = (int)(sizeof(kLanguages) / sizeof(kLanguages[0]));

int i18n_language_count(void) { return kLanguageCount; }
const CatimeLanguageInfo *i18n_language_info(int index) {
    if (index < 0 || index >= kLanguageCount) return NULL;
    return &kLanguages[index];
}

/* ---- translation hash table (FNV-1a, open addressing) ---- */

#define I18N_BUCKETS 1024
#define I18N_MAX_ENTRIES 700

typedef struct {
    char *key;
    char *val;
} I18nEntry;

static I18nEntry g_entries[I18N_BUCKETS];
static char g_keys[I18N_MAX_ENTRIES][256];
static char g_vals[I18N_MAX_ENTRIES][768];
static int g_entry_count = 0;

static const CatimeLanguageInfo *g_current = &kLanguages[2]; /* English */

static unsigned hash_str(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

static void table_clear(void) {
    /* keys/vals live in fixed buffers indexed by slot, so just zero the table */
    memset(g_entries, 0, sizeof(g_entries));
    g_entry_count = 0;
}

static int table_put(const char *key, const char *val) {
    if (g_entry_count >= I18N_MAX_ENTRIES) return -1;
    unsigned h = hash_str(key) & (I18N_BUCKETS - 1);
    for (int i = 0; i < I18N_BUCKETS; i++) {
        unsigned idx = (h + (unsigned)i) & (I18N_BUCKETS - 1);
        I18nEntry *e = &g_entries[idx];
        if (e->key == NULL) {
            char *kbuf = g_keys[g_entry_count];
            char *vbuf = g_vals[g_entry_count];
            snprintf(kbuf, 256, "%s", key);
            snprintf(vbuf, 768, "%s", val);
            e->key = kbuf;
            e->val = vbuf;
            g_entry_count++;
            return 0;
        }
        if (strcmp(e->key, key) == 0) {
            /* duplicate key: last value wins */
            snprintf(e->val, 768, "%s", val);
            return 0;
        }
    }
    return -1;
}

static const char *table_get(const char *key) {
    unsigned h = hash_str(key) & (I18N_BUCKETS - 1);
    for (int i = 0; i < I18N_BUCKETS; i++) {
        unsigned idx = (h + (unsigned)i) & (I18N_BUCKETS - 1);
        I18nEntry *e = &g_entries[idx];
        if (e->key == NULL) return NULL;
        if (strcmp(e->key, key) == 0) return e->val;
    }
    return NULL;
}

/* ---- value unescaping (\n, \t, \\ only, value side) ---- */

static void unescape_value(char *dst, size_t dst_size, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            char c = src[i + 1];
            if (c == 'n') { dst[j++] = '\n'; i++; }
            else if (c == 't') { dst[j++] = '\t'; i++; }
            else if (c == '\\') { dst[j++] = '\\'; i++; }
            else { dst[j++] = src[i]; }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* ---- line parsing ---- */

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* read a quoted token into out (raw, no unescape). Returns pointer past
 * closing quote, or NULL if the line does not start with a quote. */
static const char *read_quoted(const char *p, char *out, size_t out_size) {
    if (*p != '"') return NULL;
    p++;
    size_t j = 0;
    while (*p && *p != '"') {
        if (j + 1 < out_size) out[j++] = *p;
        p++;
    }
    out[j] = '\0';
    if (*p != '"') return NULL;
    return p + 1;
}

static void parse_buffer(const char *buf, size_t len) {
    const char *line = buf;
    const char *end = buf + len;
    while (line < end) {
        const char *eol = memchr(line, '\n', (size_t)(end - line));
        const char *next = eol ? eol + 1 : end;
        size_t line_len = eol ? (size_t)(eol - line) : (size_t)(end - line);

        /* copy line, strip trailing \r */
        char raw[1536];
        if (line_len >= sizeof(raw)) line_len = sizeof(raw) - 1;
        memcpy(raw, line, line_len);
        raw[line_len] = '\0';
        if (line_len > 0 && raw[line_len - 1] == '\r') raw[line_len - 1] = '\0';

        const char *p = skip_ws(raw);
        if (*p == '\0' || *p == '#' || *p == ';' || *p == '[') {
            line = next;
            continue;
        }

        char key[256];
        char val[1536];
        const char *q = read_quoted(p, key, sizeof(key));
        if (!q) { line = next; continue; }
        q = skip_ws(q);
        if (*q != '=') { line = next; continue; }
        q = skip_ws(q + 1);
        char raw_val[1536];
        q = read_quoted(q, raw_val, sizeof(raw_val));
        if (!q) { line = next; continue; }
        unescape_value(val, sizeof(val), raw_val);
        table_put(key, val);

        line = next;
    }
}

static int load_file(const CatimeLanguageInfo *lang) {
    char rel[256];
    snprintf(rel, sizeof(rel), "languages/%s", lang->file);
    const char *path = paths_find_data(rel);
    if (!path) {
        LOG_WARNING("i18n: language file not found: %s", rel);
        return -1;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_WARNING("i18n: cannot open %s", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 2 * 1024 * 1024) {
        fclose(f);
        LOG_WARNING("i18n: bad size %ld for %s", sz, path);
        return -1;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    /* strip UTF-8 BOM */
    char *start = buf;
    if (rd >= 3 && (unsigned char)start[0] == 0xEF &&
        (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
    }

    table_clear();
    parse_buffer(start, rd - (size_t)(start - buf));
    free(buf);
    LOG_INFO("i18n: loaded %s (%d entries) from %s", lang->locale, g_entry_count, path);
    return 0;
}

int i18n_set_language(const char *config_key) {
    for (int i = 0; i < kLanguageCount; i++) {
        if (strcmp(kLanguages[i].config_key, config_key) == 0) {
            if (load_file(&kLanguages[i]) == 0) {
                g_current = &kLanguages[i];
                return 0;
            }
            /* fall back to English on load failure */
            if (load_file(&kLanguages[2]) == 0) {
                g_current = &kLanguages[2];
                return 0;
            }
            return -1;
        }
    }
    /* unknown key -> English */
    if (load_file(&kLanguages[2]) == 0) g_current = &kLanguages[2];
    return 0;
}

const char *i18n_current_config_key(void) { return g_current->config_key; }
const char *i18n_current_native_name(void) { return g_current->native_name; }

const char *i18n_get(const char *key) {
    if (!key) return "";
    const char *v = table_get(key);
    return v ? v : key;
}

/* ---- locale detection ---- */

static const CatimeLanguageInfo *detect_from_env(void) {
    const char *vars[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    char loc[64] = {0};
    for (int i = 0; i < 3; i++) {
        const char *v = getenv(vars[i]);
        if (v && v[0] && strcmp(v, "C") != 0 && strcmp(v, "POSIX") != 0) {
            snprintf(loc, sizeof(loc), "%s", v);
            break;
        }
    }
    if (!loc[0]) return &kLanguages[2]; /* English */

    /* normalize: lowercase up to '.' or '@' or '_' */
    char norm[64];
    size_t j = 0;
    for (size_t i = 0; loc[i] && j + 1 < sizeof(norm); i++) {
        char c = loc[i];
        if (c == '.' || c == '@') break;
        norm[j++] = (char)tolower((unsigned char)c);
    }
    norm[j] = '\0';

    if (strncmp(norm, "zh_tw", 5) == 0 || strncmp(norm, "zh_hk", 5) == 0 ||
        strncmp(norm, "zh_mo", 5) == 0 || strncmp(norm, "zh-hant", 7) == 0)
        return &kLanguages[1]; /* Traditional */
    if (strncmp(norm, "zh", 2) == 0) return &kLanguages[0]; /* Simplified */

    if (strncmp(norm, "fr", 2) == 0) return &kLanguages[3];
    if (strncmp(norm, "de", 2) == 0) return &kLanguages[4];
    if (strncmp(norm, "ja", 2) == 0) return &kLanguages[5];
    if (strncmp(norm, "ko", 2) == 0) return &kLanguages[6];
    if (strncmp(norm, "pt", 2) == 0) return &kLanguages[7];
    if (strncmp(norm, "ru", 2) == 0) return &kLanguages[8];
    if (strncmp(norm, "es", 2) == 0) return &kLanguages[9];
    return &kLanguages[2]; /* English */
}

void i18n_init(void) {
    const CatimeLanguageInfo *detected = detect_from_env();
    if (load_file(detected) != 0) {
        load_file(&kLanguages[2]);
        g_current = &kLanguages[2];
    } else {
        g_current = detected;
    }
}
