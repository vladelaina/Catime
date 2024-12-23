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
#define SWITCH_INTERVAL 50     // 图片切换时间（毫秒）

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

    // 创建窗口，窗口大小根据图片大小调整
    SDL_Window *window = SDL_CreateWindow("SDL2 Image Display", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, imgWidth, imgHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
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

    // 设置为工具窗口
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT);

    // 设置窗口透明并允许鼠标穿透
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY); // 设置透明色键

    // 设置窗口为置顶
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_FreeSurface(image);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建纹理
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, image);
    SDL_FreeSurface(image); // 不再需要表面了
    if (texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 设置托盘图标
    NOTIFYICONDATA nid;
    ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP; // 设置消息回调
    nid.hIcon = (HICON)LoadImage(NULL, "icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE); // 图标路径

    // 修改 nid.szTip 的类型为 wchar_t 数组
    wchar_t szTip[128];  // 定义为 wchar_t 类型
    wcsncpy(szTip, L"My Tray Icon", sizeof(szTip) / sizeof(wchar_t)); // 宽字符字符串
    wcscpy((wchar_t*)nid.szTip, szTip); // 使用宽字符字符串填充

    Shell_NotifyIcon(NIM_ADD, &nid);

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
            if (new_image == NULL) {
                fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
                continue;
            }

            // 创建新纹理
            SDL_Texture *new_texture = SDL_CreateTextureFromSurface(renderer, new_image);
            SDL_FreeSurface(new_image);

            if (new_texture == NULL) {
                fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
                continue;
            }

            SDL_DestroyTexture(texture);  // 销毁旧纹理
            texture = new_texture;        // 更新为新纹理
        }

        // 渲染图片
        SDL_RenderClear(renderer);
        SDL_Rect dstRect = {0, 0, imgWidth, imgHeight}; // 目标矩形，指定显示的图片大小
        SDL_RenderCopy(renderer, texture, NULL, &dstRect); // 使用指定大小渲染
        SDL_RenderPresent(renderer);

        SDL_Delay(10); // 防止占用过多 CPU
    }

    // 清理资源
    Shell_NotifyIcon(NIM_DELETE, &nid); // 删除托盘图标
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
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

