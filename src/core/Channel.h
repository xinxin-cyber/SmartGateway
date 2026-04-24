// src/core/Channel.h
#pragma once
#include <functional>
#include <stdint.h>

namespace sg {
namespace core{

class EventLoop; // 前置声明，避免头文件中包含

class Channel {
public:
	// C++11 回调类型定义
	using EventCallback = std::function<void()>;

	Channel(EventLoop* loop, int fd);
	~Channel();

	// 核心路由函数：由 EventLoop 在 epoll_wait 返回后调用
	void handleEvent();

	// 绑定业务回调 （比如挂载 timerfd 的读取操作，或 I2C 的读取操作）
	void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb);}
	void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
	void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb);}

	// 获取当前 FD 状态
	int fd() const {return fd_;}
	uint32_t events() const {return events_;}
	void setRevents(uint32_t revt) {revents_ = revt;}
	bool isNoneEvent() const {return events_ == 0;}

	// 控制流操作：告知内核我们关注什么事件（EPOLLIN / EPOLLOUT）
	void enableReading();
	void disableReading();
	void disableAll();
private:
	// 内部调用 loop_ ->updateChannel(this) 来更新 epoll 树
	void update();

	EventLoop* loop_;
	const int fd_;
	uint32_t events_;  // 用户关心的事件
	uint32_t revents_; // 内核态返回的事件
	
	EventCallback readCallback_;
	EventCallback writeCallback_;
	EventCallback errorCallback_;
};

}// namespace core
}//namespace sg
