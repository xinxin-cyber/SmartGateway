#include <iostream>
#include <thread>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
// 传感器头文件
#include "CpuSensor.h"
#include "LightSensor.h"
#include "gui.h"
// --- 全局保险箱 ---
struct SensorData {
	float cpu_temp;
	int ambient_light;
};

SensorData globalData = {0.0f, 0};
std::mutex dataMutex;

// --- 底层 UI 绘制函数 ---
void drawRectangle(int *pixel_ptr, int xres, int start_x, int start_y, int width, int height, int color) {
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            int index = y * xres + x;
            pixel_ptr[index] = color;
        }
    }
}

// --- 后台采集线程（生产者） ---
void dataAcquisitionTask(){
	std::cout << "[Acquisition] Thread started." << std::endl;
	CpuSensor cpuSensor;
	LightSensor lightSensor;
	cpuSensor.init();
	lightSensor.init();

	while(true){
		float current_temp = cpuSensor.getTemperature();
		int current_light = lightSensor.getLightIntensity();

		// 上锁更新数据
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			globalData.cpu_temp = current_temp;
			globalData.ambient_light = current_light;
		}
		
		usleep(500000); // 500ms -- 2Hz 采样率
	}
}

// --- 主线程：UI 渲染引擎（消费者） ---
int main(){
	std::cout << "=== Orion Gateway V1.5 (Visual Edition) ===" << std::endl;

	// 1. 初始化大屏幕 Framebuffer
	int fb_fd = open("/dev/fb0", O_RDWR);
	if (fb_fd < 0) return -1;

	struct fb_var_screeninfo vinfo;
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	int *pixel_ptr = (int *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

	// 刷黑全屏
    	drawRectangle(pixel_ptr, vinfo.xres, 0, 0, vinfo.xres, vinfo.yres, 0x00000000);
	// 画 UI 外框（温度条和光照状态框）
	drawRectangle(pixel_ptr, vinfo.xres, 95, 195, 610, 60, 0x00333333); // 温度底框
	drawRectangle(pixel_ptr, vinfo.xres, 95, 295, 110, 110, 0x00333333); // 光照底框

	// 2. 启动后台采集线程
	std::thread acqThread(dataAcquisitionTask);
	acqThread.detach();

	// 3. 进入高频 UI 渲染循环
	while (true){
		SensorData localCopy;
		// 极速开锁，取走最新数据
		{
			std::lock_guard<std::mutex> lock(dataMutex);
			localCopy = globalData;
		}

		// --- 渲染温度进度条 ---
		// 假设温度在 30 到 80 度之间浮动，映射到 600 像素的宽度；
		int temp_width = (int)((localCopy.cpu_temp - 30.0f) / 50.0f * 600);
	        if (temp_width < 0) temp_width = 0;
		if (temp_width > 600) temp_width = 600;
		
		int temp_color = 0x0000FF00; // 绿
		if (localCopy.cpu_temp > 65.0f) temp_color = 0x00FF0000; //红
		drawRectangle(pixel_ptr, vinfo.xres, 100, 200, 600, 50, 0x00000000); // 擦除旧进度条
		drawRectangle(pixel_ptr, vinfo.xres, 100, 200, temp_width, 50, temp_color); // 画新进度条	
		// 在进度条上方显示具体数字
		char temp_buf[16];
		// "%.1f" 的魔法就在于它强制规定：只留 1 位小数！
		snprintf(temp_buf, sizeof(temp_buf), "%.1f", localCopy.cpu_temp);
		std::string temp_str = "Temp: " + std::string(temp_buf) + " C";
		drawString(pixel_ptr, vinfo.xres, 100, 170, temp_str, 0x00FFFFFF); // 白色文字
		// --- 渲染光照指示块 ---
		//  如果亮度大于 50 ，显示黄色块；否则显示暗灰色
		int light_color = (localCopy.ambient_light > 50) ? 0x00FFD700 : 0x00555555;
	        drawRectangle(pixel_ptr, vinfo.xres, 100, 300, 100, 100, light_color); 
       		
 		usleep(100000); // UI 刷新率 10Hz （0.1 秒）		
	}
	// 刷黑全屏
        drawRectangle(pixel_ptr, vinfo.xres, 0, 0, vinfo.xres, vinfo.yres, 0x00000000);
	munmap(pixel_ptr, screensize);
	close(fb_fd);
	return 0;

}



























