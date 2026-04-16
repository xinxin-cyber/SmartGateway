#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

// --- 封装一个画矩形的函数 ---
void drawRectangle(int *pixel_ptr, int xres, int start_x,
		int start_y, int width, int height, int color){
	for (int y = start_y; y < start_y + height; y++){
		for (int x = start_x ; x < start_x + width; x++){
			int index = y * xres + x;
			pixel_ptr[index] = color;
		}
	}
}

int main(){
	std::cout << "=== Orion Framebuffer mmp Test ===" << std::endl;

	// 1. 打开显存设备节点
	int fb_fd = open("/dev/fb0",O_RDWR);
	if (fb_fd < 0){
		std::cerr << "Error: Cannot open /dev/fb0. Are you root?" << std::endl;
		return -1;
	}

	// 2. 动态获取屏幕参数（让代码适配未来的任何屏幕）
	struct fb_var_screeninfo vinfo;
	if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0){
		std::cerr << "Error reading variable information." << std::endl;
		close(fb_fd);
		return -1;
	}

	std::cout << "Hardware Resolution: " << vinfo.xres << " x " << vinfo.yres << std::endl;
	std::cout << "Color Depth: " << vinfo.bits_per_pixel << " bpp" << std::endl;


	// 计算显存总字节数： 宽 * 高 *（位深 / 8）
	long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	// 3. 核心： 内存映射 （零拷贝）
	// 把物理显存直接映射到 pixel_ptr 这个指针上
	int *pixel_ptr = (int *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
       	if (pixel_ptr == MAP_FAILED){
		std::cerr << "Error: Failed to map framebuffer device to memory." << std::endl;
		close(fb_fd);
		return -1;
	}	
	
	// 4. 先把全屏刷成黑色（清屏）
	std::cout << "Cleaning screen..." << std::endl;
	drawRectangle(pixel_ptr, vinfo.xres, 0, 0, vinfo.xres, vinfo.yres, 0x00000000);

	// 5. 画一个灰色的进度外框
	int bar_x = 100;
	int bar_y = 200;
	int bar_width = 800;
	int bar_height = 100;
        drawRectangle(pixel_ptr, vinfo.xres, bar_x - 5, bar_y - 5, bar_width + 10, bar_height + 10, 0x0055555555);
	
	// 6. 动画演示：模拟温度从 0 - 100
	std::cout << "Animating Temperature Bar..."<<std::endl;
	for (int temp = 0; temp <=100; temp += 1){
		// 计算当前温度对应的宽度
		int current_width = (bar_width * temp) / 100;

		// 动态选择颜色：冷色（蓝）-> 正常（绿） -> 警告（红）
		int color = 0x0000FF00; // 默认绿色
		if (temp < 30) color = 0x000000FF;
		else if (temp > 70) color = 0x00FF0000;

		// 覆盖绘制进度条
		drawRectangle(pixel_ptr, vinfo.xres, bar_x, bar_y, current_width, bar_height, color);
		usleep(300000);
	}

	sleep(3);

//	// 4. 开始越过 GPU，进行暴力刷屏！
//	int num_pixels = vinfo.xres * vinfo.yres;
//
//	std::cout << "Painting RED..." << std::endl;
//	for (int i = 0; i < num_pixels; i++){
//	//	usleep(10);
//		pixel_ptr[i] = 0x00FF0000;
//	}
//	sleep(2);
//
//	std::cout << "Painting GREEN..." << std::endl;
//        for (int i = 0; i < num_pixels; i++){
//	//	usleep(10);
//                pixel_ptr[i] = 0x0000FF00;
//        }
//	sleep(2);
//
//	std::cout << "Painting BLUE..." << std::endl;
//        for (int i = 0; i < num_pixels; i++){
//	//	usleep(10);
//
//                pixel_ptr[i] = 0x000000FF;
//        }
//	sleep(2);

	// 5. 收尾：解除映射，关闭文件
	drawRectangle(pixel_ptr, vinfo.xres, bar_x-5, bar_y-5, bar_width+10, bar_height+10, 0x00000000);
	munmap(pixel_ptr, screensize);
	close(fb_fd);
	std::cout << "Framebuffer test completed!" << std::endl;
        return 0;	


}
