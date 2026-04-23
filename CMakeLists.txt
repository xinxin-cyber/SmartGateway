# ==============================
# 基础配置
# ==============================

cmake_minimum_required(VERSION 3.10)
add_compile_options(-g -O0)
project(SmartGateway)
add_compile_options(-pthread)

# 严格执行现代 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局头文件路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# ==============================
# 第三方 SDK 路径（统一管理区）
# ==============================
set(MQTT_SDK ${CMAKE_CURRENT_SOURCE_DIR}/mqtt_sdk)
set(OPENCV_SDK ${CMAKE_CURRENT_SOURCE_DIR}/opencv_sdk)

# 1. 挂载 MQTT 依赖
include_directories(${MQTT_SDK}/include)
link_directories(${MQTT_SDK}/lib)

# 2. 挂载 OpenCV-mobile 依赖 (V2.0 视觉引擎准备)
# OpenCV 的规范：头文件通常在 include/opencv4 目录下
include_directories(${OPENCV_SDK}/include/opencv4)
include_directories(${OPENCV_SDK}/include)
link_directories(${OPENCV_SDK}/lib)

# ==============================
# 系统级依赖库
# ==============================
find_package(Threads REQUIRED)

# ==============================
# 可执行文件 (V1.0 历史资产与网关主程序)
# ==============================

# 主网关 (早期版本)
add_executable(gateway_app src/main.cpp src/CpuSensor.cpp src/LightSensor.cpp src/EepromStorage.cpp)
target_link_libraries(gateway_app Threads::Threads)

# 帧缓冲测试
add_executable(fb_test src/fb_test.cpp)

# UI 网关
add_executable(gateway_ui src/main_ui.cpp src/CpuSensor.cpp src/LightSensor.cpp src/gui.cpp)
target_link_libraries(gateway_ui Threads::Threads)

# 网络通信测试
add_executable(tcp_client src/tcp_client.cpp)
add_executable(mqtt_client src/mqtt_client.cpp)

target_link_libraries(mqtt_client mosquitto)

# 🌟 V1.0 终极融合版：UI + 传感器 + MQTT
add_executable(fusion_app src/main_fusion.cpp src/CpuSensor.cpp src/LightSensor.cpp src/gui.cpp src/EepromStorage.cpp)
target_link_libraries(fusion_app Threads::Threads mosquitto)


# ==============================
# 可执行文件 (V2.0 架构探索与视觉测试)
# ==============================

# 🚀 架构测试：多进程与共享内存 IPC (零拷贝通信)
# 因为没用到外部库，直接编译即可
add_executable(ipc_test src/ipc_test.cpp)

# 👁️ 视觉测试：OpenCV-mobile 离线渲染引擎
# 必须链接 OpenCV 的核心计算库和图像处理库
add_executable(cv_test src/cv_test.cpp)
add_executable(cv_test1 src/cv_test1.cpp)
add_executable(cv_gateway src/cv_gateway)
set(OPENCV_LIBS
    # 1. 强制捆绑 OpenCV 静态库族群，无视链接顺序
    -Wl,--start-group
    opencv_highgui
    opencv_imgproc
    opencv_core
    -Wl,--end-group

    # 2. 补齐 Linux 底层系统级依赖
    Threads::Threads # 多线程支持 (pthread)
    dl               # 动态加载器支持 (解决 dladdr 报错)
    m                # 基础数学库 (解决 softdouble 报错)
    rt               # 实时扩展库 (处理高精度时钟等)
    stdc++           # 强行补充 C++ 标准库
    gomp
)

target_link_libraries(cv_test ${OPENCV_LIBS})
target_link_libraries(cv_test1 ${OPENCV_LIBS})
target_link_libraries(cv_gateway ${OPENCV_LIBS})

