#include <SDL2/SDL.h>
#include <windows.h>  // Windows API 用于获取屏幕尺寸
#include <stdio.h>    // 添加这个头文件

int main(int argc, char *argv[]) {
    // 初始化 SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // 创建 SDL 窗口
    SDL_Window *window = SDL_CreateWindow("SDL2 Window",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          800, 600, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 加载图片
    SDL_Surface *imageSurface = SDL_LoadBMP("./asset/images/moving/mona/mona0.bmp");
    if (!imageSurface) {
        printf("SDL_LoadBMP failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 转换为纹理
    SDL_Texture *imageTexture = SDL_CreateTextureFromSurface(renderer, imageSurface);
    SDL_FreeSurface(imageSurface);  // 不再需要表面（surface）

    if (!imageTexture) {
        printf("SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 获取屏幕分辨率（Windows API）
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);  // 获取屏幕宽度
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // 获取屏幕高度

    // 获取图片的宽高
    int imageWidth, imageHeight;
    SDL_QueryTexture(imageTexture, NULL, NULL, &imageWidth, &imageHeight);

    // 计算图像中央位置
    int xPos = (screenWidth - imageWidth) / 2;
    int yPos = (screenHeight - imageHeight) / 2;

    // 主循环，显示图像
    SDL_Event event;
    int running = 1;
    while (running) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        // 清除渲染器
        SDL_RenderClear(renderer);

        // 在屏幕中央渲染图像
        SDL_Rect dstRect = { xPos, yPos, imageWidth, imageHeight };
        SDL_RenderCopy(renderer, imageTexture, NULL, &dstRect);

        // 更新渲染器
        SDL_RenderPresent(renderer);
    }

    // 清理
    SDL_DestroyTexture(imageTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

