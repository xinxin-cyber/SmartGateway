#pragma once

#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdexcept>

namespace sg {
namespace ipc {

class IpcServer {
private:
    std::string socket_path_;    // UDS 在本地文件系统中的路径 (例如 /tmp/cv_gateway.sock)
    int server_fd_;              // 监听连接的主 FD
    int client_fd_;              // 与视觉进程通信的专属 FD
    struct sockaddr_un server_addr_;

public:
    // 构造函数：指明要在哪个路径下建立通讯隧道
    explicit IpcServer(const std::string& socket_path);
    
    // 析构函数：释放 FD 并清理残留的 sock 文件
    ~IpcServer();

    // 1. 系统调用三连：socket() -> bind() -> listen()
    bool init();

    // 2. 接受视觉进程的连接 (非阻塞设计：仅当 epoll 提醒 server_fd_ 可读时才调用！)
    bool acceptClient();

    // 3. 向视觉进程发送 JSON 控制指令 (例如曝光度调整)
    bool sendCommand(const std::string& cmd_json);

    // 4. 读取视觉进程发来的状态回执
    std::string readMessage();

    // ==========================================
    // 🌟 架构师的后门：向 Reactor 暴露底层 FD
    // ==========================================
    int getServerFd() const { return server_fd_; }
    int getClientFd() const { return client_fd_; }
    
    // 优雅地断开当前客户端，准备迎接下一次连接
    void disconnectClient();
};

} // namespace ipc
} // namespace sg
