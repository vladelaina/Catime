#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL2_rotozoom.h>
#include <windows.h>
#include <math.h>

// ============== 配置区域：可根据实际需求进行调整 ============== //
#define WINDOW_WIDTH     600    // 窗口宽度
#define WINDOW_HEIGHT    400    // 窗口高度

// 下面两个值越大，背景就越模糊
#define BLUR_RADIUS         50     // 高斯模糊半径（原先5，可增大）
#define BLUR_ITERATIONS     5      // 高斯模糊迭代次数（原先2，可增大)
// ============================================================ //

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

    // ========== 水平模糊 ==========
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; i++) {
                int px = x + i;
                if (px < 0)         px = 0;
                if (px >= width)    px = width - 1;

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
                                                (Uint8)r, 
                                                (Uint8)g, 
                                                (Uint8)b, 
                                                (Uint8)a);
        }
    }

    // 再次拷贝到 temp，进行垂直方向模糊
    memcpy(temp, pixels, size * sizeof(Uint32));

    // ========== 垂直模糊 ==========
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            float r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; i++) {
                int py = y + i;
                if (py < 0)         py = 0;
                if (py >= height)   py = height - 1;

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
                                                (Uint8)r, 
                                                (Uint8)g, 
                                                (Uint8)b, 
                                                (Uint8)a);
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
    // 在 SDL 初始化之前，设置渲染器的缩放质量提示
    // "2" 表示启用各向异性过滤，提供更好的缩放质量
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    // 初始化 PNG 支持
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // 创建窗口，初始时隐藏
    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN
    );
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 创建渲染器（使用硬件加速 + 垂直同步）
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 加载背景图片（比如分辨率 3980×2488）
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

    // 计算缩放因子以覆盖窗口
    float scaleX = (float)WINDOW_WIDTH  / bgWidth;
    float scaleY = (float)WINDOW_HEIGHT / bgHeight;
    float scaleBg = (scaleX > scaleY) ? scaleX : scaleY;

    // 使用 SDL2_gfx 进行抗锯齿缩放 (rotozoomSurface)
    SDL_Surface* scaledBackground = rotozoomSurface(
        backgroundSurface,  // 原始表面
        0,                  // 旋转角度(不旋转)
        scaleBg,            // 缩放因子
        SMOOTHING_ON        // 开启双线性插值，减少锯齿
    );
    if (!scaledBackground) {
        printf("Failed to scale background image: %s\n", SDL_GetError());
        SDL_FreeSurface(backgroundSurface);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // 这里对背景做高斯模糊（次数越多、半径越大越模糊）
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

    // 加载前景图片（替换为实际前景图片路径）
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

    // 计算缩放因子，使前景占窗口的 66% 大小
    float targetWidth  = WINDOW_WIDTH  * 0.66f;
    float targetHeight = WINDOW_HEIGHT * 0.66f;
    float scaleX_fg = targetWidth  / foregroundSurface->w;
    float scaleY_fg = targetHeight / foregroundSurface->h;
    float fgScale = (scaleX_fg < scaleY_fg) ? scaleX_fg : scaleY_fg;  // 取较小者以保持纵横比

    // 使用 rotozoomSurface 进行抗锯齿缩放前景
    SDL_Surface* scaledForeground = rotozoomSurface(
        foregroundSurface,
        0,
        fgScale,
        SMOOTHING_ON
    );
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

    // 计算前景缩放后尺寸
    int scaledFgWidth  = scaledForeground->w;
    int scaledFgHeight = scaledForeground->h;

    // 前景居中坐标
    int fgCenterX = (WINDOW_WIDTH  - scaledFgWidth)  / 2;
    int fgCenterY = (WINDOW_HEIGHT - scaledFgHeight) / 2;

    // 释放不再需要的表面
    SDL_FreeSurface(scaledForeground);
    SDL_FreeSurface(foregroundSurface);
    SDL_FreeSurface(scaledBackground);
    SDL_FreeSurface(backgroundSurface);

    // 在主循环开始前，立即 render 一次以显示背景和前景
    SDL_RenderClear(renderer); // 清理渲染器

    // 渲染背景
    SDL_Rect bgRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    if (SDL_RenderCopy(renderer, backgroundTexture, NULL, &bgRect) != 0) {
        printf("SDL_RenderCopy Error (background): %s\n", SDL_GetError());
    }

    // 渲染前景
    SDL_Rect fgRect = { fgCenterX, fgCenterY, scaledFgWidth, scaledFgHeight };
    if (SDL_RenderCopy(renderer, foregroundTexture, NULL, &fgRect) != 0) {
        printf("SDL_RenderCopy Error (foreground): %s\n", SDL_GetError());
    }

    SDL_RenderPresent(renderer); // 呈现更新

    // 显示窗口，此时已经渲染好内容
    SDL_ShowWindow(window);

    // =============== 主循环 ===============
    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
               (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        // 清理渲染器（如果需要动态更新，保留渲染逻辑）
        // 如果图像不需要动态更新，可以 skip 这部分

        // =============== 渲染背景 ===============
        if (SDL_RenderCopy(renderer, backgroundTexture, NULL, &bgRect) != 0) {
            printf("SDL_RenderCopy Error (background): %s\n", SDL_GetError());
        }

        // =============== 渲染前景(居中) ===============
        if (SDL_RenderCopy(renderer, foregroundTexture, NULL, &fgRect) != 0) {
            printf("SDL_RenderCopy Error (foreground): %s\n", SDL_GetError());
        }

        // 呈现
        SDL_RenderPresent(renderer);
    }

    // 释放资源
    SDL_DestroyTexture(foregroundTexture);
    SDL_DestroyTexture(backgroundTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
