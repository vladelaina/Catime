#include <SDL2/SDL_image.h>
#include <string>
#include <windows.h>
#include "resource.h"

// 窗口属性
const int WINDOW_WIDTH = 625;
const int WINDOW_HEIGHT = 450;

// 图标属性
const int ICON_SIZE = 24;
const int ICON_MARGIN = 10;
const int MINIMIZE_X = WINDOW_WIDTH - (2 * ICON_SIZE + ICON_MARGIN + 10);
const int CLOSE_X = WINDOW_WIDTH - (ICON_SIZE + 10);
const int ICONS_Y = 10;

// 图标结构体
struct Icon {
    SDL_Texture* texture;
    SDL_Rect bounds;
    bool isHovered;  // 新增悬停状态
};

// 全局变量
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
Icon minimizeIcon;
Icon closeIcon;
SDL_Texture* backgroundTexture = nullptr;  // 新增背景纹理
bool isDragging = false;
int dragStartX, dragStartY;

SDL_Surface* LoadSurfaceFromResource(int resourceID) {
    HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(resourceID), "PNG");
    if (!hResource) {
        MessageBox(NULL, "Failed to find icon resource", "Resource Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    HGLOBAL hGlobal = LoadResource(NULL, hResource);
    if (!hGlobal) {
        MessageBox(NULL, "Failed to load icon resource", "Resource Error", MB_OK | MB_ICONERROR);
        return nullptr;
    }

    void* resourceData = LockResource(hGlobal);
    DWORD resourceSize = SizeofResource(NULL, hResource);

    SDL_RWops* rw = SDL_RWFromMem(resourceData, resourceSize);
    SDL_Surface* surface = IMG_LoadPNG_RW(rw);
    SDL_RWclose(rw);

    return surface;
}

bool init() {
    // 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        MessageBox(NULL, SDL_GetError(), "SDL Init Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 初始化 SDL_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        MessageBox(NULL, IMG_GetError(), "SDL_image Init Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 获取屏幕尺寸
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) < 0) {
        MessageBox(NULL, SDL_GetError(), "Get Display Mode Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // 计算窗口位置（居中）
    int posX = (dm.w - WINDOW_WIDTH) / 2;
    int posY = (dm.h - WINDOW_HEIGHT) / 2;

    // 创建一个隐藏的窗口
    window = SDL_CreateWindow("catime",
        posX, posY,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);

    if (!window) {
        MessageBox(NULL, SDL_GetError(), "Window Creation Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 创建渲染器
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        MessageBox(NULL, SDL_GetError(), "Renderer Creation Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // 设置初始背景色为白色
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    return true;
}

bool loadMedia() {
    // 加载最小化图标
    SDL_Surface* surface = LoadSurfaceFromResource(MINIMIZE_ICON);
    if (!surface) {
        return false;
    }
    minimizeIcon.texture = SDL_CreateTextureFromSurface(renderer, surface);
    minimizeIcon.bounds = {MINIMIZE_X, ICONS_Y, ICON_SIZE, ICON_SIZE};
    minimizeIcon.isHovered = false;  // 初始化悬停状态
    SDL_FreeSurface(surface);

    // 加载关闭图标
    surface = LoadSurfaceFromResource(CLOSE_ICON);
    if (!surface) {
        return false;
    }
    closeIcon.texture = SDL_CreateTextureFromSurface(renderer, surface);
    closeIcon.bounds = {CLOSE_X, ICONS_Y, ICON_SIZE, ICON_SIZE};
    closeIcon.isHovered = false;  // 初始化悬停状态
    SDL_FreeSurface(surface);

    // 加载背景纹理
    surface = LoadSurfaceFromResource(BACKGROUND_ICON);
    if (!surface) {
        return false;
    }
    backgroundTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!minimizeIcon.texture || !closeIcon.texture || !backgroundTexture) {
        MessageBox(NULL, "Failed to create texture from surface!", "Texture Creation Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void close() {
    // 释放资源
    if (minimizeIcon.texture) SDL_DestroyTexture(minimizeIcon.texture);
    if (closeIcon.texture) SDL_DestroyTexture(closeIcon.texture);
    if (backgroundTexture) SDL_DestroyTexture(backgroundTexture);  // 新增释放背景纹理
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

bool isInRect(int x, int y, const SDL_Rect& rect) {
    return (x >= rect.x && x <= rect.x + rect.w &&
            y >= rect.y && y <= rect.y + rect.h);
}

void render() {
    // 清除渲染器并设置为白色背景
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    // 如果有图标被悬停，渲染背景
    if (minimizeIcon.isHovered) {
        SDL_RenderCopy(renderer, backgroundTexture, NULL, &minimizeIcon.bounds);
    }
    if (closeIcon.isHovered) {
        SDL_RenderCopy(renderer, backgroundTexture, NULL, &closeIcon.bounds);
    }

    // 渲染图标
    SDL_RenderCopy(renderer, minimizeIcon.texture, NULL, &minimizeIcon.bounds);
    SDL_RenderCopy(renderer, closeIcon.texture, NULL, &closeIcon.bounds);

    // 更新屏幕
    SDL_RenderPresent(renderer);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    if (!init()) {
        close();
        return -1;
    }

    if (!loadMedia()) {
        close();
        return -1;
    }

    // 初始渲染
    render();
    
    // 显示窗口
    SDL_ShowWindow(window);

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
                case SDL_QUIT:
                    quit = true;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        int mouseX = e.button.x;
                        int mouseY = e.button.y;

                        // 检查是否点击了最小化按钮
                        if (isInRect(mouseX, mouseY, minimizeIcon.bounds)) {
                            SDL_MinimizeWindow(window);
                        }
                        // 检查是否点击了关闭按钮
                        else if (isInRect(mouseX, mouseY, closeIcon.bounds)) {
                            quit = true;
                        }
                        // 开始拖动窗口
                        else {
                            isDragging = true;
                            SDL_GetGlobalMouseState(&dragStartX, &dragStartY);
                        }
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        isDragging = false;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    {
                        int mouseX = e.motion.x;
                        int mouseY = e.motion.y;
                        
                        // 更新图标悬停状态
                        minimizeIcon.isHovered = isInRect(mouseX, mouseY, minimizeIcon.bounds);
                        closeIcon.isHovered = isInRect(mouseX, mouseY, closeIcon.bounds);

                        if (isDragging) {
                            int mouseX, mouseY;
                            SDL_GetGlobalMouseState(&mouseX, &mouseY);
                            int deltaX = mouseX - dragStartX;
                            int deltaY = mouseY - dragStartY;
                            
                            int windowX, windowY;
                            SDL_GetWindowPosition(window, &windowX, &windowY);
                            SDL_SetWindowPosition(window, 
                                windowX + deltaX, 
                                windowY + deltaY);
                            
                            dragStartX = mouseX;
                            dragStartY = mouseY;
                        }
                    }
                    break;
            }
        }

        render();
    }

    close();
    return 0;
}
