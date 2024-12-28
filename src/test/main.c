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
char IMAGE_CAROUSEL_MOVING_DIR[256];    // 新增：移动模式下的图片目录
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
int IMAGE_CAROUSEL_MOVING_SCALE_FACTOR;
int IMAGE_CAROUSEL_MOVING_INTERVAL;

// 窗口和图片资源
typedef struct {
    SDL_Window *window;
    HWND hwnd;
    char **image_files;
    int image_count;
    int imgWidth;
    int imgHeight;
    int current_index;
    Uint32 last_switch_time;
} WindowContext;

// 主窗口上下文
WindowContext main_context = {0};

// 其他全局变量
int previous_scale_factor;
int previous_switch_interval;
int previous_moving_interval;
int previous_moving_scale;
Uint32 start_time;
int previous_duration;
static int position_index = 0;

// 移动相关的全局变量
static Uint32 move_start_time = 0;
static int direction = 1;
static float current_progress = 0.0f;

// 函数声明
int get_png_files(const char *dir, char ***image_files);
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory);
SDL_Window* create_window(const char* title, int x_pos, int width, int height, HWND* out_hwnd);
void update_window_context(WindowContext* context, const char* dir, int scale_factor);
time_t get_file_modification_time(const char *filename);
void load_config(const char *filename);
int is_png(const char *filename);
int compare(const void *a, const void *b);
SDL_Surface* process_alpha(SDL_Surface* surface);
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc);
int get_current_x_position(Uint32 current_time);
void ensure_window_top_most(WindowContext* context, int x_pos);
// 获取当前的X坐标位置
int get_current_x_position(Uint32 current_time) {
    if (IMAGE_CAROUSEL_SWITCH && !IMAGE_CAROUSEL_DEBUG_MODE) {  // 只在非调试模式下使用移动逻辑
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
void ensure_window_top_most(WindowContext* context, int x_pos) {
    if (!context || !context->hwnd) return;
    
    SetWindowPos(context->hwnd, HWND_TOPMOST, 
        x_pos,
        IMAGE_CAROUSEL_MARGIN_TOP, 
        context->imgWidth, 
        context->imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
            // 设置窗口样式
            SetWindowLongPtr(*out_hwnd, GWL_EXSTYLE, 
                WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
            
            // 初始设置窗口完全透明
            HDC hdcScreen = GetDC(NULL);
            BLENDFUNCTION blend = {0};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 0;
            blend.AlphaFormat = AC_SRC_ALPHA;
            UpdateLayeredWindow(*out_hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
            ReleaseDC(NULL, hdcScreen);
        }
    }
    return win;
}
// 更新窗口上下文
void update_window_context(WindowContext* context, const char* dir, int scale_factor) {
    if (!context) return;

    // 释放旧的图片资源
    if (context->image_files) {
        for (int i = 0; i < context->image_count; i++) {
            free(context->image_files[i]);
        }
        free(context->image_files);
        context->image_files = NULL;
    }

    // 加载新的图片
    context->image_count = get_png_files(dir, &context->image_files);
    context->current_index = 0;  // 重置图片索引
    
    // 更新窗口尺寸
    if (context->image_count > 0) {
        SDL_Surface *image = IMG_Load(context->image_files[0]);
        if (image) {
            context->imgWidth = (image->w * scale_factor) / 100;
            context->imgHeight = (image->h * scale_factor) / 100;
            SDL_FreeSurface(image);
            
            if (context->window) {
                SDL_SetWindowSize(context->window, context->imgWidth, context->imgHeight);
            }
        }
    }
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

// 读取配置文件
void load_config(const char *filename) {
    const char *config_path = "./asset/config.txt";
    FILE *file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "无法打开配置文件: %s\n", config_path);
        return;
    }

    // 保存旧值以检测变化
    int old_display_duration = IMAGE_CAROUSEL_DISPLAY_DURATION;
    int old_scale_factor = IMAGE_CAROUSEL_SCALE_FACTOR;
    int old_moving_scale = IMAGE_CAROUSEL_MOVING_SCALE_FACTOR;
    int old_moving_interval = IMAGE_CAROUSEL_MOVING_INTERVAL;
    int old_switch_mode = IMAGE_CAROUSEL_SWITCH;
    int old_positions[2] = {IMAGE_CAROUSEL_POSITIONS[0], IMAGE_CAROUSEL_POSITIONS[1]};
    int old_debug_mode = IMAGE_CAROUSEL_DEBUG_MODE;
    char old_image_dir[256] = {0};
    char old_moving_dir[256] = {0};
    strcpy(old_image_dir, IMAGE_CAROUSEL_IMAGE_DIR);
    strcpy(old_moving_dir, IMAGE_CAROUSEL_MOVING_DIR);
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#') {
            continue;
        }

        // 读取配置
        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%s", IMAGE_CAROUSEL_IMAGE_DIR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_DIR=%s", IMAGE_CAROUSEL_MOVING_DIR) == 1) continue;
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
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_MOVING_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_INTERVAL=%d", &IMAGE_CAROUSEL_MOVING_INTERVAL) == 1) continue;
    }

    fclose(file);

    // 检查并应用更改
    if (old_display_duration != IMAGE_CAROUSEL_DISPLAY_DURATION) {
        start_time = SDL_GetTicks();
    }

    // 检查移动模式相关的更改
    if (IMAGE_CAROUSEL_SWITCH) {
        // 检查移动模式下的目录变化
        if (strcmp(old_moving_dir, IMAGE_CAROUSEL_MOVING_DIR) != 0) {
            update_window_context(&main_context, IMAGE_CAROUSEL_MOVING_DIR, IMAGE_CAROUSEL_MOVING_SCALE_FACTOR);
        }
        // 检查移动模式下的缩放比例变化
        else if (old_moving_scale != IMAGE_CAROUSEL_MOVING_SCALE_FACTOR) {
            update_window_context(&main_context, IMAGE_CAROUSEL_MOVING_DIR, IMAGE_CAROUSEL_MOVING_SCALE_FACTOR);
        }
    } else {
        // 检查普通模式下的目录和缩放比例变化
        if (strcmp(old_image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0 || 
            old_scale_factor != IMAGE_CAROUSEL_SCALE_FACTOR) {
            update_window_context(&main_context, IMAGE_CAROUSEL_IMAGE_DIR, IMAGE_CAROUSEL_SCALE_FACTOR);
        }
    }

    // 检查模式切换
    if (old_switch_mode != IMAGE_CAROUSEL_SWITCH) {
        const char* current_dir = IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_DIR : IMAGE_CAROUSEL_IMAGE_DIR;
        int current_scale = IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : IMAGE_CAROUSEL_SCALE_FACTOR;
        update_window_context(&main_context, current_dir, current_scale);
    }

    // 检查调试模式变化
    if (old_debug_mode != IMAGE_CAROUSEL_DEBUG_MODE || 
        (IMAGE_CAROUSEL_DEBUG_MODE && 
         (old_positions[0] != IMAGE_CAROUSEL_POSITIONS[0] || 
          old_positions[1] != IMAGE_CAROUSEL_POSITIONS[1]))) {
        main_context.last_switch_time = SDL_GetTicks() - IMAGE_CAROUSEL_SWITCH_INTERVAL;
        position_index = 0;
    }
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
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory) {
    if (!context || !context->window || !context->hwnd) return;

    SDL_Surface *image = IMG_Load(image_path);
    if (!image) return;

    // 根据当前模式选择缩放比例
    int current_scale;
    if (IMAGE_CAROUSEL_DEBUG_MODE) {
        current_scale = IMAGE_CAROUSEL_SCALE_FACTOR;
    } else if (IMAGE_CAROUSEL_SWITCH) {
        current_scale = IMAGE_CAROUSEL_MOVING_SCALE_FACTOR;
    } else {
        current_scale = IMAGE_CAROUSEL_SCALE_FACTOR;
    }
    
    // 更新窗口尺寸（如果需要）
    int new_width = (image->w * current_scale) / 100;
    int new_height = (image->h * current_scale) / 100;
    
    if (new_width != context->imgWidth || new_height != context->imgHeight) {
        context->imgWidth = new_width;
        context->imgHeight = new_height;
        SDL_SetWindowSize(context->window, context->imgWidth, context->imgHeight);
    }
    
    SDL_Surface *scaled = SDL_CreateRGBSurface(0, context->imgWidth, context->imgHeight, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
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
                    SIZE sizeWnd = {context->imgWidth, context->imgHeight};
                    
                    RECT rect;
                    GetWindowRect(context->hwnd, &rect);
                    POINT ptDst = {rect.left, rect.top};

                    UpdateLayeredWindow(context->hwnd, hdcScreen, &ptDst, &sizeWnd, 
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
    // 初始化配置
    load_config("config.txt");
    time_t last_mod_time = get_file_modification_time("./asset/config.txt");

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG == 0) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 初始化主窗口上下文
    const char* initial_dir = IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_DIR : IMAGE_CAROUSEL_IMAGE_DIR;
    int initial_scale = IMAGE_CAROUSEL_DEBUG_MODE ? IMAGE_CAROUSEL_SCALE_FACTOR :
                       (IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : 
                        IMAGE_CAROUSEL_SCALE_FACTOR);
    
    update_window_context(&main_context, initial_dir, initial_scale);
    if (main_context.image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", initial_dir);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建主窗口
    main_context.window = create_window("Main Window", 
        IMAGE_CAROUSEL_DEBUG_MODE ? IMAGE_CAROUSEL_POSITIONS[0] : IMAGE_CAROUSEL_MARGIN_LEFT,
        main_context.imgWidth, main_context.imgHeight, 
        &main_context.hwnd);
    if (!main_context.window) {
        fprintf(stderr, "Failed to create main window\n");
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);

    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    NOTIFYICONDATA nid;
    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = main_context.hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP;
    nid.hIcon = (HICON)LoadImage(NULL, "icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    wchar_t szTip[128];
    wcsncpy(szTip, L"Image Carousel", sizeof(szTip) / sizeof(wchar_t));
    wcscpy((wchar_t*)nid.szTip, szTip);
    Shell_NotifyIcon(NIM_ADD, &nid);
    #endif

    SDL_Event e;
    int quit = 0;
    start_time = SDL_GetTicks();
    main_context.last_switch_time = SDL_GetTicks();

    // 主循环
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

        // 更新图片和位置
        int current_interval = IMAGE_CAROUSEL_DEBUG_MODE ? IMAGE_CAROUSEL_SWITCH_INTERVAL :
                             (IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_INTERVAL : 
                              IMAGE_CAROUSEL_SWITCH_INTERVAL);

        if (current_time - main_context.last_switch_time >= current_interval) {
            main_context.last_switch_time = current_time;
            
            // 更新图片
            main_context.current_index = (main_context.current_index + 1) % main_context.image_count;
            const char* current_dir = IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_DIR : IMAGE_CAROUSEL_IMAGE_DIR;
            char full_path[512];
            sprintf(full_path, "%s/%s", current_dir, 
                   strrchr(main_context.image_files[main_context.current_index], '/') + 1);
            
            process_and_display_image(full_path, &main_context, hdcScreen, hdcMemory);

            // 调试模式优先级最高
            if (IMAGE_CAROUSEL_DEBUG_MODE) {
                position_index = !position_index;  // 在两个位置之间切换
                ensure_window_top_most(&main_context, IMAGE_CAROUSEL_POSITIONS[position_index]);
            } else if (IMAGE_CAROUSEL_SWITCH) {
                // 移动模式
                ensure_window_top_most(&main_context, get_current_x_position(current_time));
            } else {
                // 普通模式
                ensure_window_top_most(&main_context, IMAGE_CAROUSEL_MARGIN_LEFT);
            }
        }

        // 检查显示持续时间
        if (!IMAGE_CAROUSEL_DEBUG_MODE && 
            (current_time - start_time) / 1000 >= IMAGE_CAROUSEL_DISPLAY_DURATION) {
            quit = 1;
        }

        SDL_Delay(1);
    }

    // 清理资源
    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    Shell_NotifyIcon(NIM_DELETE, &nid);
    #endif
    
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
    
    SDL_DestroyWindow(main_context.window);
    for (int i = 0; i < main_context.image_count; i++) {
        free(main_context.image_files[i]);
    }
    free(main_context.image_files);

    IMG_Quit();
    SDL_Quit();

    return 0;
}
