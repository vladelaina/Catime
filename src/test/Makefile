# 定义变量
CC = x86_64-w64-mingw32-gcc
CFLAGS = -I/home/vladelaina/code/catime/src/test/libs/SDL2/include \
         -I/home/vladelaina/code/catime/src/test/libs/SDL2_image/include
LDFLAGS = -L/home/vladelaina/code/catime/src/test/libs/SDL2/lib \
          -L/home/vladelaina/code/catime/src/test/libs/SDL2_image/lib
LIBS = -lSDL2main -lSDL2_image -lSDL2 -static -lole32 -luuid -lsetupapi -lcomdlg32 -lwinmm -limm32 -lmsimg32 -lversion -loleaut32  # 添加必要的 Windows 库
OUT = /mnt/c/Users/vladelaina/Desktop/main.exe
SRC = main.c

# 默认目标
all: $(OUT)

# 编译目标
$(OUT): $(SRC)
	$(CC) -o $(OUT) $(SRC) $(CFLAGS) $(LDFLAGS) $(LIBS) -mwindows

# 清理目标
clean:
	rm -f $(OUT)
