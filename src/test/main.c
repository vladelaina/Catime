#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h> // 添加此行以包含 stat 函数和 struct stat

// 常量定义
int IMAGE_CAROUSEL_SCALE_FACTOR;        // 图片缩放比例
char IMAGE_CAROUSEL_IMAGE_DIR[256];     // 图片文件夹目录
char previous_image_dir[256];            // 添加一个变量来存储之前的图片目录
int IMAGE_CAROUSEL_SWITCH_INTERVAL;      // 切换到下一张图片的间隔时间（毫秒）
int IMAGE_CAROUSEL_EDGE_SIZE;            // 边缘处理的像素大小
int IMAGE_CAROUSEL_MARGIN_LEFT;          // 距离屏幕左边的距离（像素）
int IMAGE_CAROUSEL_MARGIN_TOP;           // 距离屏幕顶部的距离（像素）
int IMAGE_CAROUSEL_SHOW_TRAY_ICON;       // 控制是否显示托盘图标（0为不显示，1为显示）
int IMAGE_CAROUSEL_DISPLAY_DURATION;     // 显示持续时间（秒）

// 声明全局变量
int image_count; // 图片数量
char **image_files = NULL; // 图片文件列表
int imgWidth, imgHeight; // 图片宽度和高度
SDL_Window *window; // SDL 窗口

// 函数声明
int get_png_files(const char *dir, char ***image_files); // 前向声明

// 读取配置文件
void load_config(const char *filename) {
    const char *config_path = "./asset/config.txt"; // 新的配置文件路径
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "无法打开配置文件: %s\n", config_path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // 忽略空行和以 # 开头的行
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        // 读取配置项
        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%s", IMAGE_CAROUSEL_IMAGE_DIR) == 1) {
            // 检查路径是否发生变化
            if (strcmp(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0) {
                // 路径发生变化，释放之前的图片文件
                for (int i = 0; i < image_count; i++) {
                    free(image_files[i]);
                }
                free(image_files);
                image_files = NULL; // 清空之前的文件列表
                image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files); // 重新获取图片文件

                // 更新窗口大小
                if (image_count > 0) {
                    SDL_Surface *image = IMG_Load(image_files[0]);
                    if (image) {
                        imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        SDL_FreeSurface(image);
                        SDL_SetWindowSize(window, imgWidth, imgHeight); // 调整窗口大小
                    }
                }
            }
            strcpy(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR); // 更新之前的路径
            continue;
        }
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d", &IMAGE_CAROUSEL_SWITCH_INTERVAL) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &IMAGE_CAROUSEL_EDGE_SIZE) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &IMAGE_CAROUSEL_MARGIN_LEFT) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &IMAGE_CAROUSEL_MARGIN_TOP) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &IMAGE_CAROUSEL_SHOW_TRAY_ICON) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &IMAGE_CAROUSEL_DISPLAY_DURATION) == 1) continue;
    }

    fclose(file);
}

// 获取文件的最后修改时间
time_t get_file_modification_time(const char *filename) {
    struct stat fileInfo; // 使用标准的 struct stat
    if (stat(filename, &fileInfo) == 0) { // 使用 stat 函数
        return fileInfo.st_mtime; // 返回最后修改时间
    }
    return -1; // 返回-1表示获取失败
}

// 判断文件是否为 PNG 格式
int is_png(const char *filename) {
    const char *ext = strrchr(filename, '.');
    return ext != NULL && strcmp(ext, ".png") == 0;
}

// 比较函数，用于 qsort
int compare(const void *a, const void *b) {
    const char *fileA = *(const char **)a;
    const char *fileB = *(const char **)b;

    // 提取数字部分进行比较
    int numA = atoi(strrchr(fileA, '/') + 1); // 获取文件名中的数字
    int numB = atoi(strrchr(fileB, '/') + 1); // 获取文件名中的数字

    return numA - numB; // 返回比较结果
}

// 获取指定目录下的所有 PNG 文件
int get_png_files(const char *dir, char ***image_files) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "Failed to open directory: %s\n", dir);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (is_png(entry->d_name)) {
            (*image_files) = realloc(*image_files, sizeof(char*) * (count + 1));
            (*image_files)[count] = malloc(strlen(dir) + strlen(entry->d_name) + 2);
            sprintf((*image_files)[count], "%s/%s", dir, entry->d_name);
            count++;
        }
    }
    closedir(d);

    // 对文件名进行排序
    qsort(*image_files, count, sizeof(char*), compare);

    return count;
}

