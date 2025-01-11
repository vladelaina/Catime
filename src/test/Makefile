# 定义变量
EXE_NAME = main
CC = x86_64-w64-mingw32-gcc
CFLAGS = -I/home/vladelaina/code/catime/src/test/libs/SDL2/include \
         -I/home/vladelaina/code/catime/src/test/libs/SDL2_image/include
LDFLAGS = -L/home/vladelaina/code/catime/src/test/libs/SDL2/lib \
          -L/home/vladelaina/code/catime/src/test/libs/SDL2_image/lib
LIBS = -lSDL2main -lSDL2_image -lSDL2 -lole32 -luuid -lsetupapi \
       -lcomdlg32 -lwinmm -limm32 -lmsimg32 -lversion -loleaut32
OUT = /mnt/c/Users/vladelaina/Desktop/$(EXE_NAME).exe
SRC = main.c

# 默认目标
all: $(OUT)

# 编译目标
$(OUT): $(SRC)
	$(CC) -o $(OUT) $(SRC) $(CFLAGS) $(LDFLAGS) $(LIBS) -mwindows

# 运行目标
run: $(OUT)
	@cd /mnt/c/Users/vladelaina/Desktop && \
	powershell.exe -Command "./$(EXE_NAME).exe"

# 停止目标
stop:
	@powershell.exe -Command "Stop-Process -Name '$(EXE_NAME)' -Force -ErrorAction SilentlyContinue"

# 清理目标
clean:
	rm -f $(OUT)

.PHONY: all clean run stop
