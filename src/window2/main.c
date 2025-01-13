#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL2_rotozoom.h>
#include <windows.h>
#include <math.h>

// 定义参数
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define BLUR_RADIUS 5
#define BLUR_ITERATIONS 2

// 高斯模糊函数
void gaussianBlur(SDL_Surface* surface, int radius) {
    if (radius < 1) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    int width = surface->w;
    int height = surface->h;
    int size = width * height;

    // 创建临时缓冲区
    Uint32* temp = (Uint32*)malloc(size * sizeof(Uint32));
    if (!temp) {
        printf("Failed to allocate memory for Gaussian blur.\n");
        return;
    }

    // 复制原始像素到临时缓冲区
    memcpy(temp, pixels, size * sizeof(Uint32));

    // 高斯核计算
    float sigma = radius / 2.0f;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc(kernelSize * sizeof(float));
    if (!kernel) {
        printf("Failed to allocate memory for Gaussian kernel.\n");
        free(temp);
        return;
    }

    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        kernel[i + radius] = expf(-(i * i) / (2.0f * sigma * sigma));
        sum += kernel[i + radius];
    }

    // 归一化核
    for (int i = 0; i < kernelSize; i++) {
        kernel[i] /= sum;
    }

    // 水平方向模糊
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; i++) {
                int px = x + i;
                if (px < 0) px = 0;
                if (px >= width) px = width - 1;

                Uint32 pixel = temp[y * width + px];
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(pixel, surface->format, &pr, &pg, &pb, &pa);

                float k = kernel[i + radius];
                r += pr * k;
                g += pg * k;
                b += pb * k;
                a += pa * k;
            }

            pixels[y * width + x] = SDL_MapRGBA(surface->format, 
                (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        }
    }

    // 垂直方向模糊
    memcpy(temp, pixels, size * sizeof(Uint32)); // 更新临时缓冲区

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            float r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; i++) {
                int py = y + i;
                if (py < 0) py = 0;
                if (py >= height) py = height - 1;

                Uint32 pixel = temp[py * width + x];
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(pixel, surface->format, &pr, &pg, &pb, &pa);

                float k = kernel[i + radius];
                r += pr * k;
                g += pg * k;
                b += pb * k;
                a += pa * k;
            }

            pixels[y * width + x] = SDL_MapRGBA(surface->format, 
                (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        }
    }

    free(kernel);
    free(temp);
}

#ifdef __cplusplus
extern "C"
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 创建窗口
    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建渲染器
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载背景图片
    SDL_Surface* backgroundSurface = IMG_Load("Background.png");
    if (!backgroundSurface) {
        printf("Failed to load background image: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 获取原始背景图尺寸
    int bgWidth = backgroundSurface->w;
    int bgHeight = backgroundSurface->h;

    // 计算缩放因子以覆盖整个窗口
    float scaleX = (float)WINDOW_WIDTH / bgWidth;
    float scaleY = (float)WINDOW_HEIGHT / bgHeight;
    float scaleBg = (scaleX > scaleY) ? scaleX : scaleY;

    // 使用 SDL2_gfx 进行抗锯齿缩放背景
    SDL_Surface* scaledBackground = rotozoomSurface(backgroundSurface, 0, scaleBg, SMOOTHING_ON);
    if (!scaledBackground) {
        printf("Failed to scale background image: %s\n", SDL_GetError());
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 应用高斯模糊到背景
    for (int i = 0; i < BLUR_ITERATIONS; i++) {
        gaussianBlur(scaledBackground, BLUR_RADIUS);
    }

    // 创建背景纹理
    SDL_Texture* backgroundTexture = SDL_CreateTextureFromSurface(renderer, scaledBackground);
    if (!backgroundTexture) {
        printf("SDL_CreateTextureFromSurface Error (background): %s\n", SDL_GetError());
        SDL_FreeSurface(scaledBackground);
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载前景图片
    SDL_Surface* foregroundSurface = IMG_Load("Foreground.png");
    if (!foregroundSurface) {
        printf("Failed to load foreground image: %s\n", IMG_GetError());
        SDL_DestroyTexture(backgroundTexture);
        SDL_FreeSurface(scaledBackground);
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 计算缩放因子为0.5 (使前景比背景小)
    float fgScale = 0.5f; // 可根据需要调整

    // 使用 SDL2_gfx 进行抗锯齿缩放前景
    SDL_Surface* scaledForeground = rotozoomSurface(foregroundSurface, 0, fgScale, SMOOTHING_ON);
    if (!scaledForeground) {
        printf("Failed to scale foreground image: %s\n", SDL_GetError());
        SDL_FreeSurface(foregroundSurface);
        SDL_DestroyTexture(backgroundTexture);
        SDL_FreeSurface(scaledBackground);
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建前景纹理
    SDL_Texture* foregroundTexture = SDL_CreateTextureFromSurface(renderer, scaledForeground);
    if (!foregroundTexture) {
        printf("SDL_CreateTextureFromSurface Error (foreground): %s\n", SDL_GetError());
        SDL_FreeSurface(scaledForeground);
        SDL_FreeSurface(foregroundSurface);
        SDL_DestroyTexture(backgroundTexture);
        SDL_FreeSurface(scaledBackground);
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 获取前景缩放后的尺寸
    int scaledFgWidth = scaledForeground->w;
    int scaledFgHeight = scaledForeground->h;

    // 计算前景的居中位置
    int fgCenterX = (WINDOW_WIDTH - scaledFgWidth) / 2;
    int fgCenterY = (WINDOW_HEIGHT - scaledFgHeight) / 2;

    // 释放不再需要的表面
    SDL_FreeSurface(scaledForeground);
    SDL_FreeSurface(foregroundSurface);
    SDL_FreeSurface(scaledBackground);
    SDL_FreeSurface(backgroundSurface);

    // 主循环
    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        // 清理渲染器
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // 设置背景颜色为黑色
        SDL_RenderClear(renderer);

        // 渲染背景
        SDL_Rect bgRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
        SDL_RenderCopy(renderer, backgroundTexture, NULL, &bgRect);

        // 渲染前景
        SDL_Rect fgRect = { fgCenterX, fgCenterY, scaledFgWidth, scaledFgHeight };
        SDL_RenderCopy(renderer, foregroundTexture, NULL, &fgRect);

        // 显示渲染内容
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyTexture(foregroundTexture);
    SDL_DestroyTexture(backgroundTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
