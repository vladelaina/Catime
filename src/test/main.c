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

// 基础路径定义
const char* BASE_IMAGE_PATH = "./asset/images/moving";
// 配置文件路径（全局变量）
const char* config_path = "./asset/config.txt";

// 配置状态结构
typedef struct {
    int scale_factor;
    char image_dir[256];
    int switch_interval; // 单位：毫秒
    int switch_speed_step; // 切换速度调整步长
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

// 新增：存储所有子目录
char **image_dirs = NULL;
int image_dir_count = 0;
int current_dir_index = 0;

// 函数声明
void clear_image_cache(WindowContext* context);
void save_config_state(const char *filename, ConfigState* state);
void write_config(const char *filename, ConfigState* state);
void init_image_cache(WindowContext* context);
SDL_Surface* process_alpha(SDL_Surface* surface);
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc);
int get_png_files(const char *dir, char ***image_files);
int get_subdirectories(const char *dir, char ***subdirs);
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
void switch_to_next_directory();
void switch_to_previous_directory();
void increase_switch_speed();
void decrease_switch_speed();

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
            sprintf(full_path, "%s/%s", dir, entry->d_name);
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
        qsort(*image_files, count, sizeof(char*), compare_numbers);
    }

    return count;
}

// 获取所有子目录
int get_subdirectories(const char *dir, char ***subdirs) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "无法打开目录: %s\n", dir);
        return 0;
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *full_path = malloc(strlen(dir) + strlen(entry->d_name) + 2);
        if (!full_path) {
            fprintf(stderr, "内存分配失败\n");
            closedir(d);
            return count;
        }
        sprintf(full_path, "%s\\%s", dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (st.st_mode & S_IFDIR) {
                char **temp = realloc(*subdirs, sizeof(char*) * (count + 1));
                if (!temp) {
                    fprintf(stderr, "内存重新分配失败\n");
                    free(full_path);
                    closedir(d);
                    return count;
                }
                *subdirs = temp;
                (*subdirs)[count] = full_path;
                count++;
            } else {
                free(full_path);
            }
        } else {
            free(full_path);
        }
    }
    closedir(d);

    if (count > 0) {
        qsort(*subdirs, count, sizeof(char*), compare_numbers);
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

    char line[512];
    char temp_image_dir[256] = {0};
    char temp_margins[256] = {0};
    int margins_values[4] = {0}; // 左边距, 上边距, 切换间隔, 缩放因子

    while (fgets(line, sizeof(line), file)) {
        // 忽略空行和注释
        if (line[0] == '\n' || line[0] == '#' || line[0] == '\r') continue;

        if (sscanf(line, "IMAGE_CAROUSEL_IMAGE_DIR=%255s", temp_image_dir) == 1) {
            // 检查是否是完整路径
            if (strstr(temp_image_dir, BASE_IMAGE_PATH) == NULL) {
                // 如果不是完整路径，添加基础路径
                snprintf(current_config_state.image_dir, sizeof(current_config_state.image_dir),
                        "%s\\%s", BASE_IMAGE_PATH, temp_image_dir);
            } else {
                strncpy(current_config_state.image_dir, temp_image_dir, sizeof(current_config_state.image_dir) - 1);
            }
            continue;
        }

        if (sscanf(line, "IMAGE_CAROUSEL_SWITCH_SPEED_STEP=%d", &current_config_state.switch_speed_step) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_EDGE_SIZE=%d", &current_config_state.edge_size) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d", &current_config_state.show_tray_icon) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d", &current_config_state.display_duration) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_ZOOM_STEP=%d", &current_config_state.zoom_step) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MIN_SCALE_FACTOR=%d", &current_config_state.min_scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_MAX_SCALE_FACTOR=%d", &current_config_state.max_scale_factor) == 1) continue;
        if (sscanf(line, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d", &current_config_state.enable_dragging) == 1) continue;

        // 解析轮播区域设置
        if (sscanf(line, "IMAGE_CAROUSEL_MARGINS=%255[^'\n']", temp_margins) == 1) {
            // 将逗号分隔的值解析为整数
            if (sscanf(temp_margins, "%d,%d,%d,%d", &margins_values[0], &margins_values[1],
                       &margins_values[2], &margins_values[3]) == 4) {
                current_config_state.margin_left = margins_values[0];
                current_config_state.margin_top = margins_values[1];
                current_config_state.switch_interval = margins_values[2];
                current_config_state.scale_factor = margins_values[3];
            }
            continue;
        }
    }

    fclose(file);
}

