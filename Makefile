# 设置编译器为 MinGW
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# 设置目标文件夹路径
OUTPUT_DIR = ./bin

# 创建构建目录
BUILD_DIR = build

# 设置文件名 - 添加 async_update_checker.c 到源文件列表
SRC_FILES = src/main.c src/window.c src/tray.c src/color.c src/font.c src/language.c src/timer.c src/tray_menu.c src/startup.c src/config.c src/window_procedure.c src/media.c src/notification.c src/tray_events.c src/window_events.c src/drag_scale.c src/drawing.c src/timer_events.c src/dialog_procedure.c src/update_checker.c src/async_update_checker.c
RC_FILE = resource/resource.rc

# 创建目标文件夹和资源文件夹
$(shell mkdir -p $(OUTPUT_DIR) $(ASSET_DIR))

# 编译选项
CFLAGS = -mwindows -Iinclude -O3 -flto=8 -march=native -ffunction-sections -fdata-sections -fno-strict-aliasing

# 链接选项 - 添加必要的库并优化
LDFLAGS = -lole32 -lshell32 -lcomdlg32 -luuid -lwininet -Wl,--gc-sections -flto=8 -s

# 确保 dwmapi.lib 被链接
LIBS = -ldwmapi -luser32 -lgdi32 -lcomdlg32

# 生成目标文件列表
OBJS = $(BUILD_DIR)/main.o \
       $(BUILD_DIR)/window.o \
       $(BUILD_DIR)/tray.o \
       $(BUILD_DIR)/color.o \
       $(BUILD_DIR)/font.o \
       $(BUILD_DIR)/language.o \
       $(BUILD_DIR)/timer.o \
       $(BUILD_DIR)/tray_menu.o \
       $(BUILD_DIR)/startup.o \
       $(BUILD_DIR)/config.o \
       $(BUILD_DIR)/window_procedure.o \
       $(BUILD_DIR)/media.o \
       $(BUILD_DIR)/notification.o \
       $(BUILD_DIR)/tray_events.o \
       $(BUILD_DIR)/window_events.o \
       $(BUILD_DIR)/drag_scale.o \
       $(BUILD_DIR)/drawing.o \
       $(BUILD_DIR)/timer_events.o \
       $(BUILD_DIR)/dialog_procedure.o \
       $(BUILD_DIR)/update_checker.o \
       $(BUILD_DIR)/async_update_checker.o

# 总文件数量 (包括资源文件)
TOTAL_FILES = 22
PROGRESS_FILE = $(BUILD_DIR)/.progress

# ASCII 艺术标志
define CATIME_LOGO
echo ""
echo -e "\033[96m██████╗ █████╗ ██╗███╗   ███╗███████╗"
echo -e "██╔════╝██╔══██╗██║████╗ ████║██╔════╝"
echo -e "██║     ███████║██║██╔████╔██║█████╗  "
echo -e "██║     ██╔══██║██║██║╚██╔╝██║██╔══╝  "
echo -e "╚██████╗██║  ██║██║██║ ╚═╝ ██║███████╗"
echo -e " ╚═════╝╚═╝  ╚═╝╚═╝╚═╝     ╚═╝╚══════╝\033[0m"
echo ""
endef

# 更新进度计数并显示真实进度条
define update_progress
	@if [ ! -f $(PROGRESS_FILE) ]; then echo "0" > $(PROGRESS_FILE); fi
	@current=$$(cat $(PROGRESS_FILE)); \
	 new_count=$$((current + 1)); \
	 if [ $$new_count -gt $(TOTAL_FILES) ]; then \
	   new_count=$(TOTAL_FILES); \
	 fi; \
	 echo $$new_count > $(PROGRESS_FILE); \
	 percentage=$$((new_count * 100 / $(TOTAL_FILES))); \
	 bar_length=$$((new_count * 40 / $(TOTAL_FILES))); \
	 if [ $$bar_length -gt 40 ]; then \
	   bar_length=40; \
	 fi; \
	 printf "\r\033[38;2;205;214;244mProgress: ["; \
	 for i in $$(seq 1 $$bar_length); do printf "█"; done; \
	 for i in $$(seq 1 $$((40 - bar_length))); do printf "░"; done; \
	 printf "] %3d%% (%d/%d) " "$$percentage" "$$new_count" "$(TOTAL_FILES)"; \
	 if [ $$new_count -eq $(TOTAL_FILES) ]; then printf "\n\033[0m"; fi
