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

// 常量定义
#define SCALE_FACTOR 20        // 图片缩放比例（20 表示 20%）
#define IMAGE_DIR "./cat"      // 图片文件夹目录
#define SWITCH_INTERVAL 150    // 图片切换时间（毫秒）

// 判断文件是否为 PNG 格式
int is_png(const char *filename) {
    const char *ext = strrchr(filename, '.');
    return ext != NULL && strcmp(ext, ".png") == 0;
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
    return count;
}

// 计算两个颜色的距离
float color_distance(Uint8 r1, Uint8 g1, Uint8 b1, Uint8 r2, Uint8 g2, Uint8 b2) {
    float dr = (float)r1 - r2;
    float dg = (float)g1 - g2;
    float db = (float)b1 - b2;
    return sqrt(dr * dr + dg * dg + db * db);
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
            
            // 检测是否是边缘像素（半透明或暗色）
            if ((a > 0 && a < 255) || (a > 0 && r < 50 && g < 50 && b < 50)) {
                Uint8 inner_r = 0, inner_g = 0, inner_b = 0;
                float min_distance = 1000000.0f;
                int found_inner = 0;
                
                // 扩大搜索范围并使用多个方向
                const int directions[8][2] = {
                    {-1, 0}, {1, 0}, {0, -1}, {0, 1},  // 上下左右
                    {-1, -1}, {-1, 1}, {1, -1}, {1, 1} // 对角线
                };
                
                // 在每个方向上搜索
                for (int dir = 0; dir < 8; dir++) {
                    for (int dist = 1; dist <= 4; dist++) {
                        int nx = x + directions[dir][0] * dist;
                        int ny = y + directions[dir][1] * dist;
                        
                        if (nx >= 0 && nx < surface->w && ny >= 0 && ny < surface->h) {
                            Uint8 nr, ng, nb, na;
                            SDL_GetRGBA(src[ny * surface->w + nx], surface->format, &nr, &ng, &nb, &na);
                            
                            // 寻找完全不透明且非黑白的有效颜色
                            if (na == 255 && 
                                !(nr > 240 && ng > 240 && nb > 240) && // 不是白色
                                !(nr < 30 && ng < 30 && nb < 30))      // 不是黑色
                            {
                                float dist = color_distance(r, g, b, nr, ng, nb);
                                if (dist < min_distance) {
                                    min_distance = dist;
                                    inner_r = nr;
                                    inner_g = ng;
                                    inner_b = nb;
                                    found_inner = 1;
                                }
                            }
                        }
                    }
                }
                
                // 如果找到合适的颜色，使用它
                if (found_inner) {
                    // 根据原始alpha值和距离进行颜色混合
                    float alpha = a / 255.0f;
                    float blend = alpha * (1.0f - min_distance / 1000.0f);
                    Uint8 final_r = (Uint8)(inner_r * blend);
                    Uint8 final_g = (Uint8)(inner_g * blend);
                    Uint8 final_b = (Uint8)(inner_b * blend);
                    dst[idx] = SDL_MapRGBA(result->format, final_r, final_g, final_b, a);
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
            // 保持原始颜色值，不进行预乘
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

    // 创建缩放后的表面
    SDL_Surface *scaled = SDL_CreateRGBSurface(0, imgWidth, imgHeight, 32,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    
    if (scaled) {
        SDL_BlitScaled(image, NULL, scaled, NULL);
        SDL_FreeSurface(image);

        // 处理边缘
        SDL_Surface *processed = process_alpha(scaled);
        SDL_FreeSurface(scaled);

        if (processed) {
            // 转换格式
            SDL_Surface *converted = SDL_ConvertSurfaceFormat(processed, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(processed);

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
                    POINT ptDst = {0, 0};
                    RECT rcWindow;
                    GetWindowRect(hwnd, &rcWindow);
                    ptDst.x = rcWindow.left;
                    ptDst.y = rcWindow.top;
                    
                    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, 
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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG == 0) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    char **image_files = NULL;
    int image_count = get_png_files(IMAGE_DIR, &image_files);
    if (image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", IMAGE_DIR);
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

    int imgWidth = (image->w * SCALE_FACTOR) / 100;
    int imgHeight = (image->h * SCALE_FACTOR) / 100;
    SDL_FreeSurface(image);

    SDL_Window *window = SDL_CreateWindow("SDL2 Image Display", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
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
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemory = CreateCompatibleDC(hdcScreen);

    // 设置托盘图标
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

    // 显示第一帧
    process_and_display_image(image_files[0], window, hdcScreen, hdcMemory, imgWidth, imgHeight);

    // 事件循环
    SDL_Event e;
    int quit = 0;
    int current_image_index = 0;
    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }

        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_time >= SWITCH_INTERVAL) {
            last_time = current_time;
            current_image_index = (current_image_index + 1) % image_count;
            process_and_display_image(image_files[current_image_index], 
                                   window, hdcScreen, hdcMemory, 
                                   imgWidth, imgHeight);
        }

        SDL_Delay(10);
    }

    // 清理资源
    Shell_NotifyIcon(NIM_DELETE, &nid);
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
