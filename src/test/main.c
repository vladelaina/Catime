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
#include <sys/stat.h>

// 常量定义
int IMAGE_CAROUSEL_SCALE_FACTOR;        
char IMAGE_CAROUSEL_IMAGE_DIR[256];     
char previous_image_dir[256];            
int IMAGE_CAROUSEL_SWITCH_INTERVAL;      
int IMAGE_CAROUSEL_EDGE_SIZE;            
int IMAGE_CAROUSEL_MARGIN_LEFT;          
int IMAGE_CAROUSEL_MARGIN_TOP;           
int IMAGE_CAROUSEL_SHOW_TRAY_ICON;       
int IMAGE_CAROUSEL_DISPLAY_DURATION;     

// 新增全局变量
int IMAGE_CAROUSEL_SWITCH;                
int IMAGE_CAROUSEL_CONTROL_TIME;          
int IMAGE_CAROUSEL_POSITIONS[2];          
int IMAGE_CAROUSEL_DEBUG_MODE;           // 新增：调试模式开关

// 声明全局变量
int image_count;
char **image_files = NULL;
int imgWidth, imgHeight;
SDL_Window *window;
int previous_scale_factor;
int previous_display_duration;
Uint32 start_time;
HWND hwnd;

// 移动相关的全局变量
static Uint32 move_start_time = 0;
static int direction = 1;
static float current_progress = 0.0f;

// 函数声明
int get_png_files(const char *dir, char ***image_files);
// 获取当前的X坐标位置
int get_current_x_position(Uint32 current_time) {
    // 如果处于调试模式
    if (IMAGE_CAROUSEL_DEBUG_MODE) {
        // 在两个位置之间切换显示
        return (current_time / IMAGE_CAROUSEL_SWITCH_INTERVAL) % 2 == 0 ? 
               IMAGE_CAROUSEL_POSITIONS[0] : IMAGE_CAROUSEL_POSITIONS[1];
    }
    
    // 非调试模式下的正常逻辑
    if (IMAGE_CAROUSEL_SWITCH) {
        if (move_start_time == 0) {
            move_start_time = current_time;
        }

        float time_diff = (float)(current_time - move_start_time);
        current_progress = fmodf(time_diff / IMAGE_CAROUSEL_CONTROL_TIME, 2.0f);
        
        if (current_progress > 1.0f) {
            float reverse_progress = 2.0f - current_progress;
            return IMAGE_CAROUSEL_POSITIONS[1] - 
                   (IMAGE_CAROUSEL_POSITIONS[1] - IMAGE_CAROUSEL_POSITIONS[0]) * reverse_progress;
        } else {
            return IMAGE_CAROUSEL_POSITIONS[0] + 
                   (IMAGE_CAROUSEL_POSITIONS[1] - IMAGE_CAROUSEL_POSITIONS[0]) * current_progress;
        }
    }
    
    // 如果不是移动模式，返回固定位置
    return IMAGE_CAROUSEL_MARGIN_LEFT;
}

// 确保窗口在最顶层
void ensure_window_top_most() {
    int current_x;
    Uint32 current_time = SDL_GetTicks();
    
    if (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) {
        current_x = get_current_x_position(current_time);
    } else {
        current_x = IMAGE_CAROUSEL_MARGIN_LEFT;
    }

    SetWindowPos(hwnd, HWND_TOPMOST, 
        current_x,
        IMAGE_CAROUSEL_MARGIN_TOP, 
        imgWidth, 
        imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// 读取配置文件
void load_config(const char *filename) {
    const char *config_path = "./asset/config.txt";
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "无法打开配置文件: %s\n", config_path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%s", IMAGE_CAROUSEL_IMAGE_DIR) == 1) {
            if (strcmp(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0) {
                for (int i = 0; i < image_count; i++) {
                    free(image_files[i]);
                }
                free(image_files);
                image_files = NULL;
                image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files);

                if (image_count > 0) {
                    SDL_Surface *image = IMG_Load(image_files[0]);
                    if (image) {
                        imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                        SDL_FreeSurface(image);
                        SDL_SetWindowSize(window, imgWidth, imgHeight);
                        ensure_window_top_most();
                    }
                }
            }
            strcpy(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR);
            continue;
        }
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d", &IMAGE_CAROUSEL_SWITCH_INTERVAL) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &IMAGE_CAROUSEL_EDGE_SIZE) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &IMAGE_CAROUSEL_MARGIN_LEFT) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &IMAGE_CAROUSEL_MARGIN_TOP) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &IMAGE_CAROUSEL_SHOW_TRAY_ICON) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &IMAGE_CAROUSEL_DISPLAY_DURATION) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH=%d", &IMAGE_CAROUSEL_SWITCH) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_CONTROL_TIME=%d", &IMAGE_CAROUSEL_CONTROL_TIME) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_POSITIONS=%d,%d", &IMAGE_CAROUSEL_POSITIONS[0], &IMAGE_CAROUSEL_POSITIONS[1]) == 2) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DEBUG_MODE=%d", &IMAGE_CAROUSEL_DEBUG_MODE) == 1) continue;  // 新增：读取调试模式开关
    }

    fclose(file);
    // load_config函数的后半部分
    if (IMAGE_CAROUSEL_SCALE_FACTOR != previous_scale_factor) {
        previous_scale_factor = IMAGE_CAROUSEL_SCALE_FACTOR;
        if (image_count > 0) {
            SDL_Surface *image = IMG_Load(image_files[0]);
            if (image) {
                imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
                SDL_FreeSurface(image);
                SDL_SetWindowSize(window, imgWidth, imgHeight);
                ensure_window_top_most();
            }
        }
    }

    if (IMAGE_CAROUSEL_DISPLAY_DURATION != previous_display_duration) {
        previous_display_duration = IMAGE_CAROUSEL_DISPLAY_DURATION;
        start_time = SDL_GetTicks();
    }
}

