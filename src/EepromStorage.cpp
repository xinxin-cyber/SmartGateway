#include "EepromStorage.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define AT2402_ADDR 0x50 // EEPROM 的 I2C 设备地址

EepromStorage::EepromStorage():i2c_fd(-1){}

EepromStorage::~EepromStorage(){
	if (i2c_fd >= 0){
		close(i2c_fd);
	}
}

bool EepromStorage::init(){
	i2c_fd = open("/dev/i2c-0",O_RDWR);
	if (i2c_fd < 0){
		std::cerr << "[EEPROM] Error: Failed to open /dev/i2c-0" <<std::endl;
		return false;
	}

	// 绑定大门牌号
	if (ioctl(i2c_fd,I2C_SLAVE, AT2402_ADDR) < 0) {
		std::cerr << "[EEPROM] Error: Failed to bind adress 0x50" << std::endl;
		return false;
	}
	std::cout << "[EEPROM] AT2402 Initialized on I2C-0" << std::endl;
	return true;
}

bool EepromStorage::writeByte(unsigned char memAddr, char data){
	// 关键点：发送的数组必须是 [内存地址，真实数据]
	unsigned char buffer[2];
	buffer[0] = memAddr;
	buffer[1] = data;

	if (write(i2c_fd,buffer, 2) != 2){
		return false;
	}

	// 硬件灵魂代码：AT2402 规定的 5ms 物理烧录周期
	usleep(5000);
	return true;
}

char EepromStorage::readByte(unsigned char memAddr){
	// 1. 伪写：扔过去一个”房间号“，设置地址指针
	if (write(i2c_fd, &memAddr, 1) != 1){
		return '\0';
	}

	// 2. 读取：把这个房间里的数据拿回来
	char data  = 0;
	if (read(i2c_fd, &data, 1)!=1){
		return '\0';
	}	
	return data;
}
bool EepromStorage::writeString(unsigned char startAddr, const std::string& str) {
	std::cout << "[EEPROM] Burning data to silicon: " << str << std::endl;
	for (size_t i = 0; i < str.length(); ++i){
		// 将 C++ 字符串拆解为诸葛字符，存入连续的地址
		if (!writeByte(startAddr  + i, str[i])) {
			std::cerr << "[EEPROM] Write failed at index " << i << std::endl;
			return false;		
		}
	}
	return true;
}

std::string EepromStorage::readString(unsigned char startAddr, int length){
	std::string result = "";
	for (int i = 0; i < length; ++i){
		char c = readByte(startAddr + i);
		result += c;
	}
	return result;

}

































