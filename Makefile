# 设置编译器为 MinGW
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# 设置目标文件夹路径
OUTPUT_DIR = /mnt/d/Sundries/exe/Catime
ASSET_DIR = $(OUTPUT_DIR)/asset

# 设置源文件路径
SRC_DIR = src
RESOURCE_DIR = resources
BUILD_DIR = build
INCLUDE_DIR = include  # 包含 include 目录

# 设置文件名
SRC_FILES = $(SRC_DIR)/main.c $(SRC_DIR)/rightClickMenu.c
RC_FILE = $(RESOURCE_DIR)/resource.rc
OBJ_FILES = $(BUILD_DIR)/main.o $(BUILD_DIR)/rightClickMenu.o
IMAGE_DIR = $(RESOURCE_DIR)/images

# 创建目标文件夹和资源文件夹
$(shell mkdir -p $(OUTPUT_DIR) $(ASSET_DIR))

# 编译选项
CFLAGS = -mwindows -I$(INCLUDE_DIR)  # 包含 include 目录

# 生成目标
all: $(OUTPUT_DIR)/catime.exe
	# 转换路径并启动
	@cmd.exe /C start "" "$(shell wslpath -w $(OUTPUT_DIR))/catime.exe"

# 编译资源文件，将 .o 文件输出到 build 目录
$(BUILD_DIR)/resource.o: $(RC_FILE)
	@$(WINDRES) $(RC_FILE) -o $(BUILD_DIR)/resource.o

# 编译 main.c
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
	@$(CC) -c $(SRC_DIR)/main.c -o $(BUILD_DIR)/main.o $(CFLAGS)

# 编译 rightClickMenu.c
$(BUILD_DIR)/rightClickMenu.o: $(SRC_DIR)/rightClickMenu.c
	@$(CC) -c $(SRC_DIR)/rightClickMenu.c -o $(BUILD_DIR)/rightClickMenu.o $(CFLAGS)

# 链接编译目标文件，输出到输出目录
$(OUTPUT_DIR)/catime.exe: $(OBJ_FILES) $(BUILD_DIR)/resource.o
	@$(CC) -o $(OUTPUT_DIR)/catime.exe $(OBJ_FILES) $(BUILD_DIR)/resource.o $(CFLAGS)
	@cp -r $(IMAGE_DIR) $(ASSET_DIR)  # 复制 images 文件夹到 asset 目录

# 清理构建文件，只删除 .o 文件，不删除 build 目录
clean:
	@rm -rf $(BUILD_DIR)/*.o $(OUTPUT_DIR)/*.exe

