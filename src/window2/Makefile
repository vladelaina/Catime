# 编译器和标志
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -O2 -mwindows

# SDL2 和 SDL2_image 路径
SDL_INCLUDE = -I/home/vladelaina/code/catime/src/test/libs/SDL2/include \
              -I/home/vladelaina/code/catime/src/test/libs/SDL2_image/include

SDL_LIB = -L/home/vladelaina/code/catime/src/test/libs/SDL2/lib \
          -L/home/vladelaina/code/catime/src/test/libs/SDL2_image/lib

# 链接库
LIBS = -lSDL2 -lSDL2_image

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
