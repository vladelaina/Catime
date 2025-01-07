#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gif_lib.h>
#include <png.h>

// 创建一个图像缓冲区来存储完整的帧
typedef struct {
    unsigned char* buffer;
    int width;
    int height;
} ImageBuffer;

// 初始化图像缓冲区
ImageBuffer* create_buffer(int width, int height) {
    ImageBuffer* buffer = (ImageBuffer*)malloc(sizeof(ImageBuffer));
    buffer->width = width;
    buffer->height = height;
    buffer->buffer = (unsigned char*)calloc(width * height * 4, sizeof(unsigned char));
    return buffer;
}

// 释放图像缓冲区
void free_buffer(ImageBuffer* buffer) {
    if (buffer) {
        free(buffer->buffer);
        free(buffer);
    }
}

// 将GIF帧合成到缓冲区
void compose_frame(ImageBuffer* buffer, const SavedImage* frame, const ColorMapObject* colormap, 
                  const GraphicsControlBlock* gcb) {
    int transparent_color = -1;
    if (gcb->TransparentColor != NO_TRANSPARENT_COLOR) {
        transparent_color = gcb->TransparentColor;
    }

    for (int y = 0; y < frame->ImageDesc.Height; y++) {
        for (int x = 0; x < frame->ImageDesc.Width; x++) {
            int pixel_index = frame->RasterBits[y * frame->ImageDesc.Width + x];
            int pos = ((y + frame->ImageDesc.Top) * buffer->width + (x + frame->ImageDesc.Left)) * 4;

            // 如果不是透明像素，则更新缓冲区
            if (pixel_index != transparent_color) {
                buffer->buffer[pos] = colormap->Colors[pixel_index].Red;
                buffer->buffer[pos + 1] = colormap->Colors[pixel_index].Green;
                buffer->buffer[pos + 2] = colormap->Colors[pixel_index].Blue;
                buffer->buffer[pos + 3] = 255; // 完全不透明
            }
        }
    }
}

// 将缓冲区保存为PNG
void save_buffer_as_png(const ImageBuffer* buffer, const char* output_path) {
    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        printf("无法创建PNG文件: %s\n", output_path);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, buffer->width, buffer->height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    png_bytep row = (png_bytep)malloc(4 * buffer->width * sizeof(png_byte));
    
    for (int y = 0; y < buffer->height; y++) {
        memcpy(row, buffer->buffer + y * buffer->width * 4, buffer->width * 4);
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    free(row);
    fclose(fp);
}

void process_gif(const char* gif_path) {
    int error = 0;
    GifFileType* gif = DGifOpenFileName(gif_path, &error);
    if (!gif) {
        printf("无法打开GIF文件: %s\n", gif_path);
        return;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        printf("读取GIF文件失败: %s\n", gif_path);
        DGifCloseFile(gif, &error);
        return;
    }

    char dir_name[256];
    strncpy(dir_name, gif_path, strlen(gif_path) - 4);
    dir_name[strlen(gif_path) - 4] = '\0';
    mkdir(dir_name);

    // 创建图像缓冲区
    ImageBuffer* buffer = create_buffer(gif->SWidth, gif->SHeight);
    
    // 创建背景缓冲区（用于存储完整的帧）
    ImageBuffer* prev_buffer = create_buffer(gif->SWidth, gif->SHeight);

    // 如果有全局颜色表，先填充背景色
    if (gif->SColorMap) {
        int background_color = gif->SBackGroundColor;
        if (background_color < gif->SColorMap->ColorCount) {
            for (int i = 0; i < gif->SWidth * gif->SHeight; i++) {
                int pos = i * 4;
                prev_buffer->buffer[pos] = gif->SColorMap->Colors[background_color].Red;
                prev_buffer->buffer[pos + 1] = gif->SColorMap->Colors[background_color].Green;
                prev_buffer->buffer[pos + 2] = gif->SColorMap->Colors[background_color].Blue;
                prev_buffer->buffer[pos + 3] = 255;
            }
        }
    }

    // 处理每一帧
    for (int i = 0; i < gif->ImageCount; i++) {
        // 复制前一帧的内容到当前缓冲区
        memcpy(buffer->buffer, prev_buffer->buffer, buffer->width * buffer->height * 4);

        SavedImage* frame = &gif->SavedImages[i];
        ColorMapObject* colormap = frame->ImageDesc.ColorMap ? 
                                 frame->ImageDesc.ColorMap : gif->SColorMap;

        // 获取图形控制块
        GraphicsControlBlock gcb;
        DGifSavedExtensionToGCB(gif, i, &gcb);

        // 根据处置方法处理帧
        if (gcb.DisposalMode == DISPOSE_BACKGROUND) {
            // 如果是背景处置方法，清除前一帧的区域
            for (int y = frame->ImageDesc.Top; y < frame->ImageDesc.Top + frame->ImageDesc.Height; y++) {
                for (int x = frame->ImageDesc.Left; x < frame->ImageDesc.Left + frame->ImageDesc.Width; x++) {
                    int pos = (y * buffer->width + x) * 4;
                    buffer->buffer[pos] = 0;
                    buffer->buffer[pos + 1] = 0;
                    buffer->buffer[pos + 2] = 0;
                    buffer->buffer[pos + 3] = 0;
                }
            }
        }

        // 合成当前帧
        compose_frame(buffer, frame, colormap, &gcb);

        // 保存为PNG
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/frame_%04d.png", 
                dir_name, i);
        save_buffer_as_png(buffer, output_path);
        
        printf("已保存帧 %d/%d: %s\n", i + 1, gif->ImageCount, output_path);

        // 保存当前帧作为下一帧的背景
        memcpy(prev_buffer->buffer, buffer->buffer, buffer->width * buffer->height * 4);
    }

    free_buffer(buffer);
    free_buffer(prev_buffer);
    DGifCloseFile(gif, &error);
}

int main() {
    DIR* dir = opendir(".");
    if (!dir) {
        printf("无法打开当前目录\n");
        return 1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".gif") || strstr(entry->d_name, ".GIF")) {
            printf("正在处理: %s\n", entry->d_name);
            process_gif(entry->d_name);
        }
    }

    closedir(dir);
    printf("所有GIF文件处理完成！\n");
    return 0;
}
