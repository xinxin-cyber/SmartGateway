# 声明要求的最低 CMake 版本
cmake_minimum_required(VERSION 3.10)

# 定义工程名称
project(SmartGateway)

# 告诉编译器去哪里找头文件
include_directories(${PROJECT_SOURCE_DIR}/include)

# 将 src/main.cpp 编译成名为 gateway_cpp 的可执行文件
add_executable(gateway_app src/main.cpp src/CpuSensor.cpp src/LightSensor.cpp src/EepromStorage.cpp)

# === 新增下面两行，支持多线程 ===
find_package(Threads REQUIRED)
target_link_libraries(gateway_app Threads::Threads)

add_executable(fb_test src/fb_test.cpp)

add_executable(gateway_ui src/main_ui.cpp src/CpuSensor.cpp src/LightSensor.cpp src/gui.cpp)
find_package(Threads REQUIRED)
target_link_libraries(gateway_ui Threads::Threads)

