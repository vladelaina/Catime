# 设置编译器为 MinGW
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# 设置目标文件夹路径
OUTPUT_DIR = /mnt/c/Users/vladelaina/Desktop
ASSET_DIR = $(OUTPUT_DIR)/asset

# 设置文件名
SRC_FILES = main.c src/window.c src/tray.c src/color.c src/font.c src/language.c src/timer.c src/tray_menu.c src/startup.c
SRC_LANG = src/language.c
SRC_FONT = src/font.c
SRC_COLOR = src/color.c
SRC_TRAY = src/tray.c
SRC_TRAY_MENU = src/tray_menu.c
SRC_TIMER = src/timer.c
SRC_STARTUP = src/startup.c
RC_FILE = resource/resource.rc
OBJ_FILES = main.o language.o font.o color.o tray.o tray_menu.o timer.o window.o startup.o

# 创建目标文件夹和资源文件夹
$(shell mkdir -p $(OUTPUT_DIR) $(ASSET_DIR))

# 编译选项
CFLAGS = -mwindows -Iinclude

# 链接选项 - 添加必要的库
LDFLAGS = -lole32 -lshell32 -lcomdlg32 -luuid

# 确保 dwmapi.lib 被链接
LIBS = -ldwmapi -luser32 -lgdi32 -lcomdlg32

# 生成目标
all: $(OUTPUT_DIR)/catime.exe
	@rm -f *.o  # 编译完成后删除所有 .o 文件
	@cmd.exe /C start "" "$(shell echo '$(OUTPUT_DIR)/catime.exe' | sed 's#/mnt/c/#C:/#')"  # 转换路径格式

# 编译资源文件
resource.o: $(RC_FILE)
	@$(WINDRES) $(RC_FILE) -o resource.o

# 编译主程序
main.o: main.c
	@$(CC) -c main.c -o main.o $(CFLAGS)

# 编译语言模块
language.o: src/language.c
	@$(CC) -c src/language.c -o language.o $(CFLAGS)

# 编译字体模块
font.o: src/font.c
	@$(CC) -c src/font.c -o font.o $(CFLAGS)

# 编译颜色模块
color.o: src/color.c
	@$(CC) -c src/color.c -o color.o $(CFLAGS)

# 编译托盘模块
tray.o: src/tray.c
	@$(CC) -c src/tray.c -o tray.o $(CFLAGS)

# 编译托盘菜单模块
tray_menu.o: src/tray_menu.c
	@$(CC) -c src/tray_menu.c -o tray_menu.o $(CFLAGS)

# 编译计时器模块
timer.o: src/timer.c
	@$(CC) -c src/timer.c -o timer.o $(CFLAGS)

# 编译窗口模块
window.o: src/window.c
	@$(CC) -c src/window.c -o window.o $(CFLAGS)

# 编译自启动模块
startup.o: src/startup.c
	@$(CC) -c src/startup.c -o startup.o $(CFLAGS)

# 链接编译目标文件，输出到输出目录
$(OUTPUT_DIR)/catime.exe: $(OBJ_FILES) resource.o
	@$(CC) -o $(OUTPUT_DIR)/catime.exe $(OBJ_FILES) resource.o $(CFLAGS) $(LDFLAGS) $(LIBS)

# 清理构建文件
clean:
	@powershell.exe -Command "Stop-Process -Name catime -Force -ErrorAction SilentlyContinue"
	@rm -f *.o $(OUTPUT_DIR)/*.exe
