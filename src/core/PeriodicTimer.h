// src/core/PeriodicTimer.h
#pragma once
#include "Channel.h"
#include <functional>

namespace sg {
namespace core {

class EventLoop;

class PeriodicTimer {
public:
    using TimerCallback = std::function<void()>;

    // interval_ms: 循环周期（毫秒）
    PeriodicTimer(EventLoop* loop, uint32_t interval_ms, TimerCallback cb);
    ~PeriodicTimer();

    // 启动/停止定时器
    void start();
    void stop();

private:
    void handleRead(); // 处理 timerfd 可读事件（超时）

    EventLoop* loop_;
    int timerFd_;
    uint32_t intervalMs_;
    TimerCallback callback_;
    Channel timerChannel_; // 每一个定时器都自带一个 Channel
};

} // namespace core
} // namespace sg
