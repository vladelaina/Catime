/**
 * @file font.c
 * @brief 字体管理模块实现文件
 * 
 * 本文件实现了应用程序的字体管理相关功能，包括字体加载、设置和管理。
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/font.h"
#include "../resource/resource.h"

char FONT_FILE_NAME[100] = "Hack Nerd Font.ttf";
char FONT_INTERNAL_NAME[100];
char PREVIEW_FONT_NAME[100] = "";
char PREVIEW_INTERNAL_NAME[100] = "";
BOOL IS_PREVIEWING = FALSE;

FontResource fontResources[] = {
    {CLOCK_IDC_FONT_RECMONO, IDR_FONT_RECMONO, "RecMonoCasual Nerd Font Mono Essence.ttf"},
    {CLOCK_IDC_FONT_DEPARTURE, IDR_FONT_DEPARTURE, "DepartureMono Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_TERMINESS, IDR_FONT_TERMINESS, "Terminess Nerd Font Propo Essence.ttf"},
    {CLOCK_IDC_FONT_GOHUFONT, IDR_FONT_GOHUFONT, "GohuFont uni11 Nerd Font Mono.ttf"},
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
    {CLOCK_IDC_FONT_PIXELIFY, IDR_FONT_PIXELIFY, "Pixelify Sans Medium Essence.ttf"},
    {CLOCK_IDC_FONT_PRESS_START, IDR_FONT_PRESS_START, "Press Start 2P Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BUBBLES, IDR_FONT_RUBIK_BUBBLES, "Rubik Bubbles Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_BURNED, IDR_FONT_RUBIK_BURNED, "Rubik Burned Essence.ttf"},
    {CLOCK_IDC_FONT_RUBIK_GLITCH_POP, IDR_FONT_RUBIK_GLITCH_POP, "Rubik Glitch Pop Essence.ttf"},
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
    {CLOCK_IDC_FONT_PROFONT, IDR_FONT_PROFONT, "ProFont IIx Nerd Font.ttf"},
    {CLOCK_IDC_FONT_DADDYTIME, IDR_FONT_DADDYTIME, "DaddyTimeMono Nerd Font Propo Essence.ttf"},
};

const int FONT_RESOURCES_COUNT = sizeof(fontResources) / sizeof(FontResource);


/// 当前时钟文本颜色
extern char CLOCK_TEXT_COLOR[];

/// 获取配置文件路径
extern void GetConfigPath(char* path, int maxLen);

/// 读取配置文件
extern void ReadConfig(void);

/// 字体枚举回调函数
extern int CALLBACK EnumFontFamExProc(ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, DWORD FontType, LPARAM lParam);

BOOL LoadFontFromResource(HINSTANCE hInstance, int resourceId) {
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), RT_FONT);
    if (hResource == NULL) {
        return FALSE;
    }

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (hMemory == NULL) {
        return FALSE;
    }

    void* fontData = LockResource(hMemory);
    if (fontData == NULL) {
        return FALSE;
    }

    DWORD fontLength = SizeofResource(hInstance, hResource);
    DWORD nFonts = 0;
    HANDLE handle = AddFontMemResourceEx(fontData, fontLength, NULL, &nFonts);
    
    if (handle == NULL) {
        return FALSE;
    }
    
    return TRUE;
}

BOOL LoadFontByName(HINSTANCE hInstance, const char* fontName) {
    for (int i = 0; i < sizeof(fontResources) / sizeof(FontResource); i++) {
        if (strcmp(fontResources[i].fontName, fontName) == 0) {
            return LoadFontFromResource(hInstance, fontResources[i].resourceId);
        }
    }
    return FALSE;
}

void WriteConfigFont(const char* font_file_name) {
    char config_path[MAX_PATH];
    GetConfigPath(config_path, MAX_PATH);
    
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file for reading: %s\n", config_path);
        return;
    }

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

    char *new_config = (char *)malloc(file_size + 100);
    if (!new_config) {
        fprintf(stderr, "Memory allocation failed!\n");
        free(config_content);
        return;
    }
    new_config[0] = '\0';

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

    strcat(new_config, "CLOCK_TEXT_COLOR=");
    strcat(new_config, CLOCK_TEXT_COLOR);
    strcat(new_config, "\n");

    free(config_content);

    file = fopen(config_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open config file for writing: %s\n", config_path);
        free(new_config);
        return;
    }
    fwrite(new_config, sizeof(char), strlen(new_config), file);
    fclose(file);

    free(new_config);

    ReadConfig();
}

void ListAvailableFonts(void) {
    HDC hdc = GetDC(NULL);
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfCharSet = DEFAULT_CHARSET;

    HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             lf.lfCharSet, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, NULL);
    SelectObject(hdc, hFont);

    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, 0, 0);

    DeleteObject(hFont);
    ReleaseDC(NULL, hdc);
}

int CALLBACK EnumFontFamExProc(
    ENUMLOGFONTEX *lpelfe,
    NEWTEXTMETRICEX *lpntme,
    DWORD FontType,
    LPARAM lParam
) {
    return 1;
}