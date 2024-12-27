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

// 基础显示配置
int IMAGE_CAROUSEL_SCALE_FACTOR;        
char IMAGE_CAROUSEL_IMAGE_DIR[256];     
char previous_image_dir[256];            
int IMAGE_CAROUSEL_SWITCH_INTERVAL;      
int IMAGE_CAROUSEL_EDGE_SIZE;            
int IMAGE_CAROUSEL_MARGIN_LEFT;          
int IMAGE_CAROUSEL_MARGIN_TOP;           
int IMAGE_CAROUSEL_SHOW_TRAY_ICON;       
int IMAGE_CAROUSEL_DISPLAY_DURATION;     
int IMAGE_CAROUSEL_SWITCH;                
int IMAGE_CAROUSEL_CONTROL_TIME;          
int IMAGE_CAROUSEL_POSITIONS[2];          
int IMAGE_CAROUSEL_DEBUG_MODE;           

// 移动显示配置
int MOVING_SCALE_FACTOR;               // 移动时的图片缩放比例
char MOVING_IMAGE_DIR[256];            // 移动时的图片文件夹目录
int MOVING_SWITCH_INTERVAL;            // 移动时的图片切换间隔
char moving_previous_image_dir[256];   // 用于检测移动图片目录是否改变

// 静态显示相关变量
int image_count;
char **image_files = NULL;
int imgWidth, imgHeight;

// 移动显示相关变量
int moving_image_count = 0;
char **moving_image_files = NULL;
int moving_imgWidth, moving_imgHeight;

// 窗口相关
SDL_Window *window = NULL;
SDL_Window *second_window = NULL;
HWND hwnd;
HWND second_hwnd;

// 其他全局变量
int previous_scale_factor;
int previous_display_duration;
Uint32 start_time;

// 移动相关的全局变量
static Uint32 move_start_time = 0;
static int direction = 1;
static float current_progress = 0.0f;

// 函数声明
int get_png_files(const char *dir, char ***image_files);
void process_and_display_image(const char* image_path, SDL_Window* window, HDC hdcScreen, HDC hdcMemory, int imgWidth, int imgHeight, HWND windowHwnd);
// 获取当前的X坐标位置
int get_current_x_position(Uint32 current_time) {
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
    
    return IMAGE_CAROUSEL_MARGIN_LEFT;
}

// 确保窗口在最顶层
void ensure_window_top_most(HWND windowHwnd, int x_pos) {
    SetWindowPos(windowHwnd, HWND_TOPMOST, 
        x_pos,
        IMAGE_CAROUSEL_MARGIN_TOP, 
        imgWidth, 
        imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// 创建窗口
SDL_Window* create_window(const char* title, int x_pos, int width, int height, HWND* out_hwnd) {
    SDL_Window* win = SDL_CreateWindow(title, 
        x_pos,
        IMAGE_CAROUSEL_MARGIN_TOP,
        width, 
        height, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);

    if (win) {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(win, &wmInfo) == 1) {
            *out_hwnd = wmInfo.info.win.window;
            SetWindowLongPtr(*out_hwnd, GWL_EXSTYLE, 
                WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
        }
    }
    return win;
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

        // 读取基础配置
        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%s", IMAGE_CAROUSEL_IMAGE_DIR) == 1) {
            if (strcmp(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0) {
                for (int i = 0; i < image_count; i++) {
                    free(image_files[i]);
                }
                free(image_files);
                image_files = NULL;
                image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files);
            }
            strcpy(previous_image_dir, IMAGE_CAROUSEL_IMAGE_DIR);
            continue;
        }

        // 读取移动显示相关的配置
        if (sscanf(line, "MOVING_SCALE_FACTOR=%d", &MOVING_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "MOVING_IMAGE_DIR=%s", MOVING_IMAGE_DIR) == 1) {
            if (strcmp(moving_previous_image_dir, MOVING_IMAGE_DIR) != 0) {
                if (moving_image_files) {
                    for (int i = 0; i < moving_image_count; i++) {
                        free(moving_image_files[i]);
                    }
                    free(moving_image_files);
                    moving_image_files = NULL;
                }
                moving_image_count = get_png_files(MOVING_IMAGE_DIR, &moving_image_files);
            }
            strcpy(moving_previous_image_dir, MOVING_IMAGE_DIR);
            continue;
        }
        if (sscanf(line, "MOVING_SWITCH_INTERVAL=%d", &MOVING_SWITCH_INTERVAL) == 1) continue;

        // 读取其他配置
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d", &IMAGE_CAROUSEL_SWITCH_INTERVAL) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &IMAGE_CAROUSEL_EDGE_SIZE) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &IMAGE_CAROUSEL_MARGIN_LEFT) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &IMAGE_CAROUSEL_MARGIN_TOP) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &IMAGE_CAROUSEL_SHOW_TRAY_ICON) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &IMAGE_CAROUSEL_DISPLAY_DURATION) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH=%d", &IMAGE_CAROUSEL_SWITCH) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_CONTROL_TIME=%d", &IMAGE_CAROUSEL_CONTROL_TIME) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_POSITIONS=%d,%d", &IMAGE_CAROUSEL_POSITIONS[0], &IMAGE_CAROUSEL_POSITIONS[1]) == 2) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DEBUG_MODE=%d", &IMAGE_CAROUSEL_DEBUG_MODE) == 1) continue;
    }

    fclose(file);
    // load_config函数的后半部分
    // 更新窗口大小
    if (image_count > 0) {
        SDL_Surface *image = IMG_Load(image_files[0]);
        if (image) {
            imgWidth = (image->w * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
            imgHeight = (image->h * IMAGE_CAROUSEL_SCALE_FACTOR) / 100;
            SDL_FreeSurface(image);
            if (window) {
                SDL_SetWindowSize(window, imgWidth, imgHeight);
            }
        }
    }

    if (moving_image_count > 0) {
        SDL_Surface *image = IMG_Load(moving_image_files[0]);
        if (image) {
            moving_imgWidth = (image->w * MOVING_SCALE_FACTOR) / 100;
            moving_imgHeight = (image->h * MOVING_SCALE_FACTOR) / 100;
            SDL_FreeSurface(image);
            if (second_window) {
                SDL_SetWindowSize(second_window, moving_imgWidth, moving_imgHeight);
            }
        }
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
    // process_alpha函数的后半部分
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
void process_and_display_image(const char* image_path, SDL_Window* window, HDC hdcScreen, HDC hdcMemory, int imgWidth, int imgHeight, HWND windowHwnd) {
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
                    
                    // 获取窗口当前位置
                    RECT rect;
                    GetWindowRect(windowHwnd, &rect);
                    POINT ptDst = {rect.left, rect.top};

                    UpdateLayeredWindow(windowHwnd, hdcScreen, &ptDst, &sizeWnd, 
                                     hdcMemory, &ptSrc, 0, &blend, ULW_ALPHA);

                    SelectObject(hdcMemory, hOldBitmap);
                    DeleteObject(hBitmap);
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

    // 初始化主窗口的图片
    image_count = get_png_files(IMAGE_CAROUSEL_IMAGE_DIR, &image_files);
    if (image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", IMAGE_CAROUSEL_IMAGE_DIR);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 初始化移动显示的图片
    moving_image_count = get_png_files(MOVING_IMAGE_DIR, &moving_image_files);
    if (moving_image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", MOVING_IMAGE_DIR);
    }

    // 创建主窗口
    window = create_window("Main Window", IMAGE_CAROUSEL_MARGIN_LEFT, imgWidth, imgHeight, &hwnd);
    if (!window) {
        fprintf(stderr, "Failed to create main window\n");
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 如果是调试模式，创建第二个窗口
    if (IMAGE_CAROUSEL_DEBUG_MODE) {
        second_window = create_window("Second Window", IMAGE_CAROUSEL_POSITIONS[1], 
                                   moving_imgWidth, moving_imgHeight, &second_hwnd);
    }
    // WinMain函数的后半部分
    HDC hdcScreen = GetDC(NULL);
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
    int moving_image_index = 0;
    Uint32 last_time = SDL_GetTicks();
    Uint32 last_moving_time = SDL_GetTicks();
    start_time = SDL_GetTicks();

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

        if (IMAGE_CAROUSEL_DEBUG_MODE) {
            // 调试模式：在两个位置显示不同的图片
            // 更新第一个窗口的图片
            if (current_time - last_time >= IMAGE_CAROUSEL_SWITCH_INTERVAL) {
                last_time = current_time;
                current_image_index = (current_image_index + 1) % image_count;
                process_and_display_image(image_files[current_image_index], 
                                       window, hdcScreen, hdcMemory, 
                                       imgWidth, imgHeight, hwnd);
            }

            // 更新第二个窗口的图片
            if (current_time - last_moving_time >= MOVING_SWITCH_INTERVAL && moving_image_count > 0) {
                last_moving_time = current_time;
                moving_image_index = (moving_image_index + 1) % moving_image_count;
                process_and_display_image(moving_image_files[moving_image_index], 
                                       second_window, hdcScreen, hdcMemory, 
                                       moving_imgWidth, moving_imgHeight, second_hwnd);
            }

            // 确保两个窗口都在最顶层
            ensure_window_top_most(hwnd, IMAGE_CAROUSEL_POSITIONS[0]);
            ensure_window_top_most(second_hwnd, IMAGE_CAROUSEL_POSITIONS[1]);
        } else {
            // 正常模式：根据配置决定是否移动
            if (current_time - last_time >= IMAGE_CAROUSEL_SWITCH_INTERVAL) {
                last_time = current_time;
                current_image_index = (current_image_index + 1) % image_count;
                process_and_display_image(image_files[current_image_index], 
                                       window, hdcScreen, hdcMemory, 
                                       imgWidth, imgHeight, hwnd);
            }

            // 确保窗口在最顶层并更新位置
            ensure_window_top_most(hwnd, 
                IMAGE_CAROUSEL_SWITCH ? get_current_x_position(current_time) : IMAGE_CAROUSEL_MARGIN_LEFT);
        }

        // 只在非调试模式下检查显示持续时间
        if (!IMAGE_CAROUSEL_DEBUG_MODE && 
            (current_time - start_time) / 1000 >= IMAGE_CAROUSEL_DISPLAY_DURATION) {
            quit = 1;
        }

        SDL_Delay(1);
    }

    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    Shell_NotifyIcon(NIM_DELETE, &nid);
    #endif
    
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
    
    if (second_window) {
        SDL_DestroyWindow(second_window);
    }
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    // 释放资源
    for (int i = 0; i < image_count; i++) {
        free(image_files[i]);
    }
    free(image_files);

    if (moving_image_files) {
        for (int i = 0; i < moving_image_count; i++) {
            free(moving_image_files[i]);
        }
        free(moving_image_files);
    }

    return 0;
}
