#ifndef GUI_H
#define GUI_H

#include <string>
// --- 1. 字符字典：获取指定字符的点阵数据 ---
const unsigned char* getFontData(char c);

// --- 2. 核心函数：绘制单个字符 ---
void drawChar(int *pixel_ptr, int xres, int x, int y, const unsigned char *data, int color);

// --- 3. 进阶函数：绘制整个字符串 ---
void drawString(int *pixel_ptr, int xres, int x, int y, const std::string& str, int color);

#endif
