#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

// 常量定义
#define IMAGE_DIR "./cat"       // 图片文件夹目录
#define SWITCH_INTERVAL 150     // 图片切换时间（毫秒）

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

// 将SDL表面转换为Windows位图
HBITMAP SDLSurfaceToWinBitmap(SDL_Surface* surface, HDC hdc) {
    // 创建与设备兼容的位图信息
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = surface->w;
    bmi.bmiHeader.biHeight = -surface->h;  // 负值表示从上到下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // 创建DIB section
    void* bits;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap) return NULL;

    // 复制并转换SDL表面数据到位图
    SDL_LockSurface(surface);
    Uint32* src = (Uint32*)surface->pixels;
    Uint32* dst = (Uint32*)bits;
    
    for (int i = 0; i < surface->w * surface->h; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(src[i], surface->format, &r, &g, &b, &a);
        
        // 预乘 alpha
        r = (r * a) / 255;
        g = (g * a) / 255;
        b = (b * a) / 255;
        
        // BGRA 格式
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    
    SDL_UnlockSurface(surface);
    return hBitmap;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // 初始化 SDL_image
    if (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG == 0) {
        fprintf(stderr, "IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 获取 IMAGE_DIR 目录下的所有 PNG 文件
    char **image_files = NULL;
    int image_count = get_png_files(IMAGE_DIR, &image_files);
    if (image_count == 0) {
        fprintf(stderr, "No PNG files found in %s\n", IMAGE_DIR);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 获取第一张图片的宽高
    SDL_Surface *image = IMG_Load(image_files[0]);
    if (image == NULL) {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int imgWidth = image->w;
    int imgHeight = image->h;

    // 创建窗口
    SDL_Window *window = SDL_CreateWindow("SDL2 Image Display", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        imgWidth, 
        imgHeight, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_FreeSurface(image);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 获取窗口的原始窗口句柄
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo) != 1) {
        fprintf(stderr, "SDL_GetWindowWMInfo failed\n");
        SDL_DestroyWindow(window);
        SDL_FreeSurface(image);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    HWND hwnd = wmInfo.info.win.window;

    // 设置窗口属性
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, 
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST);

    // 设置窗口为置顶
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 获取DC
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

    // 初始化第一帧
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(image, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(image);
    
    if (converted) {
        HBITMAP hBitmap = SDLSurfaceToWinBitmap(converted, hdcMemory);
        if (hBitmap) {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);
            
            BLENDFUNCTION blend = {0};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat = AC_SRC_ALPHA;

            POINT ptSrc = {0, 0};
            SIZE sizeWnd = {converted->w, converted->h};
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

        // 每隔SWITCH_INTERVAL切换一次图片
        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_time >= SWITCH_INTERVAL) {
            last_time = current_time;
            current_image_index = (current_image_index + 1) % image_count;

            // 加载下一张图片
            SDL_Surface *new_image = IMG_Load(image_files[current_image_index]);
            if (new_image != NULL) {
                SDL_Surface *converted = SDL_ConvertSurfaceFormat(new_image, 
                    SDL_PIXELFORMAT_RGBA32, 0);
                SDL_FreeSurface(new_image);

                if (converted != NULL) {
                    HBITMAP hBitmap = SDLSurfaceToWinBitmap(converted, hdcMemory);
                    if (hBitmap) {
                        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemory, hBitmap);

                        BLENDFUNCTION blend = {0};
                        blend.BlendOp = AC_SRC_OVER;
                        blend.SourceConstantAlpha = 255;
                        blend.AlphaFormat = AC_SRC_ALPHA;

                        POINT ptSrc = {0, 0};
                        SIZE sizeWnd = {converted->w, converted->h};
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

        SDL_Delay(10);
    }

    // 清理资源
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DeleteDC(hdcMemory);
    ReleaseDC(NULL, hdcScreen);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    // 释放图片文件名数组
    for (int i = 0; i < image_count; i++) {
        free(image_files[i]);
    }
    free(image_files);

    return 0;
}