endef

# 生成目标 - Run init_progress first, then build dependencies, then run final commands
all: clear_screen show_logo directories init_progress build_executable compress_executable finalize_build

build_executable: $(OUTPUT_DIR)/catime.exe

compress_executable: build_executable
	@original_size=$$(stat -c %s "$(OUTPUT_DIR)/catime.exe"); \
	 printf "Compressing with UPX: [ ]"; \
	 upx --best --lzma "$(OUTPUT_DIR)/catime.exe" > /dev/null 2>&1 & \
	 pid=$$!; \
	 spin='-\|/'; \
	 i=0; \
	 while kill -0 $$pid 2>/dev/null; do \
	 	i=$$(( $$i+1 )); \
	 	printf "\rCompressing with UPX: [%c]" "$${spin:$$((i%4)):1}"; \
	 	sleep 0.1; \
	 done; \
	 wait $$pid; \
	 compressed_size=$$(stat -c %s "$(OUTPUT_DIR)/catime.exe"); \
	 ratio=$$(awk -v o=$$original_size -v c=$$compressed_size 'BEGIN {printf "%.2f", c * 100 / o}'); \
	 printf "\rCompressing with UPX: [Done]\n"; \
	 printf "Compressed: %s -> %s (%s%%)\n" "$$original_size" "$$compressed_size" "$$ratio";

finalize_build: compress_executable
	@echo -e "\033[92mBuild completed! Output directory: $(OUTPUT_DIR)\033[0m"
	@rm -f $(PROGRESS_FILE)

# 清屏
clear_screen:
	@clear || cls

# 显示标志
show_logo:
	@$(CATIME_LOGO)

# 初始化进度 - Should run before compilation starts
init_progress:
	@mkdir -p $(BUILD_DIR)
	@echo "0" > $(PROGRESS_FILE)
	@printf "\033[38;2;205;214;244mProgress: ["; \
	 for i in $$(seq 1 40); do printf "░"; done; \
	 printf "] %3d%% (0/%d) " "0" "$(TOTAL_FILES)"

# 创建必要的目录
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUTPUT_DIR)

# 编译资源文件
$(BUILD_DIR)/resource.o: $(RC_FILE) resource/about_dialog.rc
	@echo "Compiling resources..."
	@$(WINDRES) -I resource $(RC_FILE) -o $(BUILD_DIR)/resource.o
	@$(call update_progress)

# 编译主程序
$(BUILD_DIR)/main.o: src/main.c
	@$(CC) -c src/main.c -o $(BUILD_DIR)/main.o $(CFLAGS)
	@$(call update_progress)

# 编译语言模块
$(BUILD_DIR)/language.o: src/language.c
	@$(CC) -c src/language.c -o $(BUILD_DIR)/language.o $(CFLAGS)
	@$(call update_progress)

# 编译字体模块
$(BUILD_DIR)/font.o: src/font.c
	@$(CC) -c src/font.c -o $(BUILD_DIR)/font.o $(CFLAGS)
	@$(call update_progress)

# 编译颜色模块
$(BUILD_DIR)/color.o: src/color.c
	@$(CC) -c src/color.c -o $(BUILD_DIR)/color.o $(CFLAGS)
	@$(call update_progress)

# 编译托盘模块
$(BUILD_DIR)/tray.o: src/tray.c
	@$(CC) -c src/tray.c -o $(BUILD_DIR)/tray.o $(CFLAGS)
	@$(call update_progress)

# 编译托盘菜单模块
$(BUILD_DIR)/tray_menu.o: src/tray_menu.c
	@$(CC) -c src/tray_menu.c -o $(BUILD_DIR)/tray_menu.o $(CFLAGS)
	@$(call update_progress)

# 编译计时器模块
$(BUILD_DIR)/timer.o: src/timer.c
	@$(CC) -c src/timer.c -o $(BUILD_DIR)/timer.o $(CFLAGS)
	@$(call update_progress)

