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

// 配置文件路径（全局变量）
const char* config_path = "./asset/config.txt";

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
    int control_time;
    int positions[2];
    int moving_interval;
    int current_mode; // 当前模式：1-固定，2-移动
    int enable_dragging; // 是否启用拖动窗口
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
    // 拖动窗口相关变量
    int is_dragging;
    int drag_offset_x;
    int drag_offset_y;
} WindowContext;

// 全局变量
ConfigState current_config_state = {0};
#define MAX_CACHE_SIZE 4501
WindowContext main_context = {0};
Uint32 start_time;
Uint32 move_start_time = 0;
static int position_index = 0;
static int direction = 1;
static float current_progress = 0.0f;

// 函数声明
void clear_image_cache(WindowContext* context);
void save_config_state(const char *filename, ConfigState* state);
void write_config(const char *filename, ConfigState* state);
void init_image_cache(WindowContext* context);
SDL_Surface* process_alpha(SDL_Surface* surface);
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc);
int get_png_files(const char *dir, char ***image_files);
void load_config(const char *filename);
time_t get_file_modification_time(const char *filename);
SDL_Window* create_window(const char* title, int x_pos, int y_pos, int width, int height, HWND* out_hwnd);
void ensure_window_top_most(WindowContext* context, int x_pos);
int get_current_x_position(Uint32 current_time, ConfigState* config);
void update_window_context(WindowContext* context, const char* dir, int scale_factor);
void preload_next_image(WindowContext* context);
int check_config_changes(ConfigState* old_state, ConfigState* new_state, WindowContext* context);
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory);

// 提取文件名中的数字
int extract_number(const char* filename) {
    const char* name = strrchr(filename, '\\');
    if (name) {
        name++; // 跳过'\\'
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
        fprintf(stderr, "无法打开目录: %s\n", dir);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        const char *ext = strrchr(entry->d_name, '.');
        if (ext && (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".PNG") == 0)) {
            char *full_path = malloc(strlen(dir) + strlen(entry->d_name) + 2);
            if (!full_path) {
                fprintf(stderr, "内存分配失败\n");
                closedir(d);
                return count;
            }
            sprintf(full_path, "%s\\%s", dir, entry->d_name);
            (*image_files) = realloc(*image_files, sizeof(char*) * (count + 1));
            if (!(*image_files)) {
                fprintf(stderr, "内存重新分配失败\n");
                free(full_path);
                closedir(d);
                return count;
            }
            (*image_files)[count] = full_path;
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
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "无法打开配置文件: %s\n", filename);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\n' || line[0] == '#') continue;

        if (sscanf(line, "IMAGE_CAROUSEL_SCALE_FACTOR=%d", &current_config_state.scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%255s", current_config_state.image_dir) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d", &current_config_state.switch_interval) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_SCALE_FACTOR=%d", &current_config_state.moving_scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_DIR=%255s", current_config_state.moving_dir) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_INTERVAL=%d", &current_config_state.moving_interval) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MOVING_MARGIN_TOP=%d", &current_config_state.moving_margin_top) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &current_config_state.edge_size) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &current_config_state.margin_left) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &current_config_state.margin_top) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &current_config_state.show_tray_icon) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &current_config_state.display_duration) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_CONTROL_TIME=%d", &current_config_state.control_time) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_POSITIONS=%d,%d", &current_config_state.positions[0], &current_config_state.positions[1]) == 2) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_Current_Mode=%d", &current_config_state.current_mode) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d", &current_config_state.enable_dragging) == 1) continue;
    }

    fclose(file);
}

