#include "SharedMemory.h"
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace sg {
namespace ipc {

SharedMemory::SharedMemory(size_t size) 
    : shm_id_(-1), shm_addr_(nullptr), ev_fd_(-1), size_(size) {}

SharedMemory::~SharedMemory() {
    if (shm_addr_) shmdt(shm_addr_); // 取消挂载
    if (ev_fd_ != -1) close(ev_fd_);
}

bool SharedMemory::init(int key_id) {
    shm_id_ = shmget((key_t)key_id, size_, IPC_CREAT | 0666);
    if (shm_id_ < 0) return false;

    shm_addr_ = shmat(shm_id_, nullptr, 0);
    if (shm_addr_ == (void*)-1) return false;

    // 🌟 架构师换心：放弃 eventfd，改用 FIFO，完美兼容 epoll！
    const char* fifo_path = "/tmp/orion_cv.fifo";
    unlink(fifo_path); // 先清理残留
    if (mkfifo(fifo_path, 0666) < 0 && errno != EEXIST) {
        std::cerr << "[SHM] ❌ FIFO 创建失败" << std::endl;
        return false;
    }
    
    // 以读写非阻塞模式打开，获取 FD 给 epoll
    ev_fd_ = open(fifo_path, O_RDWR | O_NONBLOCK);
    if (ev_fd_ < 0) return false;

    std::cout << "[SHM] ✅ 1.2MB 视频缓冲区已就绪, FIFO 管道接通！" << std::endl;
    return true;
}
void SharedMemory::notify() {
	char c = '1';
    	write(ev_fd_, &c, 1);
}

void SharedMemory::clearNotify() {
	char c;
    	read(ev_fd_, &c, 1);
}

} // namespace ipc
} // namespace sg
