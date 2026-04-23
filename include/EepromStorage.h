#ifndef EEPROMSTORAGE_H
#define EEPROMSTORAGE_H

#include <string>

class EepromStorage {
public:
	EepromStorage();
	~EepromStorage();

	// 初始化 I2C 总线并绑定设备地址 0x50
	bool init();

	// 向指定内存地址写入一个字符串
	bool writeString(unsigned char startAddr, const std::string& data);

	// 从指定内存地址读取一段长度的字符串
	std::string readString(unsigned char startAddr, int length);

private:
	int i2c_fd;

	// 内部核心底层函数：写入单字节（包含 5ms 强制延时）
	bool writeByte(unsigned char memAddr, char data);

	// 内部核心底层函数：读取单字节（伪写后续）
	char readByte(unsigned char memeAddr);
};

#endif
		
