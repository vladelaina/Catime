/**
 * @file language.c
 * @brief 多语言支持模块实现文件
 * 
 * 本文件实现了应用程序的多语言支持功能，包含语言检测和本地化字符串处理。
 * 翻译内容作为资源嵌入到可执行文件中。
 */

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "../include/language.h"
#include "../resource/resource.h"

/// 全局语言变量，存储当前应用语言设置
AppLanguage CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // 默认英语

/// 全局哈希表，用于存储当前语言的翻译
#define MAX_TRANSLATIONS 200
#define MAX_STRING_LENGTH 1024

// 语言资源ID (定义在languages.rc中)
#define LANG_EN_INI       1001  // 对应languages/en.ini
#define LANG_ZH_CN_INI    1002  // 对应languages/zh_CN.ini
#define LANG_ZH_TW_INI    1003  // 对应languages/zh-Hant.ini
#define LANG_ES_INI       1004  // 对应languages/es.ini
#define LANG_FR_INI       1005  // 对应languages/fr.ini
#define LANG_DE_INI       1006  // 对应languages/de.ini
#define LANG_RU_INI       1007  // 对应languages/ru.ini
#define LANG_PT_INI       1008  // 对应languages/pt.ini
#define LANG_JA_INI       1009  // 对应languages/ja.ini
#define LANG_KO_INI       1010  // 对应languages/ko.ini

/**
 * @brief 定义语言字符串键值对结构
 */
typedef struct {
    wchar_t english[MAX_STRING_LENGTH];  // 英文键
    wchar_t translation[MAX_STRING_LENGTH];  // 翻译值
} LocalizedString;

static LocalizedString g_translations[MAX_TRANSLATIONS];
static int g_translation_count = 0;

/**
 * @brief 获取语言对应的资源ID
 * 
 * @param language 语言枚举值
 * @return UINT 对应的资源ID
 */
static UINT GetLanguageResourceID(AppLanguage language) {
    switch (language) {
        case APP_LANG_CHINESE_SIMP:
            return LANG_ZH_CN_INI;
        case APP_LANG_CHINESE_TRAD:
            return LANG_ZH_TW_INI;
        case APP_LANG_SPANISH:
            return LANG_ES_INI;
        case APP_LANG_FRENCH:
            return LANG_FR_INI;
        case APP_LANG_GERMAN:
            return LANG_DE_INI;
        case APP_LANG_RUSSIAN:
            return LANG_RU_INI;
        case APP_LANG_PORTUGUESE:
            return LANG_PT_INI;
        case APP_LANG_JAPANESE:
            return LANG_JA_INI;
        case APP_LANG_KOREAN:
            return LANG_KO_INI;
        case APP_LANG_ENGLISH:
        default:
            return LANG_EN_INI;
    }
}

/**
 * @brief 将UTF-8字符串转换为宽字符（UTF-16）字符串
 * 
 * @param utf8 UTF-8字符串
 * @param wstr 输出的宽字符串缓冲区
 * @param wstr_size 缓冲区大小（字符数）
 * @return int 转换后的字符数，失败返回-1
 */
static int UTF8ToWideChar(const char* utf8, wchar_t* wstr, int wstr_size) {
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, wstr_size) - 1;
}

/**
 * @brief 解析ini文件中的一行
 * 
 * @param line ini文件中的一行
 * @return int 是否解析成功（1成功，0失败）
 */
static int ParseIniLine(const wchar_t* line) {
    // 跳过空行和注释行
    if (line[0] == L'\0' || line[0] == L';' || line[0] == L'[') {
        return 0;
    }

    // 寻找第一个引号和最后一个引号之间的内容作为key
    const wchar_t* key_start = wcschr(line, L'"');
    if (!key_start) return 0;
    key_start++; // 跳过第一个引号

    const wchar_t* key_end = wcschr(key_start, L'"');
    if (!key_end) return 0;

    // 寻找等号后面的第一个引号和最后一个引号之间的内容作为value
    const wchar_t* value_start = wcschr(key_end + 1, L'=');
    if (!value_start) return 0;
    
    value_start = wcschr(value_start, L'"');
    if (!value_start) return 0;
    value_start++; // 跳过第一个引号

    const wchar_t* value_end = wcsrchr(value_start, L'"');
    if (!value_end) return 0;

    // 复制key
    size_t key_len = key_end - key_start;
    if (key_len >= MAX_STRING_LENGTH) key_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].english, key_start, key_len);
    g_translations[g_translation_count].english[key_len] = L'\0';

    // 复制value
    size_t value_len = value_end - value_start;
    if (value_len >= MAX_STRING_LENGTH) value_len = MAX_STRING_LENGTH - 1;
    wcsncpy(g_translations[g_translation_count].translation, value_start, value_len);
    g_translations[g_translation_count].translation[value_len] = L'\0';

    g_translation_count++;
    return 1;
}

