#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <windows.h>

#ifdef __cplusplus
extern "C"
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16); // 16x MSAA
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    
    // 设置纹理的缩放质量为最佳
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    // 创建窗口，设置为无边框，添加OpenGL支持
    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, // 窗口宽度
        600, // 窗口高度
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL
    );

    // 创建渲染器，启用透明支持和垂直同步
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    // 启用混合模式以支持透明度
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 加载背景图片
    SDL_Surface* surface = IMG_Load("Background.png");
    if (!surface) {
        printf("Failed to load image: %s\n", IMG_GetError());
        return 1;
    }

    // 创建纹理并设置混合模式
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surface);

    // 设置窗口透明
    SDL_SetWindowHitTest(window, NULL, NULL);

    // 主循环
    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        // 清除渲染器
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // 渲染背景图片
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        
        // 更新屏幕
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
