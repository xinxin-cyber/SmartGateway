#include "CpuSensor.h"
#include <iostream>
#include <fstream>
#include <string>

CpuSensor::CpuSensor(){
	// 默认 CPU 温度设备节点路径
	tempFilePath = "/sys/devices/virtual/thermal/thermal_zone0/temp";
}

CpuSensor::~CpuSensor(){
	// 清理资源 （如果有打开的文件描述符，在这里 close)
}

bool CpuSensor::init(){
	std::cout << "[CpuSensor] Initializing CPU Thermal Sensor..." << std::endl;
	std::ifstream testFile(tempFilePath);
    	if (!testFile.is_open()) {
        	std::cerr << "[CpuSensor] ERROR: Cannot open temperature file!" << std::endl;
        	std::cerr << "[CpuSensor] Path: " << tempFilePath << std::endl; // 打印路径！
        	return false;
   	 }
	return true;
}

float CpuSensor::getTemperature(){
	//1.创建一个文件读取句柄，名字叫 tempFile \
	  2.自动帮你打开 tempFilePath 这个文件 \
	  3.你接下来可以用这个句柄读数据
	std::ifstream tempFile(tempFilePath);
	if (!tempFile.is_open()){
		std::cerr << "[CpuSensor] Error: Failed to open thermal_zone0!" << std::endl;
		return -1.0f;
	}
	
	std::string tempStr;
	std::getline(tempFile, tempStr);
	tempFile.close();

	// Linux 底层读取的温度通常是扩大了 1000 倍的整数，例如 45000 代表 45.000 度
	try {
		int rawTemp = std::stoi(tempStr);
		return static_cast<float>(rawTemp) / 1000.0f;//static_cast<类型 > (变量) 强制类型转换
	} catch (const std::exception& e){
		std::cerr << "[CpuSensor] Error parsing temperature data." << std::endl;
		return -1.0f;
	}
}
	