/**
 * @brief 从资源加载指定语言的翻译
 * 
 * @param language 语言枚举值
 * @return int 是否加载成功
 */
static int LoadLanguageResource(AppLanguage language) {
    UINT resourceID = GetLanguageResourceID(language);
    
    // 重置翻译计数
    g_translation_count = 0;
    
    // 查找资源
    HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hResInfo) {
        // 如果找不到，检查是否是中文并返回
        if (language == APP_LANG_CHINESE_SIMP || language == APP_LANG_CHINESE_TRAD) {
            return 0;
        }
        
        // 非中文，加载英文作为后备
        if (language != APP_LANG_ENGLISH) {
            return LoadLanguageResource(APP_LANG_ENGLISH);
        }
        
        return 0;
    }
    
    // 获取资源大小
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        return 0;
    }
    
    // 加载资源
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        return 0;
    }
    
    // 锁定资源获取指针
    const char* pData = (const char*)LockResource(hResData);
    if (!pData) {
        return 0;
    }
    
    // 创建内存缓冲区副本
    char* buffer = (char*)malloc(dwSize + 1);
    if (!buffer) {
        return 0;
    }
    
    // 复制资源数据到缓冲区
    memcpy(buffer, pData, dwSize);
    buffer[dwSize] = '\0';
    
    // 按行分割并解析
    char* line = strtok(buffer, "\r\n");
    wchar_t wide_buffer[MAX_STRING_LENGTH];
    
    while (line && g_translation_count < MAX_TRANSLATIONS) {
        // 跳过空行和BOM标记
        if (line[0] == '\0' || (line[0] == (char)0xEF && line[1] == (char)0xBB && line[2] == (char)0xBF)) {
            line = strtok(NULL, "\r\n");
            continue;
        }
        
        // 转换为宽字符
        if (UTF8ToWideChar(line, wide_buffer, MAX_STRING_LENGTH) > 0) {
            ParseIniLine(wide_buffer);
        }
        
        line = strtok(NULL, "\r\n");
    }
    
    free(buffer);
    return 1;
}

/**
 * @brief 在全局翻译表中查找对应翻译
 * 
 * @param english 英文原文
 * @return const wchar_t* 找到的翻译，如果未找到则返回NULL
 */
static const wchar_t* FindTranslation(const wchar_t* english) {
    for (int i = 0; i < g_translation_count; i++) {
        if (wcscmp(english, g_translations[i].english) == 0) {
            return g_translations[i].translation;
        }
    }
    return NULL;
}

/**
 * @brief 初始化应用程序语言环境
 * 
 * 根据系统语言自动检测并设置应用程序的当前语言。
 * 支持检测简体中文、繁体中文及其他预设语言。
 */
static void DetectSystemLanguage() {
    LANGID langID = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(langID)) {
        case LANG_CHINESE:
            // 区分简繁体中文
            if (SUBLANGID(langID) == SUBLANG_CHINESE_SIMPLIFIED) {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_SIMP;
            } else {
                CURRENT_LANGUAGE = APP_LANG_CHINESE_TRAD;
            }
            break;
        case LANG_SPANISH:
            CURRENT_LANGUAGE = APP_LANG_SPANISH;
            break;
        case LANG_FRENCH:
            CURRENT_LANGUAGE = APP_LANG_FRENCH;
            break;
        case LANG_GERMAN:
            CURRENT_LANGUAGE = APP_LANG_GERMAN;
            break;
        case LANG_RUSSIAN:
            CURRENT_LANGUAGE = APP_LANG_RUSSIAN;
            break;
        case LANG_PORTUGUESE:
            CURRENT_LANGUAGE = APP_LANG_PORTUGUESE;
            break;
        case LANG_JAPANESE:
            CURRENT_LANGUAGE = APP_LANG_JAPANESE;
            break;
        case LANG_KOREAN:
            CURRENT_LANGUAGE = APP_LANG_KOREAN;
            break;
        default:
            CURRENT_LANGUAGE = APP_LANG_ENGLISH;  // 默认回退到英语
    }
}

