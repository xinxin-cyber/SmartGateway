// src/core/PeriodicTimer.cpp
#include "PeriodicTimer.h"
#include "EventLoop.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

namespace sg {
namespace core {

PeriodicTimer::PeriodicTimer(EventLoop* loop, uint32_t interval_ms, TimerCallback cb)
    : loop_(loop),
      timerFd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      intervalMs_(interval_ms),
      callback_(std::move(cb)),
      timerChannel_(loop, timerFd_) {
    
    if (timerFd_ < 0) {
        perror("timerfd_create failed");
    }

    // 设置 Channel 的回调：当 timerfd 可读时调用 handleRead
    timerChannel_.setReadCallback([this]() { this->handleRead(); });
}

PeriodicTimer::~PeriodicTimer() {
    stop();
    ::close(timerFd_);
}

void PeriodicTimer::start() {
    struct itimerspec newValue;
    memset(&newValue, 0, sizeof(newValue));

    // it_value 是第一次超时的时间，不能为 0，否则定时器不启动
    newValue.it_value.tv_sec = intervalMs_ / 1000;
    newValue.it_value.tv_nsec = (intervalMs_ % 1000) * 1000000;

    // it_interval 是后续循环周期
    newValue.it_interval.tv_sec = intervalMs_ / 1000;
    newValue.it_interval.tv_nsec = (intervalMs_ % 1000) * 1000000;

    if (::timerfd_settime(timerFd_, 0, &newValue, nullptr) < 0) {
        perror("timerfd_settime failed");
    }

    timerChannel_.enableReading(); // 告诉 epoll 开始监听这个定时器
}

void PeriodicTimer::stop() {
    struct itimerspec zeroValue;
    memset(&zeroValue, 0, sizeof(zeroValue));
    ::timerfd_settime(timerFd_, 0, &zeroValue, nullptr);
    timerChannel_.disableAll();
}

void PeriodicTimer::handleRead() {
    uint64_t exp;
    ssize_t s = ::read(timerFd_, &exp, sizeof(uint64_t));
    if (s != sizeof(uint64_t)) {
        // 读取字节数不对说明有问题
    }
    
    if (callback_) {
        callback_(); // 执行用户定义的业务逻辑（如传感器采样）
    }
}

} // namespace core
} // namespace sg