# 编译窗口模块
$(BUILD_DIR)/window.o: src/window.c
	@$(CC) -c src/window.c -o $(BUILD_DIR)/window.o $(CFLAGS)
	@$(call update_progress)

# 编译自启动模块
$(BUILD_DIR)/startup.o: src/startup.c
	@$(CC) -c src/startup.c -o $(BUILD_DIR)/startup.o $(CFLAGS)
	@$(call update_progress)

# 编译配置模块
$(BUILD_DIR)/config.o: src/config.c
	@$(CC) -c src/config.c -o $(BUILD_DIR)/config.o $(CFLAGS)
	@$(call update_progress)

# 编译窗口过程处理模块
$(BUILD_DIR)/window_procedure.o: src/window_procedure.c include/window_procedure.h
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(call update_progress)

# 编译计时器事件处理模块
$(BUILD_DIR)/timer_events.o: src/timer_events.c
	@$(CC) -c src/timer_events.c -o $(BUILD_DIR)/timer_events.o $(CFLAGS)
	@$(call update_progress)

# 编译拖动和缩放模块
$(BUILD_DIR)/drag_scale.o: src/drag_scale.c
	@$(CC) -c src/drag_scale.c -o $(BUILD_DIR)/drag_scale.o $(CFLAGS)
	@$(call update_progress)

# 编译媒体控制模块
$(BUILD_DIR)/media.o: src/media.c
	@$(CC) -c src/media.c -o $(BUILD_DIR)/media.o $(CFLAGS)
	@$(call update_progress)

# 编译通知模块
$(BUILD_DIR)/notification.o: src/notification.c
	@$(CC) -c src/notification.c -o $(BUILD_DIR)/notification.o $(CFLAGS)
	@$(call update_progress)

# 编译托盘事件处理模块
$(BUILD_DIR)/tray_events.o: src/tray_events.c
	@$(CC) -c src/tray_events.c -o $(BUILD_DIR)/tray_events.o $(CFLAGS)
	@$(call update_progress)

# 编译窗口事件处理模块
$(BUILD_DIR)/window_events.o: src/window_events.c
	@$(CC) -c src/window_events.c -o $(BUILD_DIR)/window_events.o $(CFLAGS)
	@$(call update_progress)

# 编译绘图模块
$(BUILD_DIR)/drawing.o: src/drawing.c
	@$(CC) -c src/drawing.c -o $(BUILD_DIR)/drawing.o $(CFLAGS)
	@$(call update_progress)

# 编译对话框过程处理模块
$(BUILD_DIR)/dialog_procedure.o: src/dialog_procedure.c
	@$(CC) -c src/dialog_procedure.c -o $(BUILD_DIR)/dialog_procedure.o $(CFLAGS)
	@$(call update_progress)

# 编译更新检查器模块
$(BUILD_DIR)/update_checker.o: src/update_checker.c
	@$(CC) -c src/update_checker.c -o $(BUILD_DIR)/update_checker.o $(CFLAGS)
	@$(call update_progress)

# 添加 async_update_checker.o 的编译规则
$(BUILD_DIR)/async_update_checker.o: src/async_update_checker.c
	@$(CC) -c src/async_update_checker.c -o $(BUILD_DIR)/async_update_checker.o $(CFLAGS)
	@$(call update_progress)

# 链接编译目标文件，输出到输出目录
$(OUTPUT_DIR)/catime.exe: $(OBJS) $(BUILD_DIR)/resource.o
	@echo "Linking executable..."
	@$(CC) -o $(OUTPUT_DIR)/catime.exe $(OBJS) $(BUILD_DIR)/resource.o $(CFLAGS) $(LDFLAGS) $(LIBS)
	# The progress bar should be full after this linking step

# 清理构建文件
clean:
	@rm -f $(BUILD_DIR)/*.o $(OUTPUT_DIR)/catime.exe
	@rm -rf $(BUILD_DIR)/include $(BUILD_DIR)/resource
	@rm -f $(PROGRESS_FILE)
	@echo "Build files cleaned."

.PHONY: all clean clear_screen show_logo init_progress directories build_executable compress_executable finalize_build
