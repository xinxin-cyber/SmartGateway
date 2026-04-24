// src/core/EventLoop.h
#pragma once
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

namespace sg{
namespace core{

class Channel;

class EventLoop {
public:
	EventLoop();
	~EventLoop();

	// 核心引擎，禁用拷贝与赋值操作，防止误操作导致 epoll fd 泄露或者异常
	EventLoop(const EventLoop&) = delete;
	EventLoop& operator = (const EventLoop&) = delete;

	// 启动心脏跳动（将阻塞在此处）
	void loop();
	// 停止心脏跳动
	void quit();

	// 供 Channel 调用的底层接口（封装 epoll_ctl 的 ADD/MOD/DEL）
	void updateChannel(Channel* channel);
	void removeChannel(Channel* channel);
private:
	int epollFd_;
	bool quit_;

	// epoll_wait 结果缓冲区，使用 vector 方便动态扩容
	std::vector<struct epoll_event> activeEvents_;

	// 维护当前 EventLoop 管理的所有 Channel
	std::unordered_map<int, Channel*> channelMap_;

	static const int kInitEventListSize = 16;
};

} // namespace core
} // namespace sg
