#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <windows.h>

// 定义模糊和渲染相关的参数
#define BLUR_RADIUS 3         // 高斯模糊半径
#define BLUR_PASSES 3         // 模糊处理次数
#define BLUR_ALPHA 192        // 模糊层的透明度 (0-255)
#define MSAA_LEVEL 16         // 抗锯齿级别
#define WINDOW_WIDTH 800      // 窗口宽度
#define WINDOW_HEIGHT 600     // 窗口高度
#define SCALE_FACTOR 0.7      // 中心图片相对于窗口高度的缩放比例

// 创建高斯模糊纹理
SDL_Texture* createBlurredTexture(SDL_Renderer* renderer, SDL_Surface* original) {
    int width = original->w;
    int height = original->h;
    
    // 创建临时表面进行模糊处理
    SDL_Surface* blurredSurface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height,
        32, SDL_PIXELFORMAT_RGBA32);
    
    // 复制原始表面到模糊表面
    SDL_BlitSurface(original, NULL, blurredSurface, NULL);
    
    // 应用多次模糊以获得更好的效果
    for (int pass = 0; pass < BLUR_PASSES; pass++) {
        for (int y = BLUR_RADIUS; y < height - BLUR_RADIUS; y++) {
            for (int x = BLUR_RADIUS; x < width - BLUR_RADIUS; x++) {
                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                
                // 计算周围像素的平均值
                for (int dy = -BLUR_RADIUS; dy <= BLUR_RADIUS; dy++) {
                    for (int dx = -BLUR_RADIUS; dx <= BLUR_RADIUS; dx++) {
                        SDL_Color pixel;
                        Uint32* pixels = (Uint32*)blurredSurface->pixels;
                        SDL_GetRGBA(pixels[(y + dy) * width + (x + dx)],
                                  blurredSurface->format,
                                  &pixel.r, &pixel.g, &pixel.b, &pixel.a);
                        r += pixel.r;
                        g += pixel.g;
                        b += pixel.b;
                        a += pixel.a;
                        count++;
                    }
                }
                
                // 设置模糊后的像素值
                Uint32* pixels = (Uint32*)blurredSurface->pixels;
                pixels[y * width + x] = SDL_MapRGBA(blurredSurface->format,
                                                  r / count,
                                                  g / count,
                                                  b / count,
                                                  a / count);
            }
        }
    }
    
    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, blurredSurface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    
    // 清理临时表面
    SDL_FreeSurface(blurredSurface);
    
    return texture;
}

#ifdef __cplusplus
extern "C"
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // 设置OpenGL属性，启用抗锯齿
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, MSAA_LEVEL);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    
    // 设置最高质量的纹理缩放
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
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    // 启用混合模式
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 加载原始图片
    SDL_Surface* surface = IMG_Load("Background.png");
    if (!surface) {
        printf("Failed to load image: %s\n", IMG_GetError());
        return 1;
    }

    // 获取图片原始尺寸
    int imgWidth = surface->w;
    int imgHeight = surface->h;

    // 计算保持宽高比的缩放尺寸
    float scale = WINDOW_HEIGHT * SCALE_FACTOR / imgHeight;
    int centerWidth = (int)(imgWidth * scale);
    int centerHeight = (int)(imgHeight * scale);

    // 创建原始纹理和模糊纹理
    SDL_Texture* originalTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Texture* blurredTexture = createBlurredTexture(renderer, surface);
    
    // 设置纹理属性
    SDL_SetTextureBlendMode(originalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(originalTexture, SDL_ScaleModeLinear); // 启用线性过滤
    
    SDL_FreeSurface(surface);

    // 设置窗口透明
    SDL_SetWindowHitTest(window, NULL, NULL);

    // 计算居中位置
    int centerX = (WINDOW_WIDTH - centerWidth) / 2;
    int centerY = (WINDOW_HEIGHT - centerHeight) / 2;

    // 主循环
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

        // 清除渲染器
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // 渲染模糊背景（稍微放大以覆盖整个窗口）
        SDL_Rect bgRect = {
            -20, -20,
            WINDOW_WIDTH + 40,
            WINDOW_HEIGHT + 40
        };
        SDL_SetTextureAlphaMod(blurredTexture, BLUR_ALPHA);
        SDL_RenderCopy(renderer, blurredTexture, NULL, &bgRect);

        // 渲染清晰的中心图片
        SDL_Rect centerRect = {centerX, centerY, centerWidth, centerHeight};
        SDL_SetTextureAlphaMod(originalTexture, 255);
        SDL_RenderCopy(renderer, originalTexture, NULL, &centerRect);
        
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyTexture(originalTexture);
    SDL_DestroyTexture(blurredTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
