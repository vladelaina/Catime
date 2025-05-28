/**
 * @file font.c
 * @brief 字体管理模块实现文件
 * 
 * 本文件实现了应用程序的字体管理相关功能，包括字体加载、预览、
 * 应用和配置文件管理。支持从资源中加载多种预定义字体。
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/font.h"
#include "../resource/resource.h"

/// @name 全局字体变量
/// @{
char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";  ///< 当前使用的字体文件名
char FONT_INTERNAL_NAME[100];                     ///< 字体内部名称（无扩展名）
char PREVIEW_FONT_NAME[100] = "";                 ///< 预览字体的文件名
char PREVIEW_INTERNAL_NAME[100] = "";             ///< 预览字体的内部名称
BOOL IS_PREVIEWING = FALSE;                       ///< 是否正在预览字体
/// @}

/**
 * @brief 字体资源数组
 * 
 * 存储应用程序内置的所有字体资源信息
 */
FontResource fontResources[] = {
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_ARBUTUS, IDR_FONT_ARBUTUS, "Arbutus Essence.ttf"},
    {CLOCK_IDC_FONT_BERKSHIRE, IDR_FONT_BERKSHIRE, "Berkshire Swash Essence.ttf"},
    {CLOCK_IDC_FONT_CAVEAT, IDR_FONT_CAVEAT, "Caveat Brush Essence.ttf"},
    {CLOCK_IDC_FONT_CREEPSTER, IDR_FONT_CREEPSTER, "Creepster Essence.ttf"},
    {CLOCK_IDC_FONT_DOTGOTHIC, IDR_FONT_DOTGOTHIC, "DotGothic16 Essence.ttf"},
    {CLOCK_IDC_FONT_DOTO, IDR_FONT_DOTO, "Doto ExtraBold Essence.ttf"},
    {CLOCK_IDC_FONT_FOLDIT, IDR_FONT_FOLDIT, "Foldit SemiBold Essence.ttf"},
    {CLOCK_IDC_FONT_FREDERICKA, IDR_FONT_FREDERICKA, "Fredericka the Great Essence.ttf"},
    {CLOCK_IDC_FONT_FRIJOLE, IDR_FONT_FRIJOLE, "Frijole Essence.ttf"},
    {CLOCK_IDC_FONT_GWENDOLYN, IDR_FONT_GWENDOLYN, "Gwendolyn Essence.ttf"},
    {CLOCK_IDC_FONT_HANDJET, IDR_FONT_HANDJET, "Handjet Essence.ttf"},
    {CLOCK_IDC_FONT_INKNUT, IDR_FONT_INKNUT, "Inknut Antiqua Medium Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARD, IDR_FONT_JACQUARD, "Jacquard 12 Essence.ttf"},
    {CLOCK_IDC_FONT_JACQUARDA, IDR_FONT_JACQUARDA, "Jacquarda Bastarda 9 Essence.ttf"},
    {CLOCK_IDC_FONT_KAVOON, IDR_FONT_KAVOON, "Kavoon Essence.ttf"},
    {CLOCK_IDC_FONT_KUMAR_ONE_OUTLINE, IDR_FONT_KUMAR_ONE_OUTLINE, "Kumar One Outline Essence.ttf"},
    {CLOCK_IDC_FONT_KUMAR_ONE, IDR_FONT_KUMAR_ONE, "Kumar One Essence.ttf"},
    {CLOCK_IDC_FONT_LAKKI_REDDY, IDR_FONT_LAKKI_REDDY, "Lakki Reddy Essence.ttf"},
    {CLOCK_IDC_FONT_LICORICE, IDR_FONT_LICORICE, "Licorice Essence.ttf"},
    {CLOCK_IDC_FONT_MA_SHAN_ZHENG, IDR_FONT_MA_SHAN_ZHENG, "Ma Shan Zheng Essence.ttf"},
    {CLOCK_IDC_FONT_MOIRAI_ONE, IDR_FONT_MOIRAI_ONE, "Moirai One Essence.ttf"},
    {CLOCK_IDC_FONT_MYSTERY_QUEST, IDR_FONT_MYSTERY_QUEST, "Mystery Quest Essence.ttf"},
    {CLOCK_IDC_FONT_NOTO_NASTALIQ, IDR_FONT_NOTO_NASTALIQ, "Noto Nastaliq Urdu Medium Essence.ttf"},
    {CLOCK_IDC_FONT_PIEDRA, IDR_FONT_PIEDRA, "Piedra Essence.ttf"},
    {CLOCK_IDC_FONT_PINYON_SCRIPT, IDR_FONT_PINYON_SCRIPT, "Pinyon Script Essence.ttf"},
    {CLOCK_IDC_FONT_PIXELIFY, IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {CLOCK_IDC_FONT_PRESS_START, IDR_FONT_PRESS_START, "Press Start 2P Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BUBBLES, IDR_FONT_RUBIK_BUBBLES, "Rubik Bubbles Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BURNED, IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_GLITCH, IDR_FONT_RUBIK_GLITCH, "Rubik Glitch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_MARKER_HATCH, IDR_FONT_RUBIK_MARKER_HATCH, "Rubik Marker Hatch Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_PUDDLES, IDR_FONT_RUBIK_PUDDLES, "Rubik Puddles Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_VINYL, IDR_FONT_RUBIK_VINYL, "Rubik Vinyl Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_WET_PAINT, IDR_FONT_RUBIK_WET_PAINT, "Rubik Wet Paint Essence.ttf"},
    {CLOCK_IDC_FONT_RUGE_BOOGIE, IDR_FONT_RUGE_BOOGIE, "Ruge Boogie Essence.ttf"},
    {CLOCK_IDC_FONT_SEVILLANA, IDR_FONT_SEVILLANA, "Sevillana Essence.ttf"},
    {CLOCK_IDC_FONT_SILKSCREEN, IDR_FONT_SILKSCREEN, "Silkscreen Essence.ttf"},
    {CLOCK_IDC_FONT_STICK, IDR_FONT_STICK, "Stick Essence.ttf"},
    {CLOCK_IDC_FONT_UNDERDOG, IDR_FONT_UNDERDOG, "Underdog Essence.ttf"},
    {CLOCK_IDC_FONT_WALLPOET, IDR_FONT_WALLPOET, "Wallpoet Essence.ttf"},
    {CLOCK_IDC_FONT_YESTERYEAR, IDR_FONT_YESTERYEAR, "Yesteryear Essence.ttf"},
    {CLOCK_IDC_FONT_ZCOOL_KUAILE, IDR_FONT_ZCOOL_KUAILE, "ZCOOL KuaiLe Essence.ttf"},
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font Essence.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

/// 字体资源数量
const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);

/// @name 外部变量声明
/// @{
extern char CLOCK_TEXT_COLOR[];  ///< 当前时钟文本颜色
/// @}

/// @name 外部函数声明
/// @{
extern void GetConfigPath(char* path, size_t maxLen);             ///< 获取配置文件路径
extern void ReadConfig(void);                                  ///< 读取配置文件
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam); ///< 字体枚举回调函数
/// @}

