/**
 * @file language.h
 * @brief 多语言支持模块头文件
 * 
 * 本文件定义了应用程序支持的语言枚举和本地化字符串获取接口。
 */

#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <wchar.h>

/**
 * @enum AppLanguage
 * @brief 应用程序支持的语言枚举
 * 
 * 定义应用程序支持的所有语言选项，用于国际化功能实现。
 */
typedef enum {
    APP_LANG_CHINESE_SIMP,   ///< 简体中文 (Simplified Chinese)
    APP_LANG_CHINESE_TRAD,   ///< 繁体中文 (Traditional Chinese)
    APP_LANG_ENGLISH,        ///< 英语 (English)
    APP_LANG_SPANISH,        ///< 西班牙语 (Spanish)
    APP_LANG_FRENCH,         ///< 法语 (French)
    APP_LANG_GERMAN,         ///< 德语 (German)
    APP_LANG_RUSSIAN,        ///< 俄语 (Russian)
    APP_LANG_PORTUGUESE,     ///< 葡萄牙语 (Portuguese)
    APP_LANG_JAPANESE,       ///< 日语 (Japanese)
    APP_LANG_KOREAN          ///< 韩语 (Korean)
} AppLanguage;

/// 当前应用程序使用的语言，默认根据系统语言自动检测
extern AppLanguage CURRENT_LANGUAGE;

/**
 * @brief 获取本地化字符串
 * @param chinese 简体中文版本的字符串
 * @param english 英语版本的字符串
 * @return 根据当前语言设置返回对应语言的字符串指针
 * 
 * 示例用法：
 * @code
 * const wchar_t* text = GetLocalizedString(L"你好", L"Hello");
 * @endcode
 */
const wchar_t* GetLocalizedString(const wchar_t* chinese, const wchar_t* english);

#endif /* LANGUAGE_H */
