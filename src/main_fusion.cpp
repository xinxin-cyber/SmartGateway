#include <iostream>
#include <csignal>
#include <thread>
#include <mutex>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include "EepromStorage.h"
#include <fstream>
// 你的传感器头文件 (如果路径不对，请根据实际情况调整)
#include "CpuSensor.h"
#include "LightSensor.h"
#include "gui.h"

using json = nlohmann::json;

// volatile 告诉编译器：这个变量随时会变，不要自作聪明做优化
// sig_atomic_t 保证在信号处理中读写是原子的
volatile sig_atomic_t keepRunning = 1;

void signalHandler(int signum) {
    std::cout << "\n[系统信号] 捕捉到信号 (" << signum << ")，正在准备安全退出..." << std::endl;
    keepRunning = 0; // 修改标志位，让所有 while(true) 停下来
}

// --- 新增：LED 控制器和反向控制回调 ---
class LedController {
private:
    std::string gpio_num;
    std::string gpio_path;
    void writeToFile(const std::string& path, const std::string& value) {
        std::ofstream file(path);
        if (file.is_open()) { file << value; file.close(); }
    }
public:
    LedController(int pin) {
        gpio_num = std::to_string(pin);
        gpio_path = "/sys/class/gpio/gpio" + gpio_num;
        writeToFile("/sys/class/gpio/export", gpio_num);
        usleep(100000);
        writeToFile(gpio_path + "/direction", "out");
    }
    ~LedController() {
        writeToFile("/sys/class/gpio/unexport", gpio_num);
    }
    void turnOn() { writeToFile(gpio_path + "/value", "0"); std::cout << "💡 LED 已点亮！" << std::endl; }
    void turnOff() { writeToFile(gpio_path + "/value", "1"); std::cout << "🌑 LED 已熄灭！" << std::endl; }
};

LedController* myled = nullptr;

// MQTT 异步回调函数（当中枢发来指令时，这个函数会自动触发）
void on_message_received(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    std::string payload((char*)message->payload, message->payloadlen);
    try {
        auto j = json::parse(payload);
        if (j.contains("cmd") && j["cmd"] == "led" && myled != nullptr) {
            if (j["status"] == 1) myled->turnOn();
            else if (j["status"] == 0) myled->turnOff();
        }
    } catch(json::parse_error& e) {}
}
// ==========================================
// 1. 全局保险箱 (供三个工人共享)
// ==========================================
struct SensorData {
    float cpu_temp;
    int ambient_light;
};

SensorData globalData = {0.0f, 0};
std::mutex dataMutex; // 保护保险箱的锁
std::string globalDeviceID = "Unknown_Device";
// --- UI 底层函数 ---
void drawRectangle(int *pixel_ptr, int xres, int start_x, int start_y, int width, int height, int color) {
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            pixel_ptr[y * xres + x] = color;
        }
    }
}

// ==========================================
// 2. 工人A：采集线程 (在后台跑，绝不卡主界面)
// ==========================================
void dataAcquisitionTask() {
    std::cout << "[工人A] 采集线程启动..." << std::endl;
    CpuSensor cpuSensor;
    LightSensor lightSensor;
    cpuSensor.init();
    lightSensor.init();

    while(keepRunning) {
        float current_temp = cpuSensor.getTemperature();
        int current_light = lightSensor.getLightIntensity();

        // 【开锁】更新保险箱数据
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            globalData.cpu_temp = current_temp;
            globalData.ambient_light = current_light;
        } // 离开括号，自动解锁

        usleep(500000); // 睡 0.5 秒
    }
}

// ==========================================
// 3. 工人C：MQTT 快递员线程 (网络不好也只会卡自己)
// ==========================================
void mqttNetworkTask() {
    std::cout << "[工人C] 网络线程启动，正在连接 MQTT..." << std::endl;
    // 1. 实例化 LED 硬件
    myled = new LedController(131); 

    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("Orion_Fusion_Node", true, NULL);

    // 2. 关键：告诉 Mosquitto，收到消息就去执行 on_message_received
    mosquitto_message_callback_set(mosq, on_message_received);

    if (mosquitto_connect(mosq, "192.168.0.129", 1883, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "[工人C] 警告：无法连接 Broker。" << std::endl;
    }

    // 3. 关键：订阅控制信箱！
    mosquitto_subscribe(mosq, NULL, "Orion/Gateway/Command", 0);

    mosquitto_loop_start(mosq);
    while(keepRunning) {
        SensorData localCopy;
        // 【开锁】飞速抄录保险箱里的数据，然后马上解锁
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            localCopy = globalData;
        }

        // 慢慢打包 JSON 发送 (网络延迟不会卡死 UI 和采集)
        json send_data = {
            {"device_id", globalDeviceID},
            {"cpu_temp", localCopy.cpu_temp},
            {"light_lux", localCopy.ambient_light}
        };
        std::string json_str = send_data.dump();
        
        mosquitto_publish(mosq, NULL, "Orion/Gateway/Telemetry", json_str.length(), json_str.c_str(), 0, false);
        
        sleep(2); // 每 2 秒上报一次
    }
    // ⬇️ 当 keepRunning 变成 0，跳出循环后，开始执行以下清理动作 ⬇️
    std::cout << "[工人C] 正在断开 MQTT 网络连接..." << std::endl;

    // 1. 停止 Mosquitto 内部的后台收发循环
    mosquitto_loop_stop(mosq, true);

    // 2. 礼貌地向 Broker 发送断开连接的请求
    mosquitto_disconnect(mosq);

    // 3. 释放这个 MQTT 实例占用的堆内存
    mosquitto_destroy(mosq);

    // 4. 清理 Mosquitto 库的全局环境
    mosquitto_lib_cleanup();
   // 4. 释放 LED 引脚
    delete myled;
    std::cout << "[工人C] 网络资源已彻底释放，线程安全退出。" << std::endl;
}