// 保存配置到配置文件
void write_config(const char *filename, ConfigState* state) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "无法打开配置文件进行写入: %s\n", filename);
        return;
    }

    fprintf(file, "# Image Carousel Configuration\n");
    fprintf(file, "IMAGE_CAROUSEL_SCALE_FACTOR=%d\n", state->scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_IMAGE_DIR=%s\n", state->image_dir);
    fprintf(file, "IMAGE_CAROUSEL_SWITCH_INTERVAL=%d\n", state->switch_interval);
    fprintf(file, "IMAGE_CAROUSEL_MOVING_SCALE_FACTOR=%d\n", state->moving_scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_MOVING_DIR=%s\n", state->moving_dir);
    fprintf(file, "IMAGE_CAROUSEL_MOVING_INTERVAL=%d\n", state->moving_interval);
    fprintf(file, "IMAGE_CAROUSEL_MOVING_MARGIN_TOP=%d\n", state->moving_margin_top);
    fprintf(file, "IMAGE_CAROUSEL_EDGE_SIZE=%d\n", state->edge_size);
    fprintf(file, "IMAGE_CAROUSEL_MARGIN_LEFT=%d\n", state->margin_left);
    fprintf(file, "IMAGE_CAROUSEL_MARGIN_TOP=%d\n", state->margin_top);
    fprintf(file, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d\n", state->show_tray_icon);
    fprintf(file, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d\n", state->display_duration);
    fprintf(file, "IMAGE_CAROUSEL_CONTROL_TIME=%d\n", state->control_time);
    fprintf(file, "IMAGE_CAROUSEL_POSITIONS=%d,%d\n", state->positions[0], state->positions[1]);
    fprintf(file, "IMAGE_CAROUSEL_Current_Mode=%d\n", state->current_mode);
    fprintf(file, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d\n", state->enable_dragging);

    fclose(file);
}

// 保存配置状态到配置文件
void save_config_state(const char *filename, ConfigState* state) {
    write_config(filename, state);
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
                const int dx[] = {
                    -current_config_state.edge_size, 0, current_config_state.edge_size, 
                    -current_config_state.edge_size, current_config_state.edge_size, 
                    -current_config_state.edge_size, 0, current_config_state.edge_size
                };
                const int dy[] = {
                    -current_config_state.edge_size, -current_config_state.edge_size, -current_config_state.edge_size,
                    0, 0,
                    current_config_state.edge_size, current_config_state.edge_size, current_config_state.edge_size
                };

                for (int i = 0; i < 8; i++) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];

                    if (nx >= 0 && nx < surface->w && ny >= 0 && ny < surface->h) {
                        Uint8 nr, ng, nb, na_neighbor;
                        SDL_GetRGBA(src[ny * surface->w + nx], surface->format, &nr, &ng, &nb, &na_neighbor);

                        if (na_neighbor == 0) {
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
    bmi.bmiHeader.biHeight = -surface->h; // top-down
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
    if (!context->image_cache) {
        fprintf(stderr, "无法分配图片缓存\n");
    }
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
SDL_Window* create_window(const char* title, int x_pos, int y_pos, int width, int height, HWND* out_hwnd) {
    SDL_Window* win = SDL_CreateWindow(title, 
        x_pos,
        y_pos,
        width, 
        height, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP);

    if (win) {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo(win, &wmInfo)) {
            *out_hwnd = wmInfo.info.win.window;

            // 根据拖动开关设置窗口样式
            LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
            if (!current_config_state.enable_dragging) {
                exStyle |= WS_EX_TRANSPARENT;
            }
            SetWindowLongPtr(*out_hwnd, GWL_EXSTYLE, exStyle);

            HDC hdcScreen = GetDC(NULL);
            BLENDFUNCTION blend = {0};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 255;
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
    
    int current_margin_top;
    switch (current_config_state.current_mode) {
        case 1: // 固定模式
            current_margin_top = current_config_state.margin_top;
            break;
        case 2: // 移动模式
            current_margin_top = current_config_state.moving_margin_top;
            break;
        default:
            current_margin_top = current_config_state.margin_top;
    }

    if (current_margin_top < 0) current_margin_top = 0; // 防止负值

    SetWindowPos(context->hwnd, HWND_TOPMOST, 
        x_pos,
        current_margin_top, 
        context->imgWidth, 
        context->imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// 获取当前X位置
int get_current_x_position(Uint32 current_time, ConfigState* config) {
    if ((config->current_mode == 2) && config->switch_interval > 0) {
        if (move_start_time == 0) {
            move_start_time = current_time;
        }

        float time_diff = (float)(current_time - move_start_time);
        current_progress = fmodf(time_diff / config->control_time, 2.0f);
        
        if (current_progress > 1.0f) {
            float reverse_progress = 2.0f - current_progress;
            return config->positions[1] - 
                   (config->positions[1] - config->positions[0]) * reverse_progress;
        } else {
            return config->positions[0] + 
                   (config->positions[1] - config->positions[0]) * current_progress;
        }
    }

    return config->margin_left;
}

// 更新窗口上下文和重新加载图片
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

    int current_scale;
    switch (current_config_state.current_mode) {
        case 1: // 固定模式
            current_scale = current_config_state.scale_factor;
            break;
        case 2: // 移动模式
            current_scale = current_config_state.moving_scale_factor;
            break;
        default:
            current_scale = current_config_state.scale_factor;
    }
    
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

    int current_scale;
    switch (current_config_state.current_mode) {
        case 1: // 固定模式
            current_scale = current_config_state.scale_factor;
            break;
        case 2: // 移动模式
            current_scale = current_config_state.moving_scale_factor;
            break;
        default:
            current_scale = current_config_state.scale_factor;
    }
    
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

// 检查配置变化
int check_config_changes(ConfigState* old_state, ConfigState* new_state, WindowContext* context) {
    int needs_window_update = 0;
    int needs_cache_clear = 0;

    if (old_state->current_mode != new_state->current_mode) {
        needs_window_update = 1;
        needs_cache_clear = 1;
    }

    switch (new_state->current_mode) {
        case 1: // 固定模式
            if (old_state->scale_factor != new_state->scale_factor ||
                strcmp(old_state->image_dir, new_state->image_dir) != 0 ||
                old_state->margin_left != new_state->margin_left ||
                old_state->margin_top != new_state->margin_top ||
                old_state->enable_dragging != new_state->enable_dragging) { // 拖动开关检测
                needs_window_update = 1;
                needs_cache_clear = 1;
            }
            break;
        case 2: // 移动模式
            if (old_state->moving_scale_factor != new_state->moving_scale_factor ||
                strcmp(old_state->moving_dir, new_state->moving_dir) != 0 ||
                old_state->moving_interval != new_state->moving_interval ||
                old_state->positions[0] != new_state->positions[0] ||
                old_state->positions[1] != new_state->positions[1] ||
                old_state->control_time != new_state->control_time ||
                old_state->moving_margin_top != new_state->moving_margin_top) {
                needs_window_update = 1;
                needs_cache_clear = 1;
            }
            break;
        default:
            break;
    }

    // 检查拖动开关变化
    if (old_state->enable_dragging != new_state->enable_dragging) {
        needs_window_update = 1;
    }

    if (needs_cache_clear) {
        clear_image_cache(context);
    }

    return needs_window_update;
}

// 主函数
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化配置
    load_config(config_path);
    time_t last_mod_time = get_file_modification_time(config_path);
    save_config_state(config_path, &current_config_state);

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init 错误: %s\n", SDL_GetError());
        return 1;
    }

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        fprintf(stderr, "IMG_Init 错误: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 初始化主窗口上下文
    const char* initial_dir;
    int initial_scale;
    switch (current_config_state.current_mode) {
        case 1: // 固定模式
            initial_dir = current_config_state.image_dir;
            initial_scale = current_config_state.scale_factor;
            break;
        case 2: // 移动模式
            initial_dir = current_config_state.moving_dir;
            initial_scale = current_config_state.moving_scale_factor;
            break;
        default:
            initial_dir = current_config_state.image_dir;
            initial_scale = current_config_state.scale_factor;
    }

    // 检查目录是否存在
    DIR *dir = opendir(initial_dir);
    if (dir) {
        closedir(dir);
    } else {
        fprintf(stderr, "目录不存在: %s\n", initial_dir);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    update_window_context(&main_context, initial_dir, initial_scale);
    if (main_context.image_count == 0) {
        fprintf(stderr, "在 %s 中找不到PNG文件\n", initial_dir);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建主窗口
    int initial_x, initial_y;
    switch (current_config_state.current_mode) {
        case 1: // 固定模式
            initial_x = current_config_state.margin_left;
            initial_y = current_config_state.margin_top;
            break;
        case 2: // 移动模式
            initial_x = current_config_state.positions[0];
            initial_y = current_config_state.moving_margin_top;
            break;
        default:
            initial_x = current_config_state.margin_left;
            initial_y = current_config_state.margin_top;
    }

    main_context.window = create_window("Image Carousel", 
        initial_x,
        initial_y,
        main_context.imgWidth, main_context.imgHeight, 
        &main_context.hwnd);
    if (!main_context.window) {
        fprintf(stderr, "创建主窗口失败\n");
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);

    // 主循环变量
    SDL_Event e;
    int quit = 0;
    start_time = SDL_GetTicks();
    move_start_time = SDL_GetTicks();
    main_context.last_switch_time = SDL_GetTicks();

    // 预加载第一张图片
    process_and_display_image(main_context.image_files[main_context.current_index], 
                              &main_context, hdcScreen, hdcMemory);
    
    // 预加载下一张图片
    preload_next_image(&main_context);

    // 主循环
    while (!quit) {
        // 获取当前鼠标位置
        int mouse_x, mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        SDL_Window* current_window = SDL_GetMouseFocus();
        HWND mouse_focus_hwnd = NULL;
        if (current_window) {
            SDL_SysWMinfo wmInfoMouse;
            SDL_VERSION(&wmInfoMouse.version);
            if (SDL_GetWindowWMInfo(current_window, &wmInfoMouse)) {
                mouse_focus_hwnd = wmInfoMouse.info.win.window;
            }
        }

        // 自动激活窗口如果鼠标在窗口上方且拖动启用
        if (current_config_state.enable_dragging && mouse_focus_hwnd == main_context.hwnd) {
            SDL_RaiseWindow(main_context.window);
        }

        // 等待事件或超时
        Uint32 current_time = SDL_GetTicks();
        Uint32 time_since_last_switch = current_time - main_context.last_switch_time;
        int current_interval = (current_config_state.current_mode == 1) ? 
                               current_config_state.switch_interval : 
                               current_config_state.moving_interval;

        // 计算等待时间
        int wait_time = current_interval - time_since_last_switch;
        if (wait_time < 0) wait_time = 0;

        // 等待事件或超时
        if (SDL_WaitEventTimeout(&e, wait_time)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            // 处理鼠标事件
            if (current_config_state.enable_dragging) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        main_context.is_dragging = 1;
                        POINT cursor_pos;
                        GetCursorPos(&cursor_pos);
                        RECT rect;
                        GetWindowRect(main_context.hwnd, &rect);
                        main_context.drag_offset_x = cursor_pos.x - rect.left;
                        main_context.drag_offset_y = cursor_pos.y - rect.top;
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        if (main_context.is_dragging) {
                            main_context.is_dragging = 0;
                        }
                    }
                }
                else if (e.type == SDL_MOUSEMOTION) {
                    if (main_context.is_dragging) {
                        POINT cursor_pos;
                        GetCursorPos(&cursor_pos);
                        int new_x = cursor_pos.x - main_context.drag_offset_x;
                        int new_y = cursor_pos.y - main_context.drag_offset_y;
                        SetWindowPos(main_context.hwnd, HWND_TOPMOST, new_x, new_y,
                            main_context.imgWidth, main_context.imgHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        
                        // 实时更新配置
                        current_config_state.margin_left = new_x;
                        current_config_state.margin_top = new_y;
                        save_config_state(config_path, &current_config_state);
                    }
                }
            }
        }

        current_time = SDL_GetTicks();
        time_since_last_switch = current_time - main_context.last_switch_time;

        if (time_since_last_switch >= current_interval) {
            main_context.last_switch_time += current_interval; // 累加间隔

            main_context.current_index = (main_context.current_index + 1) % main_context.image_count;
            
            process_and_display_image(main_context.image_files[main_context.current_index], 
                                      &main_context, hdcScreen, hdcMemory);
            
            preload_next_image(&main_context);

            if (current_config_state.current_mode == 2) { // 移动模式
                ensure_window_top_most(&main_context, get_current_x_position(current_time, &current_config_state));
            } else { // 固定模式
                ensure_window_top_most(&main_context, current_config_state.margin_left);
            }
        }

        // 检查配置文件修改
        time_t current_mod_time = get_file_modification_time(config_path);
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time;
            ConfigState old_state = current_config_state;
            load_config(config_path);

            if (check_config_changes(&old_state, &current_config_state, &main_context)) {
                const char* current_dir;
                int current_scale;
                switch (current_config_state.current_mode) {
                    case 1: // 固定模式
                        current_dir = current_config_state.image_dir;
                        current_scale = current_config_state.scale_factor;
                        break;
                    case 2: // 移动模式
                        current_dir = current_config_state.moving_dir;
                        current_scale = current_config_state.moving_scale_factor;
                        break;
                    default:
                        current_dir = current_config_state.image_dir;
                        current_scale = current_config_state.scale_factor;
                }
                
                update_window_context(&main_context, current_dir, current_scale);
                
                if (main_context.image_count > 0) {
                    process_and_display_image(main_context.image_files[main_context.current_index], 
                                              &main_context, hdcScreen, hdcMemory);
                    preload_next_image(&main_context);
                }

                // 更新窗口样式 based on enable_dragging
                LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
                if (!current_config_state.enable_dragging) {
                    exStyle |= WS_EX_TRANSPARENT;
                }
                SetWindowLongPtr(main_context.hwnd, GWL_EXSTYLE, exStyle);

                HDC hdcScreenUpdate = GetDC(NULL);
                BLENDFUNCTION blend = {0};
                blend.BlendOp = AC_SRC_OVER;
                blend.SourceConstantAlpha = 255;
                blend.AlphaFormat = AC_SRC_ALPHA;
                UpdateLayeredWindow(main_context.hwnd, hdcScreenUpdate, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
                ReleaseDC(NULL, hdcScreenUpdate);
            }
        }

        // 检查退出条件
        if (current_config_state.display_duration > 0 && 
            (current_time - start_time) >= current_config_state.display_duration) {
            quit = 1;
        }
    }

    // 清理资源
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