// 处理 alpha 通道和边缘
SDL_Surface* process_alpha(SDL_Surface* surface) {
    SDL_Surface* result = SDL_CreateRGBSurface(0, surface->w, surface->h, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    if (!result) return NULL;
    
    SDL_LockSurface(surface);
    SDL_LockSurface(result);
    
    Uint32* src = (Uint32*)surface->pixels;
    Uint32* dst = (Uint32*)result->pixels;
    
    // 第一遍：复制原始数据
    for (int i = 0; i < surface->w * surface->h; i++) {
        dst[i] = src[i];
    }
    
    // 第二遍：处理边缘像素
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            int idx = y * surface->w + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(src[idx], surface->format, &r, &g, &b, &a);
            
            if (a > 0) {
                const int dx[] = {-IMAGE_CAROUSEL_EDGE_SIZE , 0, IMAGE_CAROUSEL_EDGE_SIZE , -IMAGE_CAROUSEL_EDGE_SIZE , IMAGE_CAROUSEL_EDGE_SIZE , -IMAGE_CAROUSEL_EDGE_SIZE , 0, IMAGE_CAROUSEL_EDGE_SIZE };
                const int dy[] = {-IMAGE_CAROUSEL_EDGE_SIZE , -IMAGE_CAROUSEL_EDGE_SIZE , -IMAGE_CAROUSEL_EDGE_SIZE , 0, 0, IMAGE_CAROUSEL_EDGE_SIZE , IMAGE_CAROUSEL_EDGE_SIZE , IMAGE_CAROUSEL_EDGE_SIZE };
                
                for (int i = 0; i < 8; i++) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];
                    
                    if (nx >= 0 && nx < surface->w && ny >= 0 && ny < surface->h) {
                        Uint8 nr, ng, nb, na;
                        SDL_GetRGBA(src[ny * surface->w + nx], surface->format, &nr, &ng, &nb, &na);
                        
                        if (na == 0) {
                            dst[idx] = SDL_MapRGBA(result->format, 0, 0, 0, 0);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    SDL_UnlockSurface(result);
    SDL_UnlockSurface(surface);
    
    return result;
}

// 将SDL表面转换为Windows位图
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc) {
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = surface->w;
    bmi.bmiHeader.biHeight = -surface->h;  // 负值表示从上到下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap) return NULL;

    SDL_LockSurface(surface);
    Uint32* src = (Uint32*)surface->pixels;
    Uint32* dst = (Uint32*)bits;
    
    for (int i = 0; i < surface->w * surface->h; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(src[i], surface->format, &r, &g, &b, &a);
        
        if (a == 0) {
            dst[i] = 0;
        } else {
            dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    
    SDL_UnlockSurface(surface);
    return hBitmap;
}

// 处理和显示图像
void process_and_display_image(const char* image_path, SDL_Window* window, HDC hdcScreen, HDC hdcMemory, int imgWidth, int imgHeight) {
    SDL_Surface *image = IMG_Load(image_path);
    if (!image) return;

    SDL_Surface *scaled = SDL_CreateRGBSurface(0, imgWidth, imgHeight, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    if (scaled) {
        SDL_BlitScaled(image, NULL, scaled, NULL);
        SDL_FreeSurface(image); // 释放原始图像

        SDL_Surface *processed = process_alpha(scaled);
        SDL_FreeSurface(scaled); // 释放缩放后的图像

        if (processed) {
            SDL_Surface *converted = SDL_ConvertSurfaceFormat(processed, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(processed); // 释放处理后的图像

            if (converted) {
                HBITMAP hBitmap = SDLSurfaceToWinBitmap(converted, hdcMemory);
                if (hBitmap) {
                    SDL_SysWMinfo wmInfo;
                    SDL_VERSION(&wmInfo.version);
                    SDL_GetWindowWMInfo(window, &wmInfo);
                    HWND hwnd = wmInfo.info.win.window;

                    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);

                    BLENDFUNCTION blend = {0};
                    blend.BlendOp = AC_SRC_OVER;
                    blend.SourceConstantAlpha = 255;
                    blend.AlphaFormat = AC_SRC_ALPHA;

                    POINT ptSrc = {0, 0};
                    SIZE sizeWnd = {imgWidth, imgHeight};
                    POINT ptDst = {IMAGE_CAROUSEL_MARGIN_LEFT, IMAGE_CAROUSEL_MARGIN_TOP};

                    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, 
                                     hdcMemory, &ptSrc, 0, &blend, ULW_ALPHA);

                    SelectObject(hdcMemory, hOldBitmap);
                    DeleteObject(hBitmap);
                }
                SDL_FreeSurface(converted); // 释放转换后的图像
            }
        }
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 加载配置
    load_config("config.txt");

    // 获取配置文件的最后修改时间
    time_t last_mod_time = get_file_modification_time("./asset/config.txt"); // 更新路径

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG == 0) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files);
    if (image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", IMAGE_CAROUSEL_IMAGE_DIR);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface *image = IMG_Load(image_files[0]);
    if (!image) {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
    imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
    SDL_FreeSurface(image);

    window = SDL_CreateWindow("SDL2 Image Display", 
        IMAGE_CAROUSEL_MARGIN_LEFT,
        IMAGE_CAROUSEL_MARGIN_TOP,
        imgWidth, 
        imgHeight, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo) != 1) {
        fprintf(stderr, "SDL_GetWindowWMInfo failed\n");
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    HWND hwnd = wmInfo.info.win.window;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, 
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);

    HDC hdcScreen = GetDC(NULL);
    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 0;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
    
    SetWindowPos(hwnd, HWND_TOPMOST, 
        IMAGE_CAROUSEL_MARGIN_LEFT, 
        IMAGE_CAROUSEL_MARGIN_TOP, 
        imgWidth, 
        imgHeight, 
        SWP_NOACTIVATE);

    HDC hdcMemory = CreateCompatibleDC(hdcScreen);

    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    NOTIFYICONDATA nid;
    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP;
    nid.hIcon = (HICON)LoadImage(NULL, "icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    wchar_t szTip[128];
    wcsncpy(szTip, L"My Tray Icon", sizeof(szTip) / sizeof(wchar_t));
    wcscpy((wchar_t*)nid.szTip, szTip);
    Shell_NotifyIcon(NIM_ADD, &nid);
    #endif

    process_and_display_image(image_files[0], window, hdcScreen, hdcMemory, imgWidth, imgHeight);

    SDL_Event e;
    int quit = 0; // 退出标志
    int current_image_index = 0; // 当前图片索引
    Uint32 last_time = SDL_GetTicks();
    Uint32 start_time = SDL_GetTicks(); // 记录开始时间

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        // 检查配置文件是否被修改
        time_t current_mod_time = get_file_modification_time("./asset/config.txt"); // 更新路径
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time; // 更新最后修改时间
            load_config("config.txt"); // 重新加载配置

            // 检查路径是否发生变化
            if (strcmp(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0) {
                // 路径发生变化，释放之前的图片文件
                for (int i = 0; i < image_count; i++) {
                    free(image_files[i]);
                }
                free(image_files);
                image_files = NULL; // 清空之前的文件列表
                image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files); // 重新获取图片文件

                // 更新窗口大小
                if (image_count > 0) {
                    SDL_Surface *image = IMG_Load(image_files[0]);
                    if (image) {
                        imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        SDL_FreeSurface(image);
                        SDL_SetWindowSize(window, imgWidth, imgHeight); // 调整窗口大小
                    }
                }
                strcpy(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR); // 更新之前的路径
            }
        }

        Uint32 current_time = SDL_GetTicks();

        // 使用 IMAGE_CAROUSEL_SWITCH_INTERVAL 来控制切换速度
        if (current_time - last_time >= IMAGE_CAROUSEL_SWITCH_INTERVAL) {
            last_time = current_time;
            current_image_index = (current_image_index + 1) % image_count;
            process_and_display_image(image_files[current_image_index], 
                                   window, hdcScreen, hdcMemory, 
                                   imgWidth, imgHeight);
        }

        // 确保窗口始终在最上层
        SetWindowPos(hwnd, HWND_TOPMOST, 
            IMAGE_CAROUSEL_MARGIN_LEFT, 
            IMAGE_CAROUSEL_MARGIN_TOP, 
            imgWidth, 
            imgHeight, 
            SWP_NOACTIVATE);

        // 检查是否超过显示持续时间
        if ((current_time - start_time) / 1000 >= IMAGE_CAROUSEL_DISPLAY_DURATION) {
            quit = 1; // 超过时间后退出
        }

        SDL_Delay(1);
    }

    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    Shell_NotifyIcon(NIM_DELETE, &nid);
    #endif
    
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    for (int i = 0; i < image_count; i++) {
        free(image_files[i]);
    }
    free(image_files);

    return 0;
}
