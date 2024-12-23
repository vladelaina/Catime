#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#include <windows.h>
#include <shellapi.h>

// 修改入口函数为 WinMain
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

    // 加载图片
    SDL_Surface *image = IMG_Load("./cat/1.png");
    if (image == NULL) {
        fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 获取图片的原始宽高
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

    // 渲染图片（使用图片原始的宽高）
    SDL_RenderClear(renderer);
    SDL_Rect dstRect = {0, 0, imgWidth, imgHeight}; // 目标矩形，指定显示的图片大小
    SDL_RenderCopy(renderer, texture, NULL, &dstRect); // 使用指定大小渲染
    SDL_RenderPresent(renderer);

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
    while (!quit) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }
        SDL_Delay(10); // 防止占用过多 CPU
    }

    // 清理资源
    Shell_NotifyIcon(NIM_DELETE, &nid); // 删除托盘图标
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}

