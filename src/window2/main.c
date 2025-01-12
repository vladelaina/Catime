#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <windows.h>

// 定义参数
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SCALE_FACTOR 0.7
#define BLUR_ALPHA 192

// 降采样来实现模糊效果
SDL_Texture* createBlurTexture(SDL_Renderer* renderer, SDL_Surface* original) {
    // 创建小尺寸的表面（降采样）
    int smallWidth = original->w / 4;
    int smallHeight = original->h / 4;
    
    SDL_Surface* smallSurface = SDL_CreateRGBSurfaceWithFormat(
        0, smallWidth, smallHeight,
        32, SDL_PIXELFORMAT_RGBA32);
    
    // 缩放到小尺寸（这会产生初步的模糊效果）
    SDL_BlitScaled(original, NULL, smallSurface, NULL);
    
    // 创建临时纹理
    SDL_Texture* smallTexture = SDL_CreateTextureFromSurface(renderer, smallSurface);
    SDL_SetTextureBlendMode(smallTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(smallTexture, SDL_ScaleModeLinear);
    
    SDL_FreeSurface(smallSurface);
    
    // 创建最终尺寸的纹理
    SDL_Texture* finalTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        original->w, original->h
    );
    
    // 设置混合模式和缩放质量
    SDL_SetTextureBlendMode(finalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(finalTexture, SDL_ScaleModeLinear);
    
    // 将小纹理放大到最终尺寸（这会产生第二次模糊效果）
    SDL_SetRenderTarget(renderer, finalTexture);
    SDL_RenderCopy(renderer, smallTexture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);
    
    SDL_DestroyTexture(smallTexture);
    
    return finalTexture;
}

#ifdef __cplusplus
extern "C"
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
    );

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 加载图片
    SDL_Surface* surface = IMG_Load("Background.png");
    if (!surface) {
        printf("Failed to load image: %s\n", IMG_GetError());
        return 1;
    }

    // 获取图片尺寸
    int imgWidth = surface->w;
    int imgHeight = surface->h;

    // 计算缩放尺寸
    float scale = WINDOW_HEIGHT * SCALE_FACTOR / imgHeight;
    int centerWidth = (int)(imgWidth * scale);
    int centerHeight = (int)(imgHeight * scale);

    // 创建纹理
    SDL_Texture* originalTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Texture* blurTexture = createBlurTexture(renderer, surface);
    
    // 设置纹理属性
    SDL_SetTextureBlendMode(originalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(originalTexture, SDL_ScaleModeLinear);
    
    SDL_FreeSurface(surface);

    SDL_SetWindowHitTest(window, NULL, NULL);

    // 计算居中位置
    int centerX = (WINDOW_WIDTH - centerWidth) / 2;
    int centerY = (WINDOW_HEIGHT - centerHeight) / 2;

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

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // 渲染模糊背景（多次渲染叠加以增强模糊效果）
        SDL_Rect bgRect = {-50, -50, WINDOW_WIDTH + 100, WINDOW_HEIGHT + 100};
        
        // 第一次渲染 - 基础模糊层
        SDL_SetTextureAlphaMod(blurTexture, BLUR_ALPHA);
        SDL_RenderCopy(renderer, blurTexture, NULL, &bgRect);
        
        // 第二次渲染 - 增强模糊效果
        SDL_SetTextureAlphaMod(blurTexture, BLUR_ALPHA / 2);
        SDL_Rect bgRect2 = {-60, -60, WINDOW_WIDTH + 120, WINDOW_HEIGHT + 120};
        SDL_RenderCopy(renderer, blurTexture, NULL, &bgRect2);

        // 渲染清晰的中心图片
        SDL_Rect centerRect = {centerX, centerY, centerWidth, centerHeight};
        SDL_SetTextureAlphaMod(originalTexture, 255);
        SDL_RenderCopy(renderer, originalTexture, NULL, &centerRect);
        
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyTexture(originalTexture);
    SDL_DestroyTexture(blurTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