// 写入配置
void write_config(const char *filename, ConfigState* state) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "无法打开配置文件进行写入: %s\n", filename);
        return;
    }

    // 提取子目录名称
    char simplified_dir[256];
    if (strstr(state->image_dir, BASE_IMAGE_PATH) != NULL) {
        // 如果包含基础路径，只保存子文件夹名称
        const char* subdir = state->image_dir + strlen(BASE_IMAGE_PATH) + 1; // +1 跳过反斜杠
        strcpy(simplified_dir, subdir);
    } else {
        strcpy(simplified_dir, state->image_dir);
    }

    fprintf(file, "# 图片轮播配置\n");
    fprintf(file, "IMAGE_CAROUSEL_IMAGE_DIR=%s\n", simplified_dir);
    fprintf(file, "IMAGE_CAROUSEL_SWITCH_SPEED_STEP=%d\n", state->switch_speed_step);
    fprintf(file, "IMAGE_CAROUSEL_EDGE_SIZE=%d\n", state->edge_size);
    fprintf(file, "IMAGE_CAROUSEL_SHOW_TRAY_ICON=%d\n", state->show_tray_icon);
    fprintf(file, "IMAGE_CAROUSEL_DISPLAY_DURATION=%d\n", state->display_duration);
    fprintf(file, "IMAGE_CAROUSEL_ZOOM_STEP=%d\n", state->zoom_step);
    fprintf(file, "IMAGE_CAROUSEL_MIN_SCALE_FACTOR=%d\n", state->min_scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_MAX_SCALE_FACTOR=%d\n", state->max_scale_factor);
    fprintf(file, "IMAGE_CAROUSEL_ENABLE_DRAGGING=%d\n", state->enable_dragging);

    // 写入轮播区域设置
    fprintf(file, "\n# 轮播区域设置（左边距,上边距,切换间隔,缩放因子）\n");
    fprintf(file, "IMAGE_CAROUSEL_MARGINS=%d,%d,%d,%d\n",
            state->margin_left,
            state->margin_top,
            state->switch_interval,
            state->scale_factor);

    fclose(file);
}

// 保存配置状态
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

    memcpy(dst, src, surface->w * surface->h * sizeof(Uint32));

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
    context->cache_size = context->image_count;
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
                    SDL_FreeSurface(converted);
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

    if (old_state->scale_factor != new_state->scale_factor ||
        strcmp(old_state->image_dir, new_state->image_dir) != 0 ||
        old_state->margin_left != new_state->margin_left ||
        old_state->margin_top != new_state->margin_top ||
        old_state->enable_dragging != new_state->enable_dragging ||
        old_state->zoom_step != new_state->zoom_step ||
        old_state->min_scale_factor != new_state->min_scale_factor ||
        old_state->max_scale_factor != new_state->max_scale_factor ||
        old_state->switch_interval != new_state->switch_interval) {
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
                HDC hdcMemory = CreateCompatibleDC(hdcScreen);
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
                    DeleteDC(hdcMemory);
                } else {
                    SDL_FreeSurface(converted);
                }
            }
        }
    }
}

// 更新窗口上下文
void update_window_context(WindowContext* context, const char* dir, int scale_factor, int preserve_index, int current_index) {
    if (!context) return;

    int saved_index = preserve_index ? current_index : 0;
    Uint32 saved_switch_time = preserve_index ? context->last_switch_time : SDL_GetTicks();

    clear_image_cache(context);
    if (context->image_files) {
        for (int i = 0; i < context->image_count; i++) {
            free(context->image_files[i]);
        }
        free(context->image_files);
        context->image_files = NULL;
    }

    context->image_count = get_png_files(dir, &context->image_files);
    context->current_index = saved_index % (context->image_count > 0 ? context->image_count : 1);

    init_image_cache(context);

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

    context->last_switch_time = saved_switch_time;
}

// 切换拖动功能
void toggle_dragging(WindowContext* context, HDC hdcScreen, HDC hdcMemory) {
    if (current_config_state.enable_dragging) {
        current_config_state.enable_dragging = 0;
        save_config_state(config_path, &current_config_state);

        LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
        SetWindowLongPtr(context->hwnd, GWL_EXSTYLE, exStyle);

        BLENDFUNCTION blend = {0};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(context->hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);

        context->is_dragging = 0;

        printf("拖动功能已禁用\n");
    }
}