// ==========================================
// 4. 工人B：主线程 (UI 画师)
// ==========================================
int main() {
    signal(SIGINT, signalHandler);
    std::cout << "=== Orion Gateway 终极融合版 ===" << std::endl;
    // --- ⬇️ 新增的 EEPROM 启动校验逻辑 ⬇️ ---
    std::cout << "[系统自检] 正在读取 EEPROM 获取设备身份..." << std::endl;
    EepromStorage eeprom;
    if (eeprom.init()) {
        // 从 0x00 地址尝试读取 16 个字节的 ID
        std::string readID = eeprom.readString(0x00, 16);

        // 工业级防呆：新买的 EEPROM 芯片里面可能是全 0xFF 或者乱码
        // 如果第一位不是合法的可见字符，我们就当它是“白片”，强行烧录一个默认 ID
        if (readID.empty() || readID[0] == '\xff' || readID[0] == '\0') {
            std::cout << "[产线模式] 检测到新芯片，正在烧录出厂 MAC 标识..." << std::endl;
            globalDeviceID = "Orion_Node_007"; // 你专属的代号
            eeprom.writeString(0x00, globalDeviceID);
        } else {
            globalDeviceID = readID;
        }
        std::cout << "[系统自检] 设备身份确认: " << globalDeviceID << std::endl;
    } else {
        std::cerr << "[系统自检] EEPROM 挂载失败，将使用默认游侠身份。" << std::endl;
    }

    // 1. 初始化屏幕
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        std::cerr << "无法打开显存!" << std::endl;
        return -1;
    }
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    int *pixel_ptr = (int *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    drawRectangle(pixel_ptr, vinfo.xres, 0, 0, vinfo.xres, vinfo.yres, 0x00000000);
    drawRectangle(pixel_ptr, vinfo.xres, 95, 195, 610, 60, 0x00333333); 
    drawRectangle(pixel_ptr, vinfo.xres, 95, 295, 110, 110, 0x00333333); 

    // 2. 雇佣工人A 和 工人C，让他们去后台干活
    std::thread acqThread(dataAcquisitionTask);

    std::thread mqttThread(mqttNetworkTask);

    // 3. 画师开始疯狂作画
    std::cout << "[工人B] UI 渲染引擎全速启动！" << std::endl;
    while (keepRunning) {
        SensorData localCopy;
        // 【开锁】看一眼数据
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            localCopy = globalData;
        }

        // --- 算宽度，画进度条 ---
        int temp_width = (int)((localCopy.cpu_temp - 30.0f) / 50.0f * 600);
        if (temp_width < 0) temp_width = 0;
        if (temp_width > 600) temp_width = 600;
        
        int temp_color = (localCopy.cpu_temp > 65.0f) ? 0x00FF0000 : 0x0000FF00; 
        drawRectangle(pixel_ptr, vinfo.xres, 100, 200, 600, 50, 0x00000000); // 擦
        drawRectangle(pixel_ptr, vinfo.xres, 100, 200, temp_width, 50, temp_color); // 画新
        
        char temp_buf[16];
        snprintf(temp_buf, sizeof(temp_buf), "%.1f", localCopy.cpu_temp);
        std::string temp_str = "Temp: " + std::string(temp_buf) + " C";
        drawString(pixel_ptr, vinfo.xres, 100, 170, temp_str, 0x00FFFFFF);

        // --- 画光照块 ---
        int light_color = (localCopy.ambient_light > 50) ? 0x00FFD700 : 0x00555555;
        drawRectangle(pixel_ptr, vinfo.xres, 100, 300, 100, 100, light_color); 
        
        usleep(100000); // 睡 0.1 秒，不榨干 CPU
    }

    // ⬇️ 接收到 Ctrl+C，跳出循环，主线程开始清场 ⬇️
    std::cout << "\n[主线程清理] 正在解除显存映射..." << std::endl;
    munmap(pixel_ptr, screensize);
    close(fb_fd);

    // 2. 核心所在：主线程在此阻塞，耐心等待两个子线程干完善后工作再结束！
    std::cout << "[主线程清理] 等待后台线程安全退出..." << std::endl;
    acqThread.join();
    mqttThread.join();

    std::cout << "=== 程序已完全安全退出 ===" << std::endl;
    return 0;
}