// 获取文件的最后修改时间
time_t get_file_modification_time(const char *filename) {
    struct stat fileInfo;
    if (stat(filename, &fileInfo) == 0) {
        return fileInfo.st_mtime;
    }
    return -1;
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
    int numA = atoi(strrchr(fileA, '/') + 1);
    int numB = atoi(strrchr(fileB, '/') + 1);
    return numA - numB;
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
    // process_alpha函数的后半部分
    // 第二遍：处理边缘像素
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            int idx = y * surface->w + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(src[idx], surface->format, &r, &g, &b, &a);
            
            if (a > 0) {
                const int dx[] = {-IMAGE_CAROUSEL_EDGE_SIZE, 0, IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE, 0, IMAGE_CAROUSEL_EDGE_SIZE};
                const int dy[] = {-IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE, 0, 0, IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE};
                
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
    bmi.bmiHeader.biHeight = -surface->h;
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
      // process_and_display_image函数的后半部分
    if (scaled) {
        SDL_BlitScaled(image, NULL, scaled, NULL);
        SDL_FreeSurface(image);

        SDL_Surface *processed = process_alpha(scaled);
        SDL_FreeSurface(scaled);

        if (processed) {
            SDL_Surface *converted = SDL_ConvertSurfaceFormat(processed, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(processed);

            if (converted) {
                HBITMAP hBitmap = SDLSurfaceToWinBitmap(converted, hdcMemory);
                if (hBitmap) {
                    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);

                    BLENDFUNCTION blend = {0};
                    blend.BlendOp = AC_SRC_OVER;
                    blend.SourceConstantAlpha = 255;
                    blend.AlphaFormat = AC_SRC_ALPHA;

                    POINT ptSrc = {0, 0};
                    SIZE sizeWnd = {imgWidth, imgHeight};
                    
                    // 获取当前X坐标位置
                    int current_x = get_current_x_position(SDL_GetTicks());
                    
                    POINT ptDst = {current_x, IMAGE_CAROUSEL_MARGIN_TOP};

                    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, 
                                     hdcMemory, &ptSrc, 0, &blend, ULW_ALPHA);

                    SelectObject(hdcMemory, hOldBitmap);
                    DeleteObject(hBitmap);
                    
                    // 确保窗口在最顶层
                    ensure_window_top_most();
                }
                SDL_FreeSurface(converted);
            }
        }
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    load_config("config.txt");
    time_t last_mod_time = get_file_modification_time("./asset/config.txt");

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
    // WinMain函数的后半部分
    window = SDL_CreateWindow("SDL2 Image Display", 
        IMAGE_CAROUSEL_MARGIN_LEFT,
        IMAGE_CAROUSEL_MARGIN_TOP,
        imgWidth, 
        imgHeight, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);

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

    hwnd = wmInfo.info.win.window;
    
    // 设置窗口样式，确保始终在最顶层
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, 
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    
    // 初始设置窗口位置和层级
    SetWindowPos(hwnd, HWND_TOPMOST, 
        IMAGE_CAROUSEL_MARGIN_LEFT, 
        IMAGE_CAROUSEL_MARGIN_TOP, 
        imgWidth, 
        imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    HDC hdcScreen = GetDC(NULL);
    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 0;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);

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

    SDL_Event e;
    int quit = 0;
    int current_image_index = 0;
    Uint32 last_time = SDL_GetTicks();
    start_time = SDL_GetTicks();
    move_start_time = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        Uint32 current_time = SDL_GetTicks();

        // 检查配置文件修改
        time_t current_mod_time = get_file_modification_time("./asset/config.txt");
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time;
            load_config("config.txt");
        }

        // 图片切换逻辑
        if (current_time - last_time >= IMAGE_CAROUSEL_SWITCH_INTERVAL) {
            last_time = current_time;
            current_image_index = (current_image_index + 1) % image_count;
            process_and_display_image(image_files[current_image_index], 
                                   window, hdcScreen, hdcMemory, 
                                   imgWidth, imgHeight);
        }

        // 确保窗口始终在最顶层
        ensure_window_top_most();

        // 检查显示持续时间
        if ((current_time - start_time) / 1000 >= IMAGE_CAROUSEL_DISPLAY_DURATION) {
            quit = 1;
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