/**
 * @brief 从资源加载字体
 * @param hInstance 应用程序实例句柄
 * @param resourceId 字体资源ID
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 从应用程序资源中加载字体并添加到系统字体集。
 */
BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    // 查找字体资源
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    // 加载资源到内存
    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    // 锁定资源
    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    // 获取资源大小并添加字体
    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    
    if (handle == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

/**
 * @brief 根据字体名称加载字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 字体文件名
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 在预定义的字体资源列表中查找指定名称的字体并加载。
 */
BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    // 遍历字体资源数组查找匹配的字体
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    return FALSE;
}

/**
 * @brief 将字体名称写入配置文件
 * @param font_file_name 要写入的字体文件名
 * 
 * 更新配置文件中的字体设置，保留其他配置项。
 */
void WriteConfigFont(const char* font_file_name) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    // 打开配置文件进行读取
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

    // 读取整个配置文件内容
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *config_content = (char *)malloc(file_size + 1);
    if (!config_content) {
        fprintf(stderr, "Memory allocation failed!\n");
        fclose(file);
        return;
    }
    fread(config_content, sizeof(char), file_size, file);
    config_content[file_size] = '\0';
    fclose(file);

    // 创建新的配置文件内容
    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

    // 按行处理并替换字体设置
    char *line = strtok(config_content, "\n");
    while (line) {
        if (strncmp(line, "FONT_FILE_NAME=", 15) == 0) {
            strcat(new_config, "FONT_FILE_NAME=");
            strcat(new_config, font_file_name);
            strcat(new_config, "\n");
        } else {
            strcat(new_config, line);
            strcat(new_config, "\n");
        }
        line = strtok(NULL, "\n");
    }

    free(config_content);

    // 写入新的配置内容
    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    // 重新读取配置
    ReadConfig();
}

/**
 * @brief 列出系统中可用的字体
 * 
 * 枚举系统中所有可用的字体，通过回调函数处理字体信息。
 */
void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    // 创建临时字体并枚举字体
    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             lf.lfCharSet, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, 0, 0);

    // 清理资源
    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

