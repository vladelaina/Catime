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
    char image_dir[256];
    int switch_interval; // 单位：毫秒
    int edge_size;
    int margin_left;
    int margin_top;
    int show_tray_icon;
    int display_duration;
    int control_time; // 虽然移除了移动模式，但保留以防将来需要
    int zoom_step; // 放大缩小步长（百分比）
    int min_scale_factor; // 最小缩放比例
    int max_scale_factor; // 最大缩放比例
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
WindowContext main_context = {0};
Uint32 start_time;

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
void ensure_window_top_most(WindowContext* context);
int extract_number(const char* filename);
int compare_numbers(const void* a, const void* b);
void preload_next_image(WindowContext* context);
int check_config_changes(ConfigState* old_state, ConfigState* new_state, WindowContext* context);
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory);
void update_window_context(WindowContext* context, const char* dir, int scale_factor, int preserve_index, int current_index);
void toggle_dragging(WindowContext* context, HDC hdcScreen, HDC hdcMemory);

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
            char **temp = realloc(*image_files, sizeof(char*) * (count + 1));
            if (!temp) {
                fprintf(stderr, "内存重新分配失败\n");
                free(full_path);
                closedir(d);
                return count;
            }
            *image_files = temp;
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
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &current_config_state.edge_size) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_LEFT=%d", &current_config_state.margin_left) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MARGIN_TOP=%d", &current_config_state.margin_top) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &current_config_state.show_tray_icon) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &current_config_state.display_duration) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_ZOOM_STEP=%d", &current_config_state.zoom_step) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MIN_SCALE_FACTOR=%d", &current_config_state.min_scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MAX_SCALE_FACTOR=%d", &current_config_state.max_scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d", &current_config_state.enable_dragging) == 1) continue;
    }

    fclose(file);
}

// 保存配置状态到配置文件
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
    fprintf(file, "IMAGE_CAROUSEL_EDGE_SIZE=%d\n", state->edge_size);
    fprintf(file, "IMAGE_CAROUSEL_MARGIN_LEFT=%d\n", state->margin_left);
    fprintf(file, "IMAGE_CAROUSEL_MARGIN_TOP=%d\n", state->margin_top);
    fprintf(file, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d\n", state->show_tray_icon);
    fprintf(file, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d\n", state->display_duration);
    fprintf(file, "IMAGE_CAROUSEL_ZOOM_STEP=%d\n", state->zoom_step);
    fprintf(file, "IMAGE_CAROUSEL_MIN_SCALE_FACTOR=%d\n", state->min_scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_MAX_SCALE_FACTOR=%d\n", state->max_scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d\n", state->enable_dragging);

    fclose(file);
}

// 保存配置状态到配置文件（复制并写入配置文件）
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
    context->cache_size = context->image_count; // 设置缓存大小为图片数量
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
                context->image_cache[i].surface = NULL;
            }
            if (context->image_cache[i].bitmap) {
                DeleteObject(context->image_cache[i].bitmap);
                context->image_cache[i].bitmap = NULL;
            }
            context->image_cache[i].is_valid = 0;
        }
        free(context->image_cache);
        context->image_cache = NULL;
    }
}

// 更新窗口上下文
void update_window_context(WindowContext* context, const char* dir, int scale_factor, int preserve_index, int current_index) {
    if (!context) return;

    // 保存当前索引和切换时间
    int saved_index = preserve_index ? current_index : 0;
    Uint32 saved_switch_time = preserve_index ? context->last_switch_time : SDL_GetTicks();

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
    context->current_index = saved_index % (context->image_count > 0 ? context->image_count : 1); // 确保索引在范围内

    // 初始化新缓存
    init_image_cache(context);

    // 更新窗口尺寸
    if (context->image_count > 0) {
        SDL_Surface *image = IMG_Load(context->image_files[context->current_index]);
        if (image) {
            context->imgWidth = (image->w * scale_factor) / 100;
            context->imgHeight = (image->h * scale_factor) / 100;
            SDL_FreeSurface(image);

            if (context->window) {
                SDL_SetWindowSize(context->window, context->imgWidth, context->imgHeight);
            }
        }
    }

    // 恢复切换时间
    context->last_switch_time = saved_switch_time;
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
void ensure_window_top_most(WindowContext* context) {
    if (!context || !context->hwnd) return;

    SetWindowPos(context->hwnd, HWND_TOPMOST, 
        current_config_state.margin_left,
        current_config_state.margin_top, 
        context->imgWidth, 
        context->imgHeight, 
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
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

    int current_scale = current_config_state.scale_factor;

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
                } else {
                    SDL_FreeSurface(converted); // 确保在失败时释放资源
                }
                
                DeleteDC(hdcMemory);
                ReleaseDC(NULL, hdcScreen);
            } else {
                SDL_FreeSurface(processed);
            }
        } else {
            SDL_FreeSurface(scaled);
        }
    } else {
        SDL_FreeSurface(image);
    }
}

