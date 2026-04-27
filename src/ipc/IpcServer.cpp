#include "IpcServer.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>

namespace sg {
namespace ipc {

IpcServer::IpcServer(const std::string& socket_path)
    : socket_path_(socket_path), server_fd_(-1), client_fd_(-1) {
    memset(&server_addr_, 0, sizeof(server_addr_));
}

IpcServer::~IpcServer() {
    disconnectClient();
    if (server_fd_ != -1) {
        close(server_fd_);
        server_fd_ = -1;
    }
    // 终极清场：析构时必须拔除系统中的 sock 文件残留
    unlink(socket_path_.c_str());
}

bool IpcServer::init() {
    // 1. 申请 AF_UNIX 本地协议族的流式套接字
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[IpcServer] ❌ socket() 创建失败: " << strerror(errno) << std::endl;
        return false;
    }

    // 🌟 架构师操作 1：设置监听 FD 为非阻塞模式 (Reactor 的铁律)
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

    // 🌟 架构师操作 2：绑定前，先无脑斩杀可能残留的历史文件
    unlink(socket_path_.c_str());

    // 2. 绑定本地物理路径
    server_addr_.sun_family = AF_UNIX;
    strncpy(server_addr_.sun_path, socket_path_.c_str(), sizeof(server_addr_.sun_path) - 1);

    if (bind(server_fd_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
        std::cerr << "[IpcServer] ❌ bind() 绑定失败: " << strerror(errno) << std::endl;
        return false;
    }

    // 3. 开启监听 (排队积压上限设为 5)
    if (listen(server_fd_, 5) < 0) {
        std::cerr << "[IpcServer] ❌ listen() 监听失败: " << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "[IpcServer] ✅ UDS 神经中枢已就绪，正在监听: " << socket_path_ << std::endl;
    return true;
}

bool IpcServer::acceptClient() {
    // 当 epoll 提醒我们 server_fd_ 响了，立刻来接客
    int new_fd = accept(server_fd_, nullptr, nullptr);
    if (new_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "[IpcServer] ❌ accept() 异常: " << strerror(errno) << std::endl;
        }
        return false;
    }

    // 如果之前有视觉进程连着（比如视觉进程崩溃重启了），先强行踢掉旧的
    disconnectClient();
    
    client_fd_ = new_fd;

    // 同样，把通信管道也设为非阻塞
    int flags = fcntl(client_fd_, F_GETFL, 0);
    fcntl(client_fd_, F_SETFL, flags | O_NONBLOCK);

    std::cout << "[IpcServer] ⚡ 视觉进程已接入！通信管道 FD: " << client_fd_ << std::endl;
    return true;
}

bool IpcServer::sendCommand(const std::string& cmd_json) {
    if (client_fd_ == -1) return false; // 还没人连进来
    
    // UDS 在本地内存中极快，直接 write 即可
    ssize_t bytes_written = write(client_fd_, cmd_json.c_str(), cmd_json.length());
    if (bytes_written < 0 && errno != EAGAIN) {
        std::cerr << "[IpcServer] ⚠️ 发送指令失败，视觉进程可能已掉线。" << std::endl;
        disconnectClient();
        return false;
    }
    return true;
}

std::string IpcServer::readMessage() {
    if (client_fd_ == -1) return "";

    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_read = read(client_fd_, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        return std::string(buffer);
    } else if (bytes_read == 0) {
        std::cout << "[IpcServer] ⚠️ 视觉进程主动断开了连接。" << std::endl;
        disconnectClient();
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "[IpcServer] ❌ 读取异常: " << strerror(errno) << std::endl;
        disconnectClient();
    }
    return "";
}

void IpcServer::disconnectClient() {
    if (client_fd_ != -1) {
        close(client_fd_);
        client_fd_ = -1;
    }
}

} // namespace ipc
} // namespace sg