// 切换到下一个目录
void switch_to_next_directory() {
    if (image_dir_count == 0) return;

    current_dir_index = (current_dir_index + 1) % image_dir_count;
    strncpy(current_config_state.image_dir, image_dirs[current_dir_index], sizeof(current_config_state.image_dir) - 1);
    current_config_state.image_dir[sizeof(current_config_state.image_dir) - 1] = '\0';
    save_config_state(config_path, &current_config_state);

    update_window_context(&main_context, current_config_state.image_dir, current_config_state.scale_factor, 0, 0);
    if (main_context.image_count > 0) {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMemory = CreateCompatibleDC(hdcScreen);
        process_and_display_image(main_context.image_files[main_context.current_index], 
                                  &main_context, hdcScreen, hdcMemory);
        preload_next_image(&main_context);
        ReleaseDC(NULL, hdcScreen);
        DeleteDC(hdcMemory);
    }

    printf("切换到目录: %s\n", current_config_state.image_dir);
}

// 切换到上一个目录
void switch_to_previous_directory() {
    if (image_dir_count == 0) return;

    current_dir_index = (current_dir_index - 1 + image_dir_count) % image_dir_count;
    strncpy(current_config_state.image_dir, image_dirs[current_dir_index], sizeof(current_config_state.image_dir) - 1);
    current_config_state.image_dir[sizeof(current_config_state.image_dir) - 1] = '\0';
    save_config_state(config_path, &current_config_state);

    update_window_context(&main_context, current_config_state.image_dir, current_config_state.scale_factor, 0, 0);
    if (main_context.image_count > 0) {
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMemory = CreateCompatibleDC(hdcScreen);
        process_and_display_image(main_context.image_files[main_context.current_index], 
                                  &main_context, hdcScreen, hdcMemory);
        preload_next_image(&main_context);
        ReleaseDC(NULL, hdcScreen);
        DeleteDC(hdcMemory);
    }

    printf("切换到目录: %s\n", current_config_state.image_dir);
}

// 增加切换速度
void increase_switch_speed() {
    current_config_state.switch_interval -= current_config_state.switch_speed_step;
    if (current_config_state.switch_interval < 1) {
        current_config_state.switch_interval = 1;
    }
    save_config_state(config_path, &current_config_state);
    printf("切换速度已增加，当前切换间隔: %d 毫秒\n", current_config_state.switch_interval);
}

