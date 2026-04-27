#pragma once

#include <sys/shm.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>

namespace sg {
namespace ipc {

class SharedMemory {
private:
    int shm_id_;        // 共享内存标识符
    void* shm_addr_;    // 内存挂载后的起始首地址
    int ev_fd_;         // 唤醒 Reactor 的 eventfd
    size_t size_;       // 申请的大小

public:
    // 构造函数：size 通常设为 640*480*3 (约 921,600 字节)
    explicit SharedMemory(size_t size);
    ~SharedMemory();

    // 初始化：创建或获取共享内存，并创建 eventfd
    bool init(int key_id);

    // 获取内存首地址，供视觉进程 memcpy 图像
    void* getAddr() const { return shm_addr_; }

    // 获取 eventfd，供主进程挂载到 epoll
    int getEventFd() const { return ev_fd_; }

    // 视觉进程调用：写完图，敲铃
    void notify();

    // 主进程调用：听到铃声，重置铃声状态
    void clearNotify();
};

} // namespace ipc
} // namespace sg
