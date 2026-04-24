// src/core/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>

// src/core/Channel.cpp
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>

namespace sg {
namespace core {

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0) {}

Channel::~Channel() {}

void Channel::handleEvent() {
    // 1. 处理错误事件或挂起 (HUP)
    if (revents_ & (EPOLLERR | EPOLLHUP)) {
        if (errorCallback_) errorCallback_();
    }

    // 2. 处理可读事件 (包括对端关闭连接的 RDHUP)
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_();
    }

    // 3. 处理可写事件
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }
}

void Channel::enableReading() {
    events_ |= EPOLLIN;
    update();
}

void Channel::disableReading() {
    events_ &= ~EPOLLIN;
    update();
}

void Channel::disableAll() {
    events_ = 0;
    update();
}

void Channel::update() {
    // 将状态同步到所挂载的 EventLoop
    loop_->updateChannel(this);
}

} // namespace core
} // namespace sg
