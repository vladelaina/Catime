#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_rotozoom.h>  // 改用这个头文件，它包含在SDL2_gfx中
#include <windows.h>

// 创建模糊效果的函数
SDL_Surface* createBlurredSurface(SDL_Surface* original, int blurRadius) {
    SDL_Surface* blurred = SDL_CreateRGBSurface(
        0, original->w, original->h,
        32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
    );
    
    // 使用SDL2_gfx的zoomSurface创建轻微模糊效果
    SDL_Surface* temp = zoomSurface(original, 0.5, 0.5, SMOOTHING_ON);
    SDL_Surface* temp2 = zoomSurface(temp, 2.0, 2.0, SMOOTHING_ON);
    
    // 复制到最终表面并调整透明度
    SDL_SetSurfaceAlphaMod(temp2, 192); // 设置透明度
    SDL_BlitSurface(temp2, NULL, blurred, NULL);
    
    // 清理临时表面
    SDL_FreeSurface(temp);
    SDL_FreeSurface(temp2);
    
    return blurred;
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
    
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        600,
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 加载原始图片
    SDL_Surface* originalSurface = IMG_Load("Background.png");
    if (!originalSurface) {
        printf("Failed to load image: %s\n", IMG_GetError());
        return 1;
    }

    // 创建模糊背景
    SDL_Surface* blurredSurface = createBlurredSurface(originalSurface, 5);
    
    // 创建纹理
    SDL_Texture* originalTexture = SDL_CreateTextureFromSurface(renderer, originalSurface);
    SDL_Texture* blurredTexture = SDL_CreateTextureFromSurface(renderer, blurredSurface);
    
    // 设置混合模式
    SDL_SetTextureBlendMode(originalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(blurredTexture, SDL_BLENDMODE_BLEND);

    // 清理surface
    SDL_FreeSurface(originalSurface);
    SDL_FreeSurface(blurredSurface);

    // 设置窗口透明
    SDL_SetWindowHitTest(window, NULL, NULL);

    // 计算居中显示的主图片尺寸
    int centerWidth = 400;
    int centerHeight = 400;
    int centerX = (800 - centerWidth) / 2;
    int centerY = (600 - centerHeight) / 2;

    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // 渲染放大的模糊背景
        SDL_Rect bgRect = {-100, -100, 1000, 800};
        SDL_RenderCopy(renderer, blurredTexture, NULL, &bgRect);
        
        // 渲染中央的清晰图片
        SDL_Rect centerRect = {centerX, centerY, centerWidth, centerHeight};
        SDL_RenderCopy(renderer, originalTexture, NULL, &centerRect);
        
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(originalTexture);
    SDL_DestroyTexture(blurredTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
