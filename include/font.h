/**
 * @file font.h
 * @brief 字体管理模块头文件
 * 
 * 本文件定义了应用程序的字体管理相关接口，包括字体加载、设置和管理功能。
 */

#ifndef FONT_H
#define FONT_H

#include <windows.h>
#include <stdbool.h>

/// 字体资源结构体，用于管理字体资源
typedef struct {
    int menuId;
    int resourceId;
    const char* fontName;
} FontResource;

/// 字体资源数组，存储所有可用的字体资源
extern FontResource fontResources[];

/// 字体资源数组的大小
extern const int FONT_RESOURCES_COUNT;

/// 字体文件名
extern char FONT_FILE_NAME[100];

/// 字体内部名称
extern char FONT_INTERNAL_NAME[100];

/// 预览字体名称
extern char PREVIEW_FONT_NAME[100];

/// 预览字体内部名称
extern char PREVIEW_INTERNAL_NAME[100];

/// 是否正在预览字体
extern BOOL IS_PREVIEWING;

/**
 * @brief 从资源加载字体
 * @param hInstance 应用程序实例句柄
 * @param resourceId 字体资源ID
 * @return 加载是否成功
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId);

/**
 * @brief 根据字体名称加载字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 字体名称
 * @return 加载是否成功
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName);

/**
 * @brief 写入字体配置到配置文件
 * @param font_file_name 字体文件名
 */
void WriteConfigFont(const char* font_file_name);

/**
 * @brief 列出系统中可用的字体
 */
void ListAvailableFonts(void);

/**
 * @brief 预览字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 要预览的字体名称
 * @return 预览是否成功
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName);

/**
 * @brief 取消字体预览
 */
void CancelFontPreview(void);

/**
 * @brief 应用字体预览
 */
void ApplyFontPreview(void);

/**
 * @brief 切换字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 要切换的字体名称
 * @return 切换是否成功
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName);

#endif /* FONT_H */