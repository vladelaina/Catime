# 编译器和标志
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -O2 -mwindows

# SDL2 路径
SDL_INCLUDE = -I/home/vladelaina/code/catime/src/test/libs/SDL2/include \
              -I/home/vladelaina/code/catime/src/test/libs/SDL2_image/include \
              -I/usr/x86_64-w64-mingw32/include/SDL2

SDL_LIB = -L/home/vladelaina/code/catime/src/test/libs/SDL2/lib \
          -L/home/vladelaina/code/catime/src/test/libs/SDL2_image/lib \
          -L/usr/x86_64-w64-mingw32/lib

# 链接库
LIBS = -lSDL2 -lSDL2_image -lSDL2_gfx

# 输出文件
OUTPUT = /mnt/c/Users/vladelaina/Desktop/windows.exe

# 默认目标
all: $(OUTPUT)

# 编译规则
$(OUTPUT): main.c
	$(CC) $(CFLAGS) $(SDL_INCLUDE) main.c -o $(OUTPUT) $(SDL_LIB) $(LIBS)

# 清理规则
clean:
	rm -f $(OUTPUT)
