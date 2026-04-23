#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <mosquitto.h> // MQTT 核心头文件
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- 武器 1：读取真实物理温度 ---
float getRealCpuTemperature() {
    std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (!tempFile.is_open()) return -1.0f;
    int raw_temp;
    tempFile >> raw_temp;
    tempFile.close();
    return raw_temp / 1000.0f;
}

// --- 武器 2：现代 C++ 封装的 GPIO 控制器 ---
class LedController{
private:
	std::string gpio_num;
	std::string gpio_path;

	// 底层私有方法：向 Linux 系统文件写入命令
	void writeToFile(const std::string& path, const std::string& value){
		std::ofstream file(path);
		if (file.is_open()){
			file << value;
			file.close();
		}else{
			std::cerr << "[硬件层报错] 无法操作物理引脚，权限不足或引脚错误：" << path << std::endl;
		}
	}
public:
	// 构造函数 （RAII: 对象创建时自动导出 GPIO 并设置为输出模式）
	LedController(int pin){
		gpio_num = std::to_string(pin);
		gpio_path = "/sys/class/gpio/gpio" + gpio_num;
     		
		std::cout << "[系统初始化] 正在挂载物理引脚 GPIO " << gpio_num << "..." << std::endl;
		writeToFile("/sys/class/gpio/export", gpio_num);
		usleep(100000);
		writeToFile(gpio_path + "/direction", "out");		
	}

	// ~析构函数
	~LedController(){
		std::cout<< "[系统清理] 释放物理引脚 GPIO " << gpio_num << std::endl;
		writeToFile("/sys/class/gpio/unexport", gpio_num);

	}

	void turnOn(){
		writeToFile(gpio_path + "/value", "0");
		std::cout << "[硬件驱动] 物理 LED 灯已点亮！" << std::endl;
	}
	void turnOff(){
                writeToFile(gpio_path + "/value", "1");
                std::cout << "[硬件驱动] 物理 LED 灯已熄灭！" << std::endl;
        }

};

LedController* myled = nullptr;


// --- 武器 3：核心【异步回调函数】 ---
//  当底层网络线程收到云端发来的数据时，会自动触发这个函数！

void on_message_received(struct mosquitto *mosq, void *userdata, const struct mosquitto_message  *message){
	// 1. 将收到的原始字节流转换为 C++ 字符串
	std::string payload((char*)message->payload, message->payloadlen);
	std::cout << "\n[异步中断] 收到云端指令，主题：" << message->topic << std::endl;
	std::cout << "[原始报文]: " << payload << std::endl;

	// 2. 现代 C++ 的高阶用法：异常捕获与 DOM 解析
	try {
		// 瞬间将字符串反序列化为极其聪明的 JSON 字典树
		auto j = json::parse(payload);

		// 像查字典一样安全的提取指令！
		if (j.contains("cmd") && j["cmd"] == "led"){
			int status = j["status"];
			if (status == 1 && myled != nullptr){
				myled->turnOn();
			}else if(status == 0){
				myled->turnOff();
			}
		}
	} catch(json::parse_error& e){
		std::cerr << "[解析异常] 脏数据拦截：" << e.what() << std::endl;
	}
}
int main() {
    std::cout << "=== Orion Edge Gateway V2.0 (Bi-directional & Hardware Sync) ===" << std::endl;

    // 实例化硬件外设
    // 这里我们动态分配内存，确保程序结束时能触发析构函数释放引脚
    myled = new LedController(131);

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("Orion_Gateway_V2", true, NULL);
    mosquitto_message_callback_set(mosq, on_message_received);

    if (mosquitto_connect(mosq, "192.168.0.129", 1883, 60) != MOSQ_ERR_SUCCESS) {
        return -1;
    }

    mosquitto_subscribe(mosq, NULL, "Orion/Gateway/Command", 0);
    mosquitto_loop_start(mosq);

    // 主线程：温度上报
    for (int i = 0; i < 60; i++) {
        float temp = getRealCpuTemperature();
        json send_data = {
            {"device_id", "i.MX6ULL"},
            {"cpu_temp", temp},
            {"status", "online"}
        };
        std::string json_str = send_data.dump();
        mosquitto_publish(mosq, NULL, "Orion/Gateway/Temp", json_str.length(), json_str.c_str(), 0, false);
        sleep(2);
    }

    // 优雅退出
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    // 销毁硬件控制对象，触发 unexport
    delete myled;

    return 0;
}
