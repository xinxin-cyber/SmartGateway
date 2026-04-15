#ifndef CPUSENSOR_H
#define CPUSENSOR_H

#include <string>

//CPU 温度传感器类
class CpuSensor {
public:
	CpuSensor();
	~CpuSensor();

	// 初始化传感器 （虽然读文件系统不需要真正初始化硬件，但保留此接口作为规范）
	bool init();

	// 获取当前 CPU 温度 （摄氏度）
	float getTemperature();
private:
	std::string tempFilePath;
};

#endif //CPUSENSOR_H