/**
 * @brief 字体枚举回调函数
 * @param lpelfe 字体枚举信息
 * @param lpntme 字体度量信息
 * @param FontType 字体类型
 * @param lParam 回调参数
 * @return int 继续枚举返回1
 * 
 * 被EnumFontFamiliesEx调用，处理枚举到的每个字体信息。
 */
int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEX *lpelfe,
    NEWTEXTMETRICEX *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    return 1;
}

/**
 * @brief 预览字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 要预览的字体名称
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 加载并设置预览字体，但不应用到配置文件。
 */
BOOL PreviewFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    // 保存当前字体名称
    strncpy(PREVIEW_FONT_NAME, fontName, sizeof(PREVIEW_FONT_NAME) - 1);
    PREVIEW_FONT_NAME[sizeof(PREVIEW_FONT_NAME) - 1] = '\0';
    
    // 获取内部字体名称（去除.ttf扩展名）
    size_t name_len = strlen(PREVIEW_FONT_NAME);
    if (name_len > 4 && strcmp(PREVIEW_FONT_NAME + name_len - 4, ".ttf") == 0) {
        // 确保目标大小足够，避免依赖源字符串长度
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(PREVIEW_INTERNAL_NAME))
            copy_len = sizeof(PREVIEW_INTERNAL_NAME) - 1;
        
        memcpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, copy_len);
        PREVIEW_INTERNAL_NAME[copy_len] = '\0';
    } else {
        strncpy(PREVIEW_INTERNAL_NAME, PREVIEW_FONT_NAME, sizeof(PREVIEW_INTERNAL_NAME) - 1);
        PREVIEW_INTERNAL_NAME[sizeof(PREVIEW_INTERNAL_NAME) - 1] = '\0';
    }
    
    // 加载预览字体
    if (!LoadFontByName(hInstance, PREVIEW_FONT_NAME)) {
        return FALSE;
    }
    
    IS_PREVIEWING = TRUE;
    return TRUE;
}

/**
 * @brief 取消字体预览
 * 
 * 清除预览状态并恢复到当前设置的字体。
 */
void CancelFontPreview(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

/**
 * @brief 应用字体预览
 * 
 * 将当前预览的字体设置为实际使用的字体，并写入配置文件。
 */
void ApplyFontPreview(void) {
    // 检查是否有有效的预览字体
    if (!IS_PREVIEWING || strlen(PREVIEW_FONT_NAME) == 0) return;
    
    // 更新当前字体
    strncpy(FONT_FILE_NAME, PREVIEW_FONT_NAME, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    strncpy(FONT_INTERNAL_NAME, PREVIEW_INTERNAL_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
    FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    
    // 保存到配置文件并取消预览状态
    WriteConfigFont(FONT_FILE_NAME);
    CancelFontPreview();
}

/**
 * @brief 切换字体
 * @param hInstance 应用程序实例句柄
 * @param fontName 要切换的字体名称
 * @return BOOL 成功返回TRUE，失败返回FALSE
 * 
 * 直接切换到指定字体，不经过预览过程。
 */
BOOL SwitchFont(HINSTANCE hInstance, const char* fontName) {
    if (!fontName) return FALSE;
    
    // 加载新字体
    if (!LoadFontByName(hInstance, fontName)) {
        return FALSE;
    }
    
    // 更新字体名称
    strncpy(FONT_FILE_NAME, fontName, sizeof(FONT_FILE_NAME) - 1);
    FONT_FILE_NAME[sizeof(FONT_FILE_NAME) - 1] = '\0';
    
    // 更新内部字体名称（去除.ttf扩展名）
    size_t name_len = strlen(FONT_FILE_NAME);
    if (name_len > 4 && strcmp(FONT_FILE_NAME + name_len - 4, ".ttf") == 0) {
        // 确保目标大小足够，避免依赖源字符串长度
        size_t copy_len = name_len - 4;
        if (copy_len >= sizeof(FONT_INTERNAL_NAME))
            copy_len = sizeof(FONT_INTERNAL_NAME) - 1;
            
        memcpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, copy_len);
        FONT_INTERNAL_NAME[copy_len] = '\0';
    } else {
        strncpy(FONT_INTERNAL_NAME, FONT_FILE_NAME, sizeof(FONT_INTERNAL_NAME) - 1);
        FONT_INTERNAL_NAME[sizeof(FONT_INTERNAL_NAME) - 1] = '\0';
    }
    
    // 写入配置文件
    WriteConfigFont(FONT_FILE_NAME);
    return TRUE;
}