#include <iostream>
#include <string>
#include <gui.h>
#include <cstring>
// 🔥 仅新增：引入完整字库头文件
#include "font_8x16.h"

// --- 1. 仅重写字库获取函数，使用完整8x16字库，其余代码完全不动 ---
const unsigned char* getFontData(char c) {
    const int FONT_HEIGHT = 16;    // ✅ 正确：局部常量，不会被覆盖
    const int ASCII_START = 0x20;
    // 超出可打印字符，返回空格
    if (c < ASCII_START || c > 0x7E) {
        return fontdata_8x16; // 空格点阵
    }
    
    // 计算字符在完整字库中的位置
    int index = (c - ASCII_START) * FONT_HEIGHT;
    return &fontdata_8x16[index];
}

// --- 以下代码 **完全原样保留**，一行未改 ---
// --- 2. 核心函数：绘制单个字符 ---
void drawChar(int *pixel_ptr, int xres, int x, int y, const unsigned char *data, int color) {
    for (int i = 0; i < 16; i++) {
        unsigned char line = data[i];
        for (int j = 0; j < 8; j++) {
            if ((line >> (7 - j)) & 0x1) {
                int target_index = (y + i) * xres + (x + j);
                pixel_ptr[target_index] = color;
            }else {
                int target_index = (y + i) * xres + (x + j);
		pixel_ptr[target_index] = 0x00000000; // 无笔画，强制画背景色 (如深灰)，彻底杜绝重影！
            }
        }
	// 【修补缝隙】强行把字符右侧那 2 个像素的间距也涂黑！
        pixel_ptr[(y + i) * xres + (x + 8)] = 0x00000000;
        pixel_ptr[(y + i) * xres + (x + 9)] = 0x00000000;

    }
}

// --- 3. 进阶函数：绘制整个字符串 ---
void drawString(int *pixel_ptr, int xres, int x, int y, const std::string& str, int color) {
    int current_x = x;
    for (char c : str) {
        const unsigned char* char_data = getFontData(c);
        drawChar(pixel_ptr, xres, current_x, y, char_data, color);
        current_x += 10; // 每个字符占 8 像素，加上 2 像素间距

    }
    // 【推土机擦除】在字符串屁股后面，强行画 3 个纯黑的空格！
    // 彻底干掉 "45CC" 或者 "46.5" 这种尾部残留垃圾！
    drawChar(pixel_ptr, xres, current_x, y, getFontData(' '), color);
    drawChar(pixel_ptr, xres, current_x + 10, y, getFontData(' '), color);
    drawChar(pixel_ptr, xres, current_x + 20, y, getFontData(' '), color);
}

void drawVideoFrame(int *pixel_ptr, int xres, int start_x, int start_y, int width, int height, void* frame_data) {
    if (!pixel_ptr || !frame_data) return;

    // 假设像素是 32bit (4字节)，也就是一个 int 的大小
    int *dst = pixel_ptr;
    int *src = (int*)frame_data;

    // 逐行进行 memcpy 拷贝
    for (int y = 0; y < height; ++y) {
        // 计算目标显存的偏移量 (加上 start_y 和 start_x 的偏移)
        int dst_offset = (start_y + y) * xres + start_x;
        // 计算源视频帧的偏移量
        int src_offset = y * width;

        // 拷贝这一整行 (width 个像素，每个像素 sizeof(int) 个字节)
        memcpy(dst + dst_offset, src + src_offset, width * sizeof(int));
    }
}