// 检查配置变化
int check_config_changes(ConfigState* old_state, ConfigState* new_state, WindowContext* context) {
    int needs_window_update = 0;
    int needs_cache_clear = 0;

    if (old_state->scale_factor != new_state->scale_factor ||
        strcmp(old_state->image_dir, new_state->image_dir) != 0 ||
        old_state->margin_left != new_state->margin_left ||
        old_state->margin_top != new_state->margin_top ||
        old_state->enable_dragging != new_state->enable_dragging ||
        old_state->zoom_step != new_state->zoom_step ||
        old_state->min_scale_factor != new_state->min_scale_factor ||
        old_state->max_scale_factor != new_state->max_scale_factor) { // 拖动开关和缩放相关检测
        needs_window_update = 1;
        needs_cache_clear = 1;
    }

    if (needs_cache_clear) {
        clear_image_cache(context);
    }

    return needs_window_update;
}

// 处理并显示图片
void process_and_display_image(const char* image_path, WindowContext* context, HDC hdcScreen, HDC hdcMemory) {
    if (!context || !context->window || !context->hwnd) return;

    // 检查缓存
    ImageCache* cache = &context->image_cache[context->current_index];
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

    int current_scale = current_config_state.scale_factor;
    
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
                } else {
                    SDL_FreeSurface(converted); // 确保在失败时释放资源
                }
            } else {
                SDL_FreeSurface(processed);
            }
        } else {
            SDL_FreeSurface(scaled);
        }
    } else {
        SDL_FreeSurface(image);
    }
}

