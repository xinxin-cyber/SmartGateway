#include "core/EventLoop.h"
#include "core/PeriodicTimer.h"
#include "CpuSensor.h"
#include "LightSensor.h"
#include "EepromStorage.h"
#include "gui.h"

#include <iostream>
#include <csignal>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;
using namespace sg::core;

// ==========================================
// 全局资源与控制
// ==========================================
EventLoop* g_loop = nullptr;

// V2.0 去掉了 std::mutex！因为所有业务都在单一主线程顺次执行，天然无锁！
struct SensorData {
    float cpu_temp = 0.0f;
    int ambient_light = 0;
} globalData;

std::string globalDeviceID = "Unknown_Device";

void signalHandler(int signum) {
    std::cout << "\n[系统信号] 捕捉到信号 (" << signum << ")，通知 epoll 安全退出..." << std::endl;
    if (g_loop) g_loop->quit();
}

// --- 保留你的 LED 控制器 ---
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
    ~LedController() { writeToFile("/sys/class/gpio/unexport", gpio_num); }
    void turnOn() { writeToFile(gpio_path + "/value", "0"); std::cout << "💡 LED 已点亮！" << std::endl; }
    void turnOff() { writeToFile(gpio_path + "/value", "1"); std::cout << "🌑 LED 已熄灭！" << std::endl; }
};

LedController* myled = nullptr;

// MQTT 下行控制回调 (运行在 mosquitto 内部线程)
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

// --- 保留你的 UI 底层 ---
void drawRectangle(int *pixel_ptr, int xres, int start_x, int start_y, int width, int height, int color) {
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            pixel_ptr[y * xres + x] = color;
        }
    }
}

// ==========================================
// 主程序入口
// ==========================================
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    std::cout << "=== 🚀 Orion SmartGateway V2.0 (epoll 驱动版) ===" << std::endl;

    // 1. 系统自检 (EEPROM) - 完美保留 V1.5 逻辑
    EepromStorage eeprom;
    if (eeprom.init()) {
        std::string readID = eeprom.readString(0x00, 16);
        if (readID.empty() || readID[0] == '\xff' || readID[0] == '\0') {
            globalDeviceID = "Orion_Node_007";
            eeprom.writeString(0x00, globalDeviceID);
        } else {
            globalDeviceID = readID;
        }
        std::cout << "[系统自检] 设备身份确认: " << globalDeviceID << std::endl;
    }

    // 2. 硬件初始化
    CpuSensor cpuSensor;
    LightSensor lightSensor;
    cpuSensor.init();
    lightSensor.init();
    myled = new LedController(131);

    // 3. 显存挂载
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { std::cerr << "无法打开显存!" << std::endl; return -1; }
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    long screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    int *pixel_ptr = (int *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    // 刷初始背景
    drawRectangle(pixel_ptr, vinfo.xres, 0, 0, vinfo.xres, vinfo.yres, 0x00000000);
    drawRectangle(pixel_ptr, vinfo.xres, 95, 195, 610, 60, 0x00333333); 
    drawRectangle(pixel_ptr, vinfo.xres, 95, 295, 110, 110, 0x00333333); 

    // 4. MQTT 挂载 (利用 mosquitto 自己的后台线程收消息)
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("Orion_Fusion_Node", true, NULL);
    mosquitto_message_callback_set(mosq, on_message_received);
    mosquitto_connect(mosq, "192.168.0.129", 1883, 60);
    mosquitto_subscribe(mosq, NULL, "Orion/Gateway/Command", 0);
    mosquitto_loop_start(mosq); // 启动 Mosquitto 内部网络循环

    try {
        // 🌟 核心：创建 Reactor 事件循环
        EventLoop loop;
        g_loop = &loop;

        // 🌟 将旧的 While 循环拆解为 Timer 回调挂载到 epoll 上

        // 任务 A: 传感器采集 (500ms)
        PeriodicTimer sensorTimer(&loop, 500, [&]() {
            globalData.cpu_temp = cpuSensor.getTemperature();
            globalData.ambient_light = lightSensor.getLightIntensity();
            // 无锁！因为下面 UI 和 MQTT 的读取也在同一个线程按顺序执行！
        });

        // 任务 B: UI 刷屏 (100ms)
        PeriodicTimer uiTimer(&loop, 100, [&]() {
            int temp_width = (int)((globalData.cpu_temp - 30.0f) / 50.0f * 600);
            if (temp_width < 0) temp_width = 0;
            if (temp_width > 600) temp_width = 600;
            
            int temp_color = (globalData.cpu_temp > 65.0f) ? 0x00FF0000 : 0x0000FF00; 
            drawRectangle(pixel_ptr, vinfo.xres, 100, 200, 600, 50, 0x00000000);
            drawRectangle(pixel_ptr, vinfo.xres, 100, 200, temp_width, 50, temp_color); 
            
            char temp_buf[16];
            snprintf(temp_buf, sizeof(temp_buf), "%.1f", globalData.cpu_temp);
            std::string temp_str = "Temp: " + std::string(temp_buf) + " C";
            drawString(pixel_ptr, vinfo.xres, 100, 170, temp_str, 0x00FFFFFF);

            int light_color = (globalData.ambient_light > 50) ? 0x00FFD700 : 0x00555555;
            drawRectangle(pixel_ptr, vinfo.xres, 100, 300, 100, 100, light_color); 
        });

        // 任务 C: MQTT 数据上报 (2000ms)
        PeriodicTimer mqttTimer(&loop, 2000, [&]() {
            json send_data = {
                {"device_id", globalDeviceID},
                {"cpu_temp", globalData.cpu_temp},
                {"light_lux", globalData.ambient_light}
            };
            std::string json_str = send_data.dump();
            mosquitto_publish(mosq, NULL, "Orion/Gateway/Telemetry", json_str.length(), json_str.c_str(), 0, false);
        });

        // 引擎点火
        sensorTimer.start();
        uiTimer.start();
        mqttTimer.start();

        std::cout << "[EventLoop] Reactor 引擎启动，接管进程..." << std::endl;
        loop.loop(); // 主线程将阻塞在此，处理所有的 timerfd 事件

    } catch (const std::exception& e) {
        std::cerr << "❌ 严重错误: " << e.what() << std::endl;
    }

    // ⬇️ 当接收到 Ctrl+C 退出 EventLoop 后，优雅清场 ⬇️
    std::cout << "\n[系统清理] 正在释放资源..." << std::endl;
    
    // 清理 MQTT
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    // 清理硬件与显存
    delete myled;
    munmap(pixel_ptr, screensize);
    close(fb_fd);

    std::cout << "=== V2.0 进程已完美退出 ===" << std::endl;
    return 0;
}
