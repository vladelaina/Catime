# Makefile

# 输出目录
OUT = /mnt/c/Users/vladelaina/Desktop

# 目标可执行文件路径
TARGET = $(OUT)/output.exe

# 编译器
CC = x86_64-w64-mingw32-gcc

# 编译选项
CFLAGS = -static -static-libgcc -I/usr/x86_64-w64-mingw32/include

# 链接选项
LDFLAGS = -L/usr/x86_64-w64-mingw32/lib -lgif -lpng -lz

# 源文件
SRCS = main.c

# 生成的目标文件
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 链接目标
$(TARGET): $(OBJS) | $(OUT)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 编译源文件为目标文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 创建输出目录
$(OUT):
	mkdir -p $(OUT)

# 清理编译生成的文件
clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