// 减少切换速度
void decrease_switch_speed() {
    current_config_state.switch_interval += current_config_state.switch_speed_step;
    save_config_state(config_path, &current_config_state);
    printf("切换速度已减少，当前切换间隔: %d 毫秒\n", current_config_state.switch_interval);
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

    // 获取所有子目录
    image_dir_count = get_subdirectories(BASE_IMAGE_PATH, &image_dirs);
    if (image_dir_count == 0) {
        fprintf(stderr, "在 %s 中找不到子目录\n", BASE_IMAGE_PATH);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 设置当前目录索引
    current_dir_index = 0;
    for (int i = 0; i < image_dir_count; i++) {
        if (strcmp(current_config_state.image_dir, image_dirs[i]) == 0) {
            current_dir_index = i;
            break;
        }
    }
    // 加载图片列表并初始化缓存
    main_context.image_count = get_png_files(current_config_state.image_dir, &main_context.image_files);
    if (main_context.image_count == 0) {
        fprintf(stderr, "在 %s 中找不到PNG文件\n", current_config_state.image_dir);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    init_image_cache(&main_context);
    main_context.current_index = 0;

    // 设置窗口尺寸
    SDL_Surface *image = IMG_Load(main_context.image_files[main_context.current_index]);
    if (image) {
        main_context.imgWidth = (image->w * current_config_state.scale_factor) / 100;
        main_context.imgHeight = (image->h * current_config_state.scale_factor) / 100;
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
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_RIGHT) {
                    switch_to_next_directory();
                }
                else if (e.key.keysym.sym == SDLK_LEFT) {
                    switch_to_previous_directory();
                }
                else if (e.key.keysym.sym == SDLK_UP) {
                    increase_switch_speed();
                }
                else if (e.key.keysym.sym == SDLK_DOWN) {
                    decrease_switch_speed();
                }
            }
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
                        SetForegroundWindow(main_context.hwnd);
                    }
                    else if (e.button.button == SDL_BUTTON_RIGHT) {
                        toggle_dragging(&main_context, hdcScreen, hdcMemory);
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        main_context.is_dragging = 0;
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
                        
                        current_config_state.margin_left = new_x;
                        current_config_state.margin_top = new_y;
                        save_config_state(config_path, &current_config_state);
                    }
                }
                else if (e.type == SDL_MOUSEWHEEL) {
                    POINT pt;
                    GetCursorPos(&pt);
                    RECT rect;
                    GetWindowRect(main_context.hwnd, &rect);
                    if (pt.x >= rect.left && pt.x <= rect.right &&
                        pt.y >= rect.top && pt.y <= rect.bottom) {
                        
                        if (e.wheel.y > 0) {
                            int steps = e.wheel.y / 1;
                            for(int i = 0; i < steps; i++) {
                                current_config_state.scale_factor += current_config_state.zoom_step;
                                if (current_config_state.scale_factor > current_config_state.max_scale_factor) {
                                    current_config_state.scale_factor = current_config_state.max_scale_factor;
                                }
                            }
                        }
                        else if (e.wheel.y < 0) {
                            int steps = (-e.wheel.y) / 1;
                            for(int i = 0; i < steps; i++) {
                                current_config_state.scale_factor -= current_config_state.zoom_step;
                                if (current_config_state.scale_factor < current_config_state.min_scale_factor) {
                                    current_config_state.scale_factor = current_config_state.min_scale_factor;
                                }
                            }

                            update_window_context(&main_context, current_config_state.image_dir, 
                                               current_config_state.scale_factor, 1, main_context.current_index);
                            if (main_context.image_count > 0) {
                                process_and_display_image(main_context.image_files[main_context.current_index], 
                                                          &main_context, hdcScreen, hdcMemory);
                                preload_next_image(&main_context);
                            }

                            save_config_state(config_path, &current_config_state);
                        }

                        update_window_context(&main_context, current_config_state.image_dir, 
                                           current_config_state.scale_factor, 1, main_context.current_index);
                        if (main_context.image_count > 0) {
                            process_and_display_image(main_context.image_files[main_context.current_index], 
                                                      &main_context, hdcScreen, hdcMemory);
                            preload_next_image(&main_context);
                        }

                        save_config_state(config_path, &current_config_state);
                    }
                }
            }
        }

        if (current_config_state.enable_dragging) {
            POINT cursor_pos;
            GetCursorPos(&cursor_pos);
            RECT rect;
            GetWindowRect(main_context.hwnd, &rect);
            if (cursor_pos.x >= rect.left && cursor_pos.x <= rect.right &&
                cursor_pos.y >= rect.top && cursor_pos.y <= rect.bottom) {
                SetForegroundWindow(main_context.hwnd);
            }
        }

        Uint32 current_time = SDL_GetTicks();
        Uint32 time_since_last_switch = current_time - main_context.last_switch_time;
        int current_interval = current_config_state.switch_interval;

        if (time_since_last_switch >= current_interval) {
            main_context.last_switch_time = current_time;
            main_context.current_index = (main_context.current_index + 1) % main_context.image_count;
            
            process_and_display_image(main_context.image_files[main_context.current_index], 
                                      &main_context, hdcScreen, hdcMemory);
            
            preload_next_image(&main_context);
            ensure_window_top_most(&main_context);
        }

        time_t current_mod_time = get_file_modification_time(config_path);
        if (current_mod_time != last_mod_time) {
            last_mod_time = current_mod_time;
            ConfigState old_state = current_config_state;
            load_config(config_path);

            if (check_config_changes(&old_state, &current_config_state, &main_context)) {
                update_window_context(&main_context, current_config_state.image_dir, 
                                   current_config_state.scale_factor, 1, main_context.current_index);
                
                if (main_context.image_count > 0) {
                    process_and_display_image(main_context.image_files[main_context.current_index], 
                                              &main_context, hdcScreen, hdcMemory);
                    preload_next_image(&main_context);
                }

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

        if (current_config_state.display_duration > 0 && 
            (current_time - start_time) >= current_config_state.display_duration) {
            quit = 1;
        }

        SDL_Delay(1);
    }

    // 清理资源
    if (current_config_state.enable_dragging) {
        current_config_state.enable_dragging = 0;
        save_config_state(config_path, &current_config_state);

        LONG_PTR exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;
        SetWindowLongPtr(main_context.hwnd, GWL_EXSTYLE, exStyle);

        BLENDFUNCTION blend = {0};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(main_context.hwnd, hdcScreen, NULL, NULL, NULL, NULL, 0, &blend, ULW_ALPHA);
    }

    clear_image_cache(&main_context);
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
    
    SDL_DestroyWindow(main_context.window);
    for (int i = 0; i < main_context.image_count; i++) {
        free(main_context.image_files[i]);
    }
    free(main_context.image_files);

    for (int i = 0; i < image_dir_count; i++) {
        free(image_dirs[i]);
    }
    free(image_dirs);

    IMG_Quit();
    SDL_Quit();

    return 0;
}
