#include <iostream>
#include <unistd.h>
#include <thread>
#include <mutex>
#include "CpuSensor.h"
#include "LightSensor.h"
#include "EepromStorage.h"

// --- 1. 定义全局的”数据保险箱“ ---
struct GatewayData{
	float cpuTemp = 0.0f;
	int lightLux = 0;
	bool isUpdated = false; // 数据更新标志位
};

GatewayData globalData;
std::mutex dataMutex;

// --- 2. 生产者：数据采集线程函数 ---

void dataAcquisitionTask(){
        CpuSensor myCpuSensor;
        LightSensor myLightSensor; // 实例化我们的I2C传感器
        if (!myCpuSensor.init() || !myLightSensor.init()){
                std::cerr << "[Acquisition Thread] Hardware Init failed！Thread exiting." << std::endl;
                return;
        }

        std::cout << "[Acquisition Thread] Started running at 2Hz..." << std::endl;
	while(true){
		float temp = myCpuSensor.getTemperature();
		int lux = myLightSensor.getLightIntensity();

		// 【加锁】打开保险箱，更新数据
		{
		   std::lock_guard<std::mutex> lock(dataMutex); 
		   // 离开大括号作用域会自动解锁 （RAII机制）
		   globalData.cpuTemp = temp;
		   globalData.lightLux = lux;
		   globalData.isUpdated = true;
		}

		usleep(500000); // 500ms 采集一次（高频）
	}
}

// --- 3. 消费者：主线程模拟网络上报 
int main(){
	std::cout << "=== Edge AI Gateway V1.0 (Multi-Threaded) ===" <<std::endl;
	EepromStorage storage;
	if (!storage.init()){
		return -1;
	}	
	std::string mySignature = "Orion_Gateway_V1";
	unsigned char startAddress = 0x00;
	std::string recoveredData = storage.readString(startAddress, mySignature.length());
	std::cout << "Recovered Data from EEPROM: [" << recoveredData << "]" << std::endl;

	// 启动独立的数据采集线程
	std::thread acqThread(dataAcquisitionTask);
	acqThread.detach(); // 分离线程：让它在后台默默跑，不阻塞主线程

	std::cout << "[Network Thread] Started running at 0.5Hz" << std::endl;
	// 主线程模拟 MQTT 网络的发送逻辑
	while(true){
		float tempToUpload = 0.0f;
		int luxToUpload = 0;
		bool hasNewData = false;

		// 【加锁】 打开保险箱，取走最新的数据,只读数据步打印发送！否则是网络耗时操作，采集线程会卡死！
		std::lock_guard<std::mutex> lock(dataMutex);
		if (globalData.isUpdated){
		  	tempToUpload = globalData.cpuTemp;
			luxToUpload = globalData.lightLux;
			globalData.isUpdated = false; // 取走后重置标志位
			hasNewData = true;
		}

		// 模拟网络发送 （如果在加锁范围内执行网络耗时操作，会导致采集线程卡死！）
		if (hasNewData) {
			std::cout << "[Network Thread] Uploading -> CPU Temp:\t" << tempToUpload
			       	<< " °C | Ambient Light:\t" << luxToUpload << std::endl;
		sleep(2);
		}
	}
	return 0;
}
//#include <iostream>
//#include "EepromStorage.h"
//
//int main(){
//	std::cout << "=== Orion Gateway Storage Test ===" << std::endl;
//
//	EepromStorage storage;
//	if(!storage.init()){
//		return -1;
//	}
//
//	// 1. 准备你要永久烧录的专属数据
//	std::string mySignature = "Orion_Gateway_V1";
//	unsigned char startAddress = 0x00;
//
//	// 2. 写入数据
////	storage.writeString(startAddress, mySignature);
////	std::cout << "Data compeletely written. You can now pull the plug!" << std::endl;
////	
//	// 3.验证读取
//	std::string recoveredData = storage.readString(startAddress, mySignature.length());
//	std::cout << "Recovered Data from EEPROM: [" << recoveredData << "]" << std::endl;
//
//	return 0;	
//
//}
//






































