#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <windows.h>
#include <math.h>

// 定义参数
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SCALE_FACTOR 0.7

// 增加抗锯齿相关参数
#define MSAA_SAMPLES 64         // 增加 MSAA 采样数
#define SUPERSAMPLING_SCALE 2.0 // 超采样比例

// 模糊相关的参数定义
#define BLUR_RADIUS 40
#define BLUR_ITERATIONS 8       // 增加模糊迭代次数
#define BLUR_DOWNSCALE 2       // 减小降采样比例以提高质量
#define BLUR_RENDER_PASSES 6    // 增加渲染叠加次数

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
    // 先创建较小尺寸的表面
    int smallWidth = original->w / BLUR_DOWNSCALE;
    int smallHeight = original->h / BLUR_DOWNSCALE;
    
    SDL_Surface* smallSurface = SDL_CreateRGBSurfaceWithFormat(
        0, smallWidth, smallHeight,
        32, SDL_PIXELFORMAT_RGBA32);
    
    // 缩小图像
    SDL_BlitScaled(original, NULL, smallSurface, NULL);
    
    // 对小图像进行多次模糊
    for (int i = 0; i < BLUR_ITERATIONS; i++) {
        gaussianBlur(smallSurface, BLUR_RADIUS);
    }
    
    // 创建最终尺寸的纹理
    SDL_Texture* finalTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        original->w, original->h
    );
    
    // 创建临时纹理并设置属性
    SDL_Texture* smallTexture = SDL_CreateTextureFromSurface(renderer, smallSurface);
    SDL_SetTextureBlendMode(smallTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(smallTexture, SDL_ScaleModeLinear);
    SDL_SetTextureBlendMode(finalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(finalTexture, SDL_ScaleModeLinear);
    
    // 将小纹理放大到最终尺寸
    SDL_SetRenderTarget(renderer, finalTexture);
    SDL_RenderCopy(renderer, smallTexture, NULL, NULL);
    SDL_SetRenderTarget(renderer, NULL);
    
    // 清理资源
    SDL_DestroyTexture(smallTexture);
    SDL_FreeSurface(smallSurface);
    
    return finalTexture;
}

#ifdef __cplusplus
extern "C"
#endif
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    // 增强 OpenGL 设置
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, MSAA_SAMPLES);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    // 设置额外的 OpenGL 属性
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

    // 创建更大的窗口用于超采样
    int supersampledWidth = (int)(WINDOW_WIDTH * SUPERSAMPLING_SCALE);
    int supersampledHeight = (int)(WINDOW_HEIGHT * SUPERSAMPLING_SCALE);

    SDL_Window* window = SDL_CreateWindow(
        "Transparent Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,    // 使用原始尺寸
        WINDOW_HEIGHT,   // 使用原始尺寸
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
    );

    // 创建超采样渲染目标
    SDL_Texture* renderTarget = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        supersampledWidth,
        supersampledHeight
    );
    SDL_SetTextureScaleMode(renderTarget, SDL_ScaleModeLinear);

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
    int centerWidth = (int)(imgWidth * scale * SUPERSAMPLING_SCALE);
    int centerHeight = (int)(imgHeight * scale * SUPERSAMPLING_SCALE);

    // 创建纹理并设置增强的属性
    SDL_Texture* originalTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Texture* blurTexture = createBlurTexture(renderer, surface);
    
    // 设置增强的纹理属性
    SDL_SetTextureBlendMode(originalTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(originalTexture, SDL_ScaleModeLinear);
    SDL_SetTextureBlendMode(blurTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(blurTexture, SDL_ScaleModeLinear);
    
    SDL_FreeSurface(surface);

    SDL_SetWindowHitTest(window, NULL, NULL);

    // 计算居中位置（考虑超采样）
    int centerX = (int)((supersampledWidth - centerWidth) / 2);
    int centerY = (int)((supersampledHeight - centerHeight) / 2);

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

        // 设置渲染目标为超采样纹理
        SDL_SetRenderTarget(renderer, renderTarget);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        // 渲染模糊背景
        for (int i = 0; i < BLUR_RENDER_PASSES; i++) {
            SDL_Rect bgRect = {
                (int)(-50 * SUPERSAMPLING_SCALE - (i * 10 * SUPERSAMPLING_SCALE)), 
                (int)(-50 * SUPERSAMPLING_SCALE - (i * 10 * SUPERSAMPLING_SCALE)), 
                (int)((WINDOW_WIDTH + 100) * SUPERSAMPLING_SCALE + (i * 20 * SUPERSAMPLING_SCALE)), 
                (int)((WINDOW_HEIGHT + 100) * SUPERSAMPLING_SCALE + (i * 20 * SUPERSAMPLING_SCALE))
            };
            SDL_RenderCopy(renderer, blurTexture, NULL, &bgRect);
        }

        // 渲染清晰的中心图片
        SDL_Rect centerRect = {centerX, centerY, centerWidth, centerHeight};
        SDL_SetTextureAlphaMod(originalTexture, 255);
        SDL_RenderCopy(renderer, originalTexture, NULL, &centerRect);

        // 将超采样纹理缩放渲染到屏幕
        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, renderTarget, NULL, NULL);
        
        SDL_RenderPresent(renderer);
    }

    // 清理资源
    SDL_DestroyTexture(renderTarget);
    SDL_DestroyTexture(originalTexture);
    SDL_DestroyTexture(blurTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