// 切换拖动功能
void toggle_dragging(WindowContext* context, HDC hdcScreen, HDC hdcMemory) {
    if (current_config_state.enable_dragging) {
        current_config_state.enable_dragging = 0;
        save_config_state(config_path, &current_config_state);

        // 更新窗口样式
        LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
        SetWindowLongPtr(context->hwnd, GWL_EXSTYLE, exStyle);

        BLENDFUNCTION blend = {0};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(context->hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);

        // 如果禁用拖动，重置拖动状态
        context->is_dragging = 0;

        printf("拖动功能已禁用\n");
    }
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
    const char* initial_dir = current_config_state.image_dir;
    int initial_scale = current_config_state.scale_factor;

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

    // 加载图片列表并初始化缓存
    main_context.image_count = get_png_files(initial_dir, &main_context.image_files);
    if (main_context.image_count == 0) {
        fprintf(stderr, "在 %s 中找不到PNG文件\n", initial_dir);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    init_image_cache(&main_context);

    main_context.current_index = 0;

    // 设置窗口尺寸
    SDL_Surface *image = IMG_Load(main_context.image_files[main_context.current_index]);
    if (image) {
        main_context.imgWidth = (image->w * initial_scale) / 100;
        main_context.imgHeight = (image->h * initial_scale) / 100;
        SDL_FreeSurface(image);
    } else {
        fprintf(stderr, "无法加载图片: %s\n", main_context.image_files[main_context.current_index]);
        clear_image_cache(&main_context);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建主窗口
    main_context.window = create_window("Image Carousel", 
        current_config_state.margin_left,
        current_config_state.margin_top,
        main_context.imgWidth, main_context.imgHeight, 
        &main_context.hwnd);
    if (!main_context.window) {
        fprintf(stderr, "创建主窗口失败\n");
        clear_image_cache(&main_context);
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
    main_context.last_switch_time = SDL_GetTicks();

    // 预加载第一张图片
    process_and_display_image(main_context.image_files[main_context.current_index], 
                              &main_context, hdcScreen, hdcMemory);
    
    // 预加载下一张图片
    preload_next_image(&main_context);

    // 主循环
    while (!quit) {
        // 处理所有待处理的事件
        while (SDL_PollEvent(&e)) {
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

                        // 激活窗口以响应拖动
                        SetForegroundWindow(main_context.hwnd);
                    }
                    else if (e.button.button == SDL_BUTTON_RIGHT) {
                        // 右键点击时禁用拖动功能
                        toggle_dragging(&main_context, hdcScreen, hdcMemory);
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
                else if (e.type == SDL_MOUSEWHEEL) {
                    // 检查鼠标是否在窗口上方
                    POINT cursor_pos;
                    GetCursorPos(&cursor_pos);
                    RECT rect;
                    GetWindowRect(main_context.hwnd, &rect);
                    if (cursor_pos.x >= rect.left && cursor_pos.x <= rect.right &&
                        cursor_pos.y >= rect.top && cursor_pos.y <= rect.bottom) {
                        // 根据滚轮方向调整缩放比例
                        if (e.wheel.y > 0) { // 向前滚动，放大
                            current_config_state.scale_factor += current_config_state.zoom_step;
                            if (current_config_state.scale_factor > current_config_state.max_scale_factor) {
                                current_config_state.scale_factor = current_config_state.max_scale_factor;
                            }
                        }
                        else if (e.wheel.y < 0) { // 向后滚动，缩小
                            current_config_state.scale_factor -= current_config_state.zoom_step;
                            if (current_config_state.scale_factor < current_config_state.min_scale_factor) {
                                current_config_state.scale_factor = current_config_state.min_scale_factor;
                            }
                        }

                        // 更新当前窗口上下文，保留当前索引
                        update_window_context(&main_context, current_config_state.image_dir, current_config_state.scale_factor, 1, main_context.current_index);
                        if (main_context.image_count > 0) {
                            process_and_display_image(main_context.image_files[main_context.current_index], 
                                                      &main_context, hdcScreen, hdcMemory);
                            preload_next_image(&main_context);
                        }

                        // 保存配置
                        save_config_state(config_path, &current_config_state);
                    }
                }
            }
            // 处理鼠标悬停以激活窗口拖动
            if (current_config_state.enable_dragging) {
                if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
                    POINT cursor_pos;
                    GetCursorPos(&cursor_pos);
                    RECT rect;
                    GetWindowRect(main_context.hwnd, &rect);
                    if (cursor_pos.x >= rect.left && cursor_pos.x <= rect.right &&
                        cursor_pos.y >= rect.top && cursor_pos.y <= rect.bottom) {
                        // 激活窗口，使其可拖动
                        SetForegroundWindow(main_context.hwnd);
                    }
                }
            }
        }

        // 获取当前鼠标位置并激活窗口（新增部分）
        if (current_config_state.enable_dragging) {
            POINT cursor_pos;
            GetCursorPos(&cursor_pos);
            RECT rect;
            GetWindowRect(main_context.hwnd, &rect);
            if (cursor_pos.x >= rect.left && cursor_pos.x <= rect.right &&
                cursor_pos.y >= rect.top && cursor_pos.y <= rect.bottom) {
                // 鼠标悬停在窗口上时激活窗口
                SetForegroundWindow(main_context.hwnd);
            }
        }

        Uint32 current_time = SDL_GetTicks();
        Uint32 time_since_last_switch = current_time - main_context.last_switch_time;
        int current_interval = current_config_state.switch_interval;

        if (time_since_last_switch >= current_interval) {
            // 修正last_switch_time以保持恒定速度
            main_context.last_switch_time = current_time;

            main_context.current_index = (main_context.current_index + 1) % main_context.image_count;
            
            process_and_display_image(main_context.image_files[main_context.current_index], 
                                      &main_context, hdcScreen, hdcMemory);
            
            preload_next_image(&main_context);

            ensure_window_top_most(&main_context);
        }

        // 检查配置文件修改
        time_t current_mod_time = get_file_modification_time(config_path);
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time;
            ConfigState old_state = current_config_state;
            load_config(config_path);

            if (check_config_changes(&old_state, &current_config_state, &main_context)) {
                const char* current_dir = current_config_state.image_dir;
                int current_scale = current_config_state.scale_factor;
                
                // 保留当前索引
                update_window_context(&main_context, current_dir, current_scale, 1, main_context.current_index);
                
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

                BLENDFUNCTION blend = {0};
                blend.BlendOp = AC_SRC_OVER;
                blend.SourceConstantAlpha = 255;
                blend.AlphaFormat = AC_SRC_ALPHA;
                UpdateLayeredWindow(main_context.hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
            }
        }

        // 检查退出条件
        if (current_config_state.display_duration > 0 && 
            (current_time - start_time) >= current_config_state.display_duration) {
            quit = 1;
        }

        // 延时以避免高CPU占用，设置为1毫秒以支持高频率
        SDL_Delay(1);
    }

    // 软件退出时确保拖动功能关闭
    if (current_config_state.enable_dragging) {
        current_config_state.enable_dragging = 0;
        save_config_state(config_path, &current_config_state);

        // 更新窗口样式
        LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
        SetWindowLongPtr(main_context.hwnd, GWL_EXSTYLE, exStyle);

        BLENDFUNCTION blend = {0};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(main_context.hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
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
