#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

class LightSensor {
public:
	LightSensor();
	~LightSensor();

	//初始化 I2C 总线和传感器
	bool init();

	// 读取环境光照度 （Ambient Light Sensor）
	int getLightIntensity();

private:
	int i2c_fd; // I2C 设备的文件描述符

	// 内部辅助函数：向从传感器写寄存器
	// 作用：往传感器的某个寄存器里写一个值
	// reg：寄存器地址（比如 0x20 控制寄存器）
	// value：要写入的数据（比如 0x0F 打开传感器）
	// 返回值：true 成功 / false 失败
	bool writeRegister(unsigned char reg, unsigned char value);
	// 内部辅助函数：从传感器读寄存器
	int readRegister(unsigned char reg);
};

#endif