/**
 * @brief 获取本地化字符串
 * @param chinese 简体中文版本的字符串
 * @param english 英语版本的字符串
 * @return const wchar_t* 当前语言对应的字符串指针
 * 
 * 根据当前语言设置返回对应语言的字符串。
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english) {
    // 首次调用时初始化翻译资源，但不自动检测系统语言
    static BOOL initialized = FALSE;
    if (!initialized) {
        // 不再调用DetectSystemLanguage()函数自动检测系统语言
        // 而是使用当前已设置的CURRENT_LANGUAGE值（可能来自配置文件）
        LoadLanguageResource(CURRENT_LANGUAGE);
        initialized = TRUE;
    }

    const wchar_t* translation = NULL;

    // 如果是简体中文并且提供了中文字符串，直接返回
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_SIMP && chinese) {
        return chinese;
    }

    // 查找翻译
    translation = FindTranslation(english);
    if (translation) {
        return translation;
    }

    // 繁体中文但未找到翻译时，返回简体中文作为备选
    if (CURRENT_LANGUAGE == APP_LANG_CHINESE_TRAD && chinese) {
        return chinese;
    }

    // 默认返回英文
    return english;
}

/**
 * @brief 设置应用程序语言
 * 
 * @param language 要设置的语言
 * @return BOOL 是否设置成功
 */
BOOL SetLanguage(AppLanguage language) {
    if (language < 0 || language >= APP_LANG_COUNT) {
        return FALSE;
    }
    
    CURRENT_LANGUAGE = language;
    g_translation_count = 0;  // 清空现有翻译
    return LoadLanguageResource(language);
}

/**
 * @brief 获取当前应用程序语言
 * 
 * @return AppLanguage 当前语言
 */
AppLanguage GetCurrentLanguage() {
    return CURRENT_LANGUAGE;
}

/**
 * @brief 获取当前语言的名称
 * @param buffer 用于存储语言名称的缓冲区
 * @param bufferSize 缓冲区大小（字符数）
 * @return 是否成功获取语言名称
 */
BOOL GetCurrentLanguageName(wchar_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) {
        return FALSE;
    }
    
    // 获取当前语言
    AppLanguage language = GetCurrentLanguage();
    
    // 根据语言枚举返回对应的名称
    switch (language) {
        case APP_LANG_CHINESE_SIMP:
            wcscpy_s(buffer, bufferSize, L"zh_CN");
            break;
        case APP_LANG_CHINESE_TRAD:
            wcscpy_s(buffer, bufferSize, L"zh-Hant");
            break;
        case APP_LANG_SPANISH:
            wcscpy_s(buffer, bufferSize, L"es");
            break;
        case APP_LANG_FRENCH:
            wcscpy_s(buffer, bufferSize, L"fr");
            break;
        case APP_LANG_GERMAN:
            wcscpy_s(buffer, bufferSize, L"de");
            break;
        case APP_LANG_RUSSIAN:
            wcscpy_s(buffer, bufferSize, L"ru");
            break;
        case APP_LANG_PORTUGUESE:
            wcscpy_s(buffer, bufferSize, L"pt");
            break;
        case APP_LANG_JAPANESE:
            wcscpy_s(buffer, bufferSize, L"ja");
            break;
        case APP_LANG_KOREAN:
            wcscpy_s(buffer, bufferSize, L"ko");
            break;
        case APP_LANG_ENGLISH:
        default:
            wcscpy_s(buffer, bufferSize, L"en");
            break;
    }
    
    return TRUE;
}
