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

// 配置状态结构
typedef struct {
    int scale_factor;
    int moving_scale_factor;
    char image_dir[256];
    char moving_dir[256];
    int switch_interval;
    int edge_size;
    int margin_left;
    int margin_top;
    int moving_margin_top;
    int show_tray_icon;
    int display_duration;
    int switch_mode;
    int control_time;
    int positions[2];
    int debug_mode;
    int moving_interval;
} ConfigState;

// 图片缓存结构
typedef struct {
    SDL_Surface* surface;
    HBITMAP bitmap;
    int width;
    int height;
    time_t last_used;
    int is_valid;
} ImageCache;

// 窗口上下文结构
typedef struct {
    SDL_Window *window;
    HWND hwnd;
    char **image_files;
    int image_count;
    int imgWidth;
    int imgHeight;
    int current_index;
    Uint32 last_switch_time;
    ImageCache* image_cache;
    int cache_size;
} WindowContext;

// 函数声明
void clear_image_cache(WindowContext* context);
void save_config_state(ConfigState* state);
void init_image_cache(WindowContext* context);
SDL_Surface* process_alpha(SDL_Surface* surface);
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc);
int get_png_files(const char *dir, char ***image_files);
void load_config(const char *filename);
time_t get_file_modification_time(const char *filename);
SDL_Window* create_window(const char* title, int x_pos, int width, int height, HWND* out_hwnd);
void ensure_window_top_most(WindowContext* context, int x_pos);
int get_current_x_position(Uint32 current_time);
void update_window_context(WindowContext* context, const char* dir, int scale_factor);
void preload_next_image(WindowContext* context);
int check_config_changes(ConfigState* old_state);
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory);
int extract_number(const char* filename);
int compare_numbers(const void* a, const void* b);

// 基础显示配置
int IMAGE_CAROUSEL_SCALE_FACTOR;        
char IMAGE_CAROUSEL_IMAGE_DIR[256];     
char IMAGE_CAROUSEL_MOVING_DIR[256];    
int IMAGE_CAROUSEL_SWITCH_INTERVAL;      
int IMAGE_CAROUSEL_EDGE_SIZE;            
int IMAGE_CAROUSEL_MARGIN_LEFT;          
int IMAGE_CAROUSEL_MARGIN_TOP;           
int IMAGE_CAROUSEL_MOVING_MARGIN_TOP;    
int IMAGE_CAROUSEL_SHOW_TRAY_ICON;       
int IMAGE_CAROUSEL_DISPLAY_DURATION;     
int IMAGE_CAROUSEL_SWITCH;                
int IMAGE_CAROUSEL_CONTROL_TIME;          
int IMAGE_CAROUSEL_POSITIONS[2];          
int IMAGE_CAROUSEL_DEBUG_MODE;           
int IMAGE_CAROUSEL_MOVING_SCALE_FACTOR;
int IMAGE_CAROUSEL_MOVING_INTERVAL;

#define MAX_CACHE_SIZE 4501

// 主窗口上下文
WindowContext main_context = {0};

// 当前配置状态
ConfigState current_config_state = {0};

// 其他全局变量
Uint32 start_time;
Uint32 move_start_time = 0;
static int position_index = 0;
static int direction = 1;
static float current_progress = 0.0f;
// 提取文件名中的数字
int extract_number(const char* filename) {
    const char* name = strrchr(filename, '/');
    if (name) {
        name++; // 跳过'/'
    } else {
        name = filename;
    }
    
    int number = 0;
    sscanf(name, "%d", &number);
    return number;
}

// 数字排序比较函数
int compare_numbers(const void* a, const void* b) {
    const char* file_a = *(const char**)a;
    const char* file_b = *(const char**)b;
    
    int num_a = extract_number(file_a);
    int num_b = extract_number(file_b);
    
    return num_a - num_b;
}

// 获取PNG文件列表
int get_png_files(const char *dir, char ***image_files) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "Failed to open directory: %s\n", dir);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".png") == 0) {
            (*image_files) = realloc(*image_files, sizeof(char*) * (count + 1));
            (*image_files)[count] = malloc(strlen(dir) + strlen(entry->d_name) + 2);
            sprintf((*image_files)[count], "%s/%s", dir, entry->d_name);
            count++;
        }
    }
    closedir(d);

    if (count > 0) {
        // 使用数字排序
        qsort(*image_files, count, sizeof(char*), compare_numbers);
    }

    return count;
}

// 获取文件修改时间
time_t get_file_modification_time(const char *filename) {
    struct stat fileInfo;
    if (stat(filename, &fileInfo) == 0) {
        return fileInfo.st_mtime;
    }
    return -1;
}

