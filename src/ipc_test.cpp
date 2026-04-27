// src/ipc_test.cpp
#include "ipc/IpcServer.h"
#include <iostream>
#include <unistd.h>

using namespace sg::ipc;

int main() {
    std::cout << "=== UDS 通信隔离测试 ===" << std::endl;
    
    // 1. 创建服务器实例，绑定物理路径
    IpcServer server("/tmp/orion_cv.sock");
    
    if (!server.init()) {
        std::cerr << "初始化失败，退出测试。" << std::endl;
        return -1;
    }

    std::cout << "等待视觉进程 (Client) 接入..." << std::endl;

    // 暴力轮询测试（仅限测试用，正式环境绝对是用 epoll）
    while (true) {
        // 如果当前没有客户端连接，就尝试去接客
        if (server.getClientFd() == -1) {
            server.acceptClient(); 
        } 
        // 如果连上了，就尝试读消息
        else {
            std::string msg = server.readMessage();
            if (!msg.empty()) {
                std::cout << "\n[主进程收到] -> " << msg;
                
                // 收到消息后，立刻给视觉进程回发一条控制指令
                std::string ack = "{\"cmd\": \"exposure\", \"val\": 50}\n";
                server.sendCommand(ack);
                std::cout << "[主进程下发] -> " << ack;
            }
        }
        
        usleep(100000); // 睡 100ms 防 CPU 跑满
    }

    return 0;
}
