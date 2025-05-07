MAKEFLAGS += --no-print-directory

# 设置编译器为 MinGW
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres

# 设置目标文件夹路径
OUTPUT_DIR = ./bin

# 创建构建目录
BUILD_DIR = build

# 检测是否在CI环境中运行
ifdef CI
    USE_COLOR =
else
    USE_COLOR = yes
endif

# 定义颜色代码
ifeq ($(USE_COLOR),yes)
    CYAN = \033[96m
    GRAY = \033[38;2;205;214;244m
    BLUE = \033[38;2;137;180;250m
    GREEN = \033[92m
    RESET = \033[0m
else
    CYAN =
    GRAY =
    BLUE =
    GREEN =
    RESET =
endif

# 设置文件名 - 添加 async_update_checker.c 和 log.c 到源文件列表
SRC_FILES = src/main.c src/window.c src/tray.c src/color.c src/font.c src/language.c src/timer.c src/tray_menu.c src/startup.c src/config.c src/window_procedure.c src/media.c src/notification.c src/tray_events.c src/window_events.c src/drag_scale.c src/drawing.c src/timer_events.c src/dialog_procedure.c src/update_checker.c src/async_update_checker.c src/log.c src/audio_player.c
RC_FILE = resource/resource.rc

# 创建目标文件夹和资源文件夹
$(shell mkdir -p $(OUTPUT_DIR) $(ASSET_DIR))

# 编译选项
CFLAGS = -mwindows -Iinclude -Ilibs/miniaudio -O3 -flto=8 -mtune=generic -ffunction-sections -fdata-sections -fno-strict-aliasing

# 链接选项 - 添加必要的库并优化
LDFLAGS = -lole32 -lshell32 -lcomdlg32 -luuid -lwininet -lwinmm -Wl,--gc-sections -flto=8 -s

# 确保 dwmapi.lib 被链接
LIBS = -ldwmapi -luser32 -lgdi32 -lcomdlg32 -lwinmm

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
       $(BUILD_DIR)/async_update_checker.o \
       $(BUILD_DIR)/log.o \
       $(BUILD_DIR)/audio_player.o

# 总文件数量 (包括资源文件)
TOTAL_FILES = 24
PROGRESS_FILE = $(BUILD_DIR)/.progress

# ASCII 艺术标志
define CATIME_LOGO
@printf "\n"
@printf "$(CYAN)██████╗  █████╗ ████████╗██╗███╗   ███╗███████╗$(RESET)\n"
@printf "$(CYAN)██╔════╝ ██╔══██╗╚══██╔══╝██║████╗ ████║██╔════╝$(RESET)\n"
@printf "$(CYAN)██║      ███████║   ██║   ██║██╔████╔██║█████╗  $(RESET)\n"
@printf "$(CYAN)██║      ██╔══██║   ██║   ██║██║╚██╔╝██║██╔══╝  $(RESET)\n"
@printf "$(CYAN)╚██████╗ ██║  ██║   ██║   ██║██║ ╚═╝ ██║███████╗$(RESET)\n"
@printf "$(CYAN) ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝$(RESET)\n"
@printf "\n"
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
	 printf "\r$(GRAY)Progress: ["; \
	 for i in $$(seq 1 $$bar_length); do printf "█"; done; \
	 for i in $$(seq 1 $$((40 - bar_length))); do printf "░"; done; \
	 printf "] %3d%% Complete $(RESET)" "$$percentage"; \
	 if [ $$new_count -eq $(TOTAL_FILES) ]; then printf "\n"; fi
endef

# 生成目标 - Run init_progress first, then build dependencies, then run final commands
all:
	@trap 'rm -f .catime_build_err_tmp .catime_build_err' INT TERM EXIT; \
	{ \
	  ($(MAKE) _all_inner) 2> .catime_build_err && rm -f .catime_build_err || { \
	    (clear || true); \
    [ -f .catime_build_err ] && cat .catime_build_err; \
    rm -f .catime_build_err; \
    exit 1; \
  }; \
}
	@trap - INT TERM EXIT # 解除 trap

_all_inner: clear_screen show_logo directories init_progress build_executable compress_executable finalize_build

build_executable: $(OUTPUT_DIR)/catime.exe

# 修复 compress_executable 函数使用兼容语法
compress_executable: build_executable
	@# Check if compilation was skipped and update progress bar to 100% if needed
	@if [ -f $(PROGRESS_FILE) ] && [ "$$(cat $(PROGRESS_FILE))" -lt "$(TOTAL_FILES)" ]; then \
	  printf "\r$(GRAY)Progress: ["; \
	  for i in $$(seq 1 40); do printf "█"; done; \
	  printf "] 100%% Complete $(RESET)\n"; \
	fi
	@printf "$(BLUE)Compressing...$(RESET)\n"; \
	 size_before=$$(stat -c %s "$(OUTPUT_DIR)/catime.exe"); \
	 printf "Compressing with UPX: [ ]"; \
	 upx --best --lzma "$(OUTPUT_DIR)/catime.exe" > /dev/null 2>&1; \
	 printf "\b\bDone]\n"; \
	 size_after=$$(stat -c %s "$(OUTPUT_DIR)/catime.exe"); \
	 size_before_kb=$$(expr $$size_before / 1024); \
	 size_after_kb=$$(expr $$size_after / 1024); \
	 ratio=0; \
	 if [ $$size_before -ne 0 ]; then \
	   ratio=$$(expr $$size_after \* 100 / $$size_before); \
	 fi; \
	 printf "Compressed: %dKiB -> %dKiB (%d%%)\n" "$$size_before_kb" "$$size_after_kb" "$$ratio";

finalize_build: compress_executable
	@printf "$(GREEN)Build completed! Output directory: $(OUTPUT_DIR)$(RESET)\n"
	@rm -f $(PROGRESS_FILE)

# 清屏 - 仅使用clear命令，不使用cls命令
clear_screen:
	@(clear || true)

# 显示标志
show_logo:
	$(CATIME_LOGO)

# 初始化进度 - Should run before compilation starts
init_progress:
	@mkdir -p $(BUILD_DIR)
	@echo "0" > $(PROGRESS_FILE)
	@printf "$(BLUE)Building...$(RESET)\n"
	@printf "$(GRAY)Progress: ["; \
	 for i in $$(seq 1 40); do printf "░"; done; \
	 printf "] %3d%% Complete $(RESET)" "0"

# 创建必要的目录
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUTPUT_DIR)

# 编译资源文件
$(BUILD_DIR)/resource.o: $(RC_FILE) resource/about_dialog.rc
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

# 添加 log.o 的编译规则
$(BUILD_DIR)/log.o: src/log.c
	@$(CC) -c src/log.c -o $(BUILD_DIR)/log.o $(CFLAGS)
	@$(call update_progress)

# 编译音频播放器模块
$(BUILD_DIR)/audio_player.o: src/audio_player.c include/audio_player.h
	@$(CC) -c src/audio_player.c -o $(BUILD_DIR)/audio_player.o $(CFLAGS) -DMINIAUDIO_IMPLEMENTATION
	@$(call update_progress)

# 链接编译目标文件，输出到输出目录
$(OUTPUT_DIR)/catime.exe: $(OBJS) $(BUILD_DIR)/resource.o
	@$(CC) -o $(OUTPUT_DIR)/catime.exe $(OBJS) $(BUILD_DIR)/resource.o $(CFLAGS) $(LDFLAGS) $(LIBS)

# 清理构建文件
clean:
	@rm -f $(BUILD_DIR)/*.o $(OUTPUT_DIR)/catime.exe
	@rm -rf $(BUILD_DIR)/include $(BUILD_DIR)/resource
	@rm -f $(PROGRESS_FILE)
	@echo "Build files cleaned."

.PHONY: all _all_inner clean clear_screen show_logo init_progress directories build_executable compress_executable finalize_build
