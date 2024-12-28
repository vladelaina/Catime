# 编译器设置
CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
WINDRES = x86_64-w64-mingw32-windres

# SDL2 和 SDL2_image 路径
SDL_INCLUDE = -I/home/vladelaina/code/catime/src/test/libs/SDL2/include \
              -I/home/vladelaina/code/catime/src/test/libs/SDL2_image/include

SDL_LIB = -L/home/vladelaina/code/catime/src/test/libs/SDL2/lib \
          -L/home/vladelaina/code/catime/src/test/libs/SDL2_image/lib

# 编译选项
CFLAGS = -mwindows $(SDL_INCLUDE)
LDFLAGS = -static $(SDL_LIB) -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lsetupapi -lwinmm -lversion -limm32 -lole32 -loleaut32 -lws2_32

# 输出文件路径
OUTPUT = /mnt/c/Users/vladelaina/Desktop/windows.exe

# 源文件和资源文件
SRC = src/window.cpp
RC = src/resource.rc
RES = src/resource.o

# 默认目标
all: $(OUTPUT)

# 编译资源文件
$(RES): $(RC)
	$(WINDRES) -I src -I resources $< -o $@

# 编译规则
$(OUTPUT): $(SRC) $(RES)
	$(CXX) -o $(OUTPUT) $^ $(CFLAGS) $(LDFLAGS)

# 清理规则
clean:
	rm -f $(OUTPUT) $(RES)

# .PHONY 表示 clean 是一个伪目标
.PHONY: clean all
