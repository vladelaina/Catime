# 设置编译器为 MinGW
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# 设置目标文件夹路径
OUTPUT_DIR = /mnt/c/Users/vladelaina/Desktop
ASSET_DIR = $(OUTPUT_DIR)/asset

# 设置文件名
SRC_FILES = main.c
RC_FILE = resource.rc
OBJ_FILES = main.o

# 创建目标文件夹和资源文件夹
$(shell mkdir -p $(OUTPUT_DIR) $(ASSET_DIR))

# 编译选项
CFLAGS = -mwindows

# 链接选项 - 添加必要的库
LDFLAGS = -lole32 -lshell32 -lcomdlg32 -luuid

# 生成目标
all: $(OUTPUT_DIR)/catime.exe
	@rm -f *.o  # 编译完成后删除所有 .o 文件

# 编译资源文件
resource.o: $(RC_FILE)
	@$(WINDRES) $(RC_FILE) -o resource.o

# 编译 main.c
main.o: $(SRC_FILES)
	@$(CC) -c $(SRC_FILES) -o main.o $(CFLAGS)

# 链接编译目标文件，输出到输出目录
$(OUTPUT_DIR)/catime.exe: $(OBJ_FILES) resource.o
	@$(CC) -o $(OUTPUT_DIR)/catime.exe $(OBJ_FILES) resource.o $(CFLAGS) $(LDFLAGS)

# 清理构建文件
clean:
	@rm -f *.o $(OUTPUT_DIR)/*.exe