// 加载配置
void load_config(const char *filename) {
    FILE *file = fopen("./asset/config.txt", "r");
    if (!file) {
        fprintf(stderr, "Cannot open config file: %s\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#') continue;

        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &IMAGE_CAROUSEL_SCALE_FACTOR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%s", IMAGE_CAROUSEL_IMAGE_DIR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_DIR=%s", IMAGE_CAROUSEL_MOVING_DIR) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d", &IMAGE_CAROUSEL_SWITCH_INTERVAL) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &IMAGE_CAROUSEL_EDGE_SIZE) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &IMAGE_CAROUSEL_MARGIN_LEFT) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &IMAGE_CAROUSEL_MARGIN_TOP) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_MARGIN_TOP=%d", &IMAGE_CAROUSEL_MOVING_MARGIN_TOP) == 1) continue;
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
}

// 保存配置状态
void save_config_state(ConfigState* state) {
    state->scale_factor = IMAGE_CAROUSEL_SCALE_FACTOR;
    state->moving_scale_factor = IMAGE_CAROUSEL_MOVING_SCALE_FACTOR;
    strcpy(state->image_dir, IMAGE_CAROUSEL_IMAGE_DIR);
    strcpy(state->moving_dir, IMAGE_CAROUSEL_MOVING_DIR);
    state->switch_interval = IMAGE_CAROUSEL_SWITCH_INTERVAL;
    state->edge_size = IMAGE_CAROUSEL_EDGE_SIZE;
    state->margin_left = IMAGE_CAROUSEL_MARGIN_LEFT;
    state->margin_top = IMAGE_CAROUSEL_MARGIN_TOP;
    state->moving_margin_top = IMAGE_CAROUSEL_MOVING_MARGIN_TOP;
    state->show_tray_icon = IMAGE_CAROUSEL_SHOW_TRAY_ICON;
    state->display_duration = IMAGE_CAROUSEL_DISPLAY_DURATION;
    state->switch_mode = IMAGE_CAROUSEL_SWITCH;
    state->control_time = IMAGE_CAROUSEL_CONTROL_TIME;
    state->positions[0] = IMAGE_CAROUSEL_POSITIONS[0];
    state->positions[1] = IMAGE_CAROUSEL_POSITIONS[1];
    state->debug_mode = IMAGE_CAROUSEL_DEBUG_MODE;
    state->moving_interval = IMAGE_CAROUSEL_MOVING_INTERVAL;
}
// 处理 alpha 通道
SDL_Surface* process_alpha(SDL_Surface* surface) {
    SDL_Surface* result = SDL_CreateRGBSurface(0, surface->w, surface->h, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    if (!result) return NULL;
    
    SDL_LockSurface(surface);
    SDL_LockSurface(result);
    
    Uint32* src = (Uint32*)surface->pixels;
    Uint32* dst = (Uint32*)result->pixels;
    
    // 复制原始数据
    for (int i = 0; i < surface->w * surface->h; i++) {
        dst[i] = src[i];
    }
    
    // 处理边缘像素
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            int idx = y * surface->w + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(src[idx], surface->format, &r, &g, &b, &a);
            
            if (a > 0) {
                const int dx[] = {-IMAGE_CAROUSEL_EDGE_SIZE, 0, IMAGE_CAROUSEL_EDGE_SIZE, 
                                -IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE, 
                                -IMAGE_CAROUSEL_EDGE_SIZE, 0, IMAGE_CAROUSEL_EDGE_SIZE};
                const int dy[] = {-IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE, -IMAGE_CAROUSEL_EDGE_SIZE,
                                0, 0,
                                IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE, IMAGE_CAROUSEL_EDGE_SIZE};
                
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

// SDL表面转换为Windows位图
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

// 初始化图片缓存
void init_image_cache(WindowContext* context) {
    if (context->image_cache) {
        clear_image_cache(context);
    }
    context->cache_size = context->image_count < MAX_CACHE_SIZE ? context->image_count : MAX_CACHE_SIZE;
    context->image_cache = (ImageCache*)calloc(context->cache_size, sizeof(ImageCache));
}

// 清理图片缓存
void clear_image_cache(WindowContext* context) {
    if (context->image_cache) {
        for (int i = 0; i < context->cache_size; i++) {
            if (context->image_cache[i].surface) {
                SDL_FreeSurface(context->image_cache[i].surface);
            }
            if (context->image_cache[i].bitmap) {
                DeleteObject(context->image_cache[i].bitmap);
            }
        }
        free(context->image_cache);
        context->image_cache = NULL;
    }
}

// 更新窗口上下文
void update_window_context(WindowContext* context, const char* dir, int scale_factor) {
    if (!context) return;

    // 清理旧资源
    clear_image_cache(context);
    if (context->image_files) {
        for (int i = 0; i < context->image_count; i++) {
            free(context->image_files[i]);
        }
        free(context->image_files);
        context->image_files = NULL;
    }

    // 加载新图片列表
    context->image_count = get_png_files(dir, &context->image_files);
    context->current_index = 0;
    
    // 初始化新缓存
    init_image_cache(context);
    
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
// 创建窗口
SDL_Window* create_window(const char* title, int x_pos, int width, int height, HWND* out_hwnd) {
    int initial_margin_top = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                            IMAGE_CAROUSEL_MOVING_MARGIN_TOP : 
                            IMAGE_CAROUSEL_MARGIN_TOP;
                            
    SDL_Window* win = SDL_CreateWindow(title, 
        x_pos,
        initial_margin_top,
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

// 确保窗口在最顶层
void ensure_window_top_most(WindowContext* context, int x_pos) {
    if (!context || !context->hwnd) return;
    
    int current_margin_top = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ?
                            IMAGE_CAROUSEL_MOVING_MARGIN_TOP :
                            IMAGE_CAROUSEL_MARGIN_TOP;
    
    SetWindowPos(context->hwnd, HWND_TOPMOST, 
        x_pos,
        current_margin_top, 
        context->imgWidth, 
        context->imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// 获取当前X位置
int get_current_x_position(Uint32 current_time) {
    if ((IMAGE_CAROUSEL_SWITCH || IMAGE_CAROUSEL_DEBUG_MODE) && !IMAGE_CAROUSEL_DEBUG_MODE) {
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

// 检查配置变化
int check_config_changes(ConfigState* old_state) {
    int needs_window_update = 0;
    int needs_cache_clear = 0;
    
    if (old_state->scale_factor != IMAGE_CAROUSEL_SCALE_FACTOR ||
        old_state->moving_scale_factor != IMAGE_CAROUSEL_MOVING_SCALE_FACTOR) {
        needs_window_update = 1;
        needs_cache_clear = 1;
    }
    
    if (strcmp(old_state->image_dir, IMAGE_CAROUSEL_IMAGE_DIR) != 0 ||
        strcmp(old_state->moving_dir, IMAGE_CAROUSEL_MOVING_DIR) != 0) {
        needs_window_update = 1;
        needs_cache_clear = 1;
    }
    
    if (old_state->margin_top != IMAGE_CAROUSEL_MARGIN_TOP ||
        old_state->moving_margin_top != IMAGE_CAROUSEL_MOVING_MARGIN_TOP ||
        old_state->margin_left != IMAGE_CAROUSEL_MARGIN_LEFT ||
        old_state->positions[0] != IMAGE_CAROUSEL_POSITIONS[0] ||
        old_state->positions[1] != IMAGE_CAROUSEL_POSITIONS[1]) {
        needs_window_update = 1;
    }
    
    save_config_state(old_state);
    
    return needs_window_update;
}
// 处理和显示图像
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory) {
    if (!context || !context->window || !context->hwnd) return;
    
    // 检查缓存
    ImageCache* cache = &context->image_cache[context->current_index % context->cache_size];
    if (cache->is_valid) {
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, cache->bitmap);
        
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
        return;
    }

    // 加载和处理新图片
    SDL_Surface *image = IMG_Load(image_path);
    if (!image) return;

    int current_scale = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                       IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : 
                       IMAGE_CAROUSEL_SCALE_FACTOR;
    
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
                    // 更新缓存
                    cache->surface = converted;
                    cache->bitmap = hBitmap;
                    cache->width = context->imgWidth;
                    cache->height = context->imgHeight;
                    cache->last_used = time(NULL);
                    cache->is_valid = 1;

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
                }
            }
        }
    }
}

// 预加载下一张图片
void preload_next_image(WindowContext* context) {
    if (!context || context->image_count <= 1) return;
    
    int next_index = (context->current_index + 1) % context->image_count;
    int cache_index = next_index % context->cache_size;
    
    if (context->image_cache[cache_index].is_valid) {
        return;
    }
    
    SDL_Surface *image = IMG_Load(context->image_files[next_index]);
    if (!image) return;

    int current_scale = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                       IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : 
                       IMAGE_CAROUSEL_SCALE_FACTOR;
    
    int new_width = (image->w * current_scale) / 100;
    int new_height = (image->h * current_scale) / 100;
    
    SDL_Surface *scaled = SDL_CreateRGBSurface(0, new_width, new_height, 32,
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
                HDC hdcScreen = GetDC(NULL);
                HDC hdcMemory = CreateCompatibleDC(hdcScreen);
                HBITMAP hBitmap = SDLSurfaceToWinBitmap(converted, hdcMemory);
                
                if (hBitmap) {
                    context->image_cache[cache_index].surface = converted;
                    context->image_cache[cache_index].bitmap = hBitmap;
                    context->image_cache[cache_index].width = new_width;
                    context->image_cache[cache_index].height = new_height;
                    context->image_cache[cache_index].last_used = time(NULL);
                    context->image_cache[cache_index].is_valid = 1;
                }
                
                DeleteDC(hdcMemory);
                ReleaseDC(NULL, hdcScreen);
            }
        }
    }
}

// 主函数
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化配置
    load_config("config.txt");
    time_t last_mod_time = get_file_modification_time("./asset/config.txt");
    save_config_state(&current_config_state);

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
    const char* initial_dir = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                            IMAGE_CAROUSEL_MOVING_DIR : IMAGE_CAROUSEL_IMAGE_DIR;
    int initial_scale = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                       IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : IMAGE_CAROUSEL_SCALE_FACTOR;
    
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

    // 主循环变量
    SDL_Event e;
    int quit = 0;
    start_time = SDL_GetTicks();
    move_start_time = SDL_GetTicks();
    main_context.last_switch_time = SDL_GetTicks();
    Uint32 last_frame_time = SDL_GetTicks();
    const int TARGET_FPS = 60;
    const int FRAME_DELAY = 1000 / TARGET_FPS;

    // 预加载第一张图片
    process_and_display_image(main_context.image_files[main_context.current_index], 
                            &main_context, hdcScreen, hdcMemory);
    
    // 预加载下一张图片
    preload_next_image(&main_context);

    // 主循环
    while (!quit) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
                break;
            }
        }

        if (quit) break;

        Uint32 current_time = SDL_GetTicks();

        // 检查配置文件修改
        time_t current_mod_time = get_file_modification_time("./asset/config.txt");
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time;
            load_config("config.txt");
            
            if (check_config_changes(&current_config_state)) {
                const char* current_dir = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                                        IMAGE_CAROUSEL_MOVING_DIR : IMAGE_CAROUSEL_IMAGE_DIR;
                int current_scale = (IMAGE_CAROUSEL_DEBUG_MODE || IMAGE_CAROUSEL_SWITCH) ? 
                                  IMAGE_CAROUSEL_MOVING_SCALE_FACTOR : IMAGE_CAROUSEL_SCALE_FACTOR;
                
                update_window_context(&main_context, current_dir, current_scale);
                
                if (main_context.image_count > 0) {
                    process_and_display_image(main_context.image_files[main_context.current_index], 
                                           &main_context, hdcScreen, hdcMemory);
                    preload_next_image(&main_context);
                }
            }
        }

        // 更新图片和位置
        int current_interval = IMAGE_CAROUSEL_DEBUG_MODE ? IMAGE_CAROUSEL_SWITCH_INTERVAL :
                             (IMAGE_CAROUSEL_SWITCH ? IMAGE_CAROUSEL_MOVING_INTERVAL : 
                              IMAGE_CAROUSEL_SWITCH_INTERVAL);

        if (current_time - main_context.last_switch_time >= current_interval) {
            main_context.last_switch_time = current_time;
            
            main_context.current_index = (main_context.current_index + 1) % main_context.image_count;
            
            process_and_display_image(main_context.image_files[main_context.current_index], 
                                   &main_context, hdcScreen, hdcMemory);
            
            preload_next_image(&main_context);

            if (IMAGE_CAROUSEL_DEBUG_MODE) {
                position_index = !position_index;
                ensure_window_top_most(&main_context, IMAGE_CAROUSEL_POSITIONS[position_index]);
            } else if (IMAGE_CAROUSEL_SWITCH) {
                ensure_window_top_most(&main_context, get_current_x_position(current_time));
            } else {
                ensure_window_top_most(&main_context, IMAGE_CAROUSEL_MARGIN_LEFT);
            }
        }

        // 检查退出条件
        if (!IMAGE_CAROUSEL_DEBUG_MODE) {
            if (IMAGE_CAROUSEL_SWITCH) {
                if ((current_time - move_start_time) >= IMAGE_CAROUSEL_CONTROL_TIME) {
                    quit = 1;
                }
            } else {
                if (IMAGE_CAROUSEL_DISPLAY_DURATION > 0 && 
                    (current_time - start_time) >= IMAGE_CAROUSEL_DISPLAY_DURATION) {
                    quit = 1;
                }
            }
        }

        // 帧率控制
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    // 清理资源
    #if IMAGE_CAROUSEL_SHOW_TRAY_ICON
    Shell_NotifyIcon(NIM_DELETE, &nid);
    #endif
    
    clear_image_cache(&main_context);
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
