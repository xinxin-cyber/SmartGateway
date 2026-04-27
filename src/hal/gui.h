#ifndef GUI_H
#define GUI_H

#include <string>
// --- 1. 字符字典：获取指定字符的点阵数据 ---
const unsigned char* getFontData(char c);

// --- 2. 核心函数：绘制单个字符 ---
void drawChar(int *pixel_ptr, int xres, int x, int y, const unsigned char *data, int color);

// --- 3. 进阶函数：绘制整个字符串 ---
void drawString(int *pixel_ptr, int xres, int x, int y, const std::string& str, int color);
// 将共享内存里的纯像素数据，按行暴力拷贝到 /dev/fb0 显存中
void drawVideoFrame(int *pixel_ptr, int xres, int start_x, int start_y, int width, int height, void* frame_data);
#endif
