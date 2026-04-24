// src/core/EventLoop.cpp
#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <stdexcept>
#include <cstdio>
#include <cerrno>

namespace sg {
namespace core {

EventLoop::EventLoop()
    : epollFd_(::epoll_create1(EPOLL_CLOEXEC)),
      quit_(false),
      activeEvents_(kInitEventListSize) {
    if (epollFd_ < 0) {
        perror("EventLoop epoll_create1 failed");
        // 如果 epoll 创建失败，整个架构失去依托，直接抛异常让进程结束
        throw std::runtime_error("epoll_create1 failed");
    }
}

EventLoop::~EventLoop() {
    ::close(epollFd_);
}

void EventLoop::loop() {
    quit_ = false;
    while (!quit_) {
        // 阻塞等待事件，超时设为 -1 (永久挂起，直到有事件唤醒，彻底让出 CPU)
        int numEvents = ::epoll_wait(epollFd_, activeEvents_.data(),
                                     static_cast<int>(activeEvents_.size()), -1);
        if (numEvents > 0) {
            // 自动扩容：如果一次性涌入的事件达到了当前数组上限，翻倍扩容防爆
            if (numEvents == static_cast<int>(activeEvents_.size())) {
                activeEvents_.resize(activeEvents_.size() * 2);
            }
            
            // 遍历所有被激活的事件
            for (int i = 0; i < numEvents; ++i) {
                // 【核心黑魔法】：把 void* 指针强转回我们当初塞进去的 C++ 对象
                Channel* channel = static_cast<Channel*>(activeEvents_[i].data.ptr);
                channel->setRevents(activeEvents_[i].events);
                channel->handleEvent(); // 扣动扳机
            }
        } else if (numEvents == 0) {
            // timeout (当前设为 -1，理论上不会到达这里)
        } else {
            // 系统调用被信号打断 (EINTR) 是正常现象，其他报错则打印
            if (errno != EINTR) {
                perror("EventLoop epoll_wait error");
            }
        }
    }
}

void EventLoop::quit() {
    quit_ = true;
}

void EventLoop::updateChannel(Channel* channel) {
    struct epoll_event event;
    event.events = channel->events();
    event.data.ptr = channel; // 绑定对象上下文
    
    int fd = channel->fd();
    auto it = channelMap_.find(fd);
    
    if (it == channelMap_.end()) {
        // 字典里没有，说明是新生效的 FD (EPOLL_CTL_ADD)
        channelMap_[fd] = channel;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &event) < 0) {
            perror("EventLoop epoll_ctl ADD error");
        }
    } else {
        // 字典里有，说明是修改关注事件 (EPOLL_CTL_MOD)
        if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("EventLoop epoll_ctl MOD error");
        }
    }
}

void EventLoop::removeChannel(Channel* channel) {
    int fd = channel->fd();
    if (channelMap_.erase(fd) > 0) {
        // 彻底从内核监听树上摘除 (EPOLL_CTL_DEL)
        if (::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            perror("EventLoop epoll_ctl DEL error");
        }
    }
}

} // namespace core
} // namespace sg
