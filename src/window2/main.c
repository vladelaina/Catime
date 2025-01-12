#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <windows.h>
#include <math.h>

// 定义参数
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SCALE_FACTOR 0.7
#define BLUR_ALPHA 192

// 高斯模糊函数
void gaussianBlur(SDL_Surface* surface, int radius) {
    Uint32* pixels = (Uint32*)surface->pixels;
    int width = surface->w;
    int height = surface->h;
    
    // 创建临时缓冲区
    Uint32* temp = (Uint32*)malloc(width * height * sizeof(Uint32));
    
    // 计算高斯核
    float sigma = radius / 2.0f;
    float* kernel = (float*)malloc((radius * 2 + 1) * sizeof(float));
    float sum = 0.0f;
    
    for (int i = -radius; i <= radius; i++) {
        kernel[i + radius] = exp(-(i * i) / (2 * sigma * sigma));
        sum += kernel[i + radius];
    }
    
    // 归一化核
    for (int i = 0; i <= radius * 2; i++) {
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
                
                Uint32 pixel = pixels[y * width + px];
                Uint8 pr, pg, pb, pa;
                SDL_GetRGBA(pixel, surface->format, &pr, &pg, &pb, &pa);
                
                float k = kernel[i + radius];
                r += pr * k;
                g += pg * k;
                b += pb * k;
                a += pa * k;
            }
            
            temp[y * width + x] = SDL_MapRGBA(surface->format, 
                (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
        }
    }
    
    // 垂直方向模糊
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

// 创建模糊纹理
SDL_Texture* createBlurTexture(SDL_Renderer* renderer, SDL_Surface* original) {
    // 创建一个副本用于模糊处理
    SDL_Surface* blurSurface = SDL_CreateRGBSurfaceWithFormat(
        0, original->w, original->h,
        32, SDL_PIXELFORMAT_RGBA32);
    
    SDL_BlitSurface(original, NULL, blurSurface, NULL);
    
    // 应用高斯模糊
    gaussianBlur(blurSurface, 200); // 半径可以调整，越大越模糊
    
    // 创建纹理
    SDL_Texture* blurTexture = SDL_CreateTextureFromSurface(renderer, blurSurface);
    SDL_SetTextureBlendMode(blurTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(blurTexture, SDL_ScaleModeLinear);
    
    SDL_FreeSurface(blurSurface);
    
    return blurTexture;
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
