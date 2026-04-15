#include "LightSensor.h"
#include <iostream>
#include <fcntl.h>  // open() 打开文件（I2C 设备）
#include <unistd.h> // read()/write()/close()
#include <sys/ioctl.h> // ioctl() 设置 I2C 地址
#include <linux/i2c-dev.h> // Linux I2C 驱动定义

// AP3216 传感器的 I2C 地址和关键寄存器
#define AP3216C_ADDR	0x1E
#define REG_SYS_CFG	0x00 //系统配置寄存器
#define REG_ALS_L	0x0C //光照数据低 8 位
#define REG_ALS_H	0x0D //        高 8 位

LightSensor::LightSensor() : i2c_fd(-1) {}

LightSensor::~LightSensor(){
	if (i2c_fd >= 0){
		close(i2c_fd);
		std::cout << "[LightSensor} I2C Bus Closed." << std::endl;
	}
}

bool LightSensor::init(){
	// 1. 打开 Linux 系统下的 I2C-0 系统设备节点
	i2c_fd = open("/dev/i2c-0", O_RDWR);
	if (i2c_fd <0){
		std::cerr << "[LightSensor] Error: Failed to open /dev/i2c-0" << std::endl;
		return false;
	}

	// 2. 告诉 Linux 内核， 我们要和地址为 0X1E 的从设备通信
	if (ioctl(i2c_fd, I2C_SLAVE, AP3216C_ADDR) < 0){
		std::cerr << "[LightSensor] Error: Failed to acquire bus access/talk to salve." << std::endl;
		return false;
	}
	// 3. 按照芯片手册，软复位芯片（往 0x00 寄存器写 0x04）
	writeRegister(REG_SYS_CFG, 0x04);
	usleep(50000); // 延时 50ms 等待复位完成
	
	// 4. 开启 ALS（环境光）和 PS(距离) 功能 （写 0x03)
	writeRegister(REG_SYS_CFG, 0x03);	
	usleep(50000);
	
	std::cout << "[LightSensor] AP3216C Initialized Sucessfully on I2C-0." << std::endl;
	return true;
}

int LightSensor::getLightIntensity(){
	if (i2c_fd < 0) return -1;

	// 分别读取低八位和高八位寄存器
	int als_l = readRegister(REG_ALS_L);
	int als_h = readRegister(REG_ALS_H);
	
	if (als_l < 0 || als_h < 0) return -1;

	// 拼接 16 位数据
	int intensity = (als_h << 8) | als_l;
	return intensity;
}

int LightSensor::readRegister(unsigned char reg){
	// 1. 先写入要读取的寄存器地址
	if (write(i2c_fd, &reg, 1)!=1) return -1;

	// 2. 读取该寄存器里的值
	unsigned char value = 0;
	if (read(i2c_fd, &value, 1) !=1) return -1;
	
	return value;
}

bool LightSensor::writeRegister(unsigned char reg, unsigned char value) {
	unsigned char buf[2] = {reg, value};
	if (write(i2c_fd, buf, 2) != 2) {
		return false;
	}
	// 使用标准 Linux 系统调用 write() 发送数据
	return true;
}
