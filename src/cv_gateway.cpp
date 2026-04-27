#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/videodev2.h>
#include <string.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;
using namespace cv;

int main() {
    cout << "========== 👁️ Orion Vision Engine V2.0 (全功能自愈版) ==========" << endl;

    // --- 1. 资源接管：带重试机制的“蹭网”逻辑 ---
    int shmid = -1;
    void* shm_addr = nullptr;
    int fifo_fd = -1;
    int uds_fd = -1;

    while (true) {
        shmid = shmget((key_t)0x1234, 1228800, 0666);
        fifo_fd = open("/tmp/orion_cv.fifo", O_WRONLY | O_NONBLOCK);
        
        // 尝试连接主网关的 UDS 控制神经
        uds_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/tmp/orion_cv.sock", sizeof(addr.sun_path) - 1);

        if (shmid >= 0 && fifo_fd >= 0 && connect(uds_fd, (struct sockaddr*)&addr, sizeof(addr)) >= 0) {
            shm_addr = shmat(shmid, NULL, 0);
            fcntl(uds_fd, F_SETFL, O_NONBLOCK); // 设为非阻塞，防止听指令时卡死抓图
            cout << "✅ IPC 全链路贯通！主网关已握手。" << endl;
            break;
        }

        cout << "⌛ 等待主网关 (fusion_app) 启动... 1秒后重试" << endl;
        if (uds_fd >= 0) close(uds_fd);
        if (fifo_fd >= 0) close(fifo_fd);
        sleep(1);
    }

    // --- 2. V4L2 硬件驱动初始化 (保持你硬核的 V4L2 逻辑) ---
    int v_fd = open("/dev/video1", O_RDWR);
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640; fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    ioctl(v_fd, VIDIOC_S_FMT, &fmt);

    struct v4l2_requestbuffers req = {};
    req.count = 1; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
    ioctl(v_fd, VIDIOC_REQBUFS, &req);

    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory = V4L2_MEMORY_MMAP; buf.index = 0;
    ioctl(v_fd, VIDIOC_QUERYBUF, &buf);

    void* buffer_start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, v_fd, buf.m.offset);
    ioctl(v_fd, VIDIOC_QBUF, &buf);
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v_fd, VIDIOC_STREAMON, &type);

    // --- 3. 核心循环：抓图 + 听指令 ---
    Mat frame_bgra, frame;
    char notify_sig = '1';
    char uds_buf[1024];

    while (true) {
        // A. 抓图与泵送 (数据流)
        ioctl(v_fd, VIDIOC_DQBUF, &buf);
        msync(buffer_start, buf.bytesused, MS_SYNC | MS_INVALIDATE);
        Mat raw_data(1, buf.bytesused, CV_8UC1, buffer_start);
        frame = imdecode(raw_data, 1);

        if (!frame.empty()) {
            cvtColor(frame, frame_bgra, COLOR_BGR2BGRA);
            memcpy(shm_addr, frame_bgra.data, 1228800);
            write(fifo_fd, &notify_sig, 1);
        }
        ioctl(v_fd, VIDIOC_QBUF, &buf);

        // B. 听从调遣 (控制流)
        memset(uds_buf, 0, sizeof(uds_buf));
        ssize_t n = read(uds_fd, uds_buf, sizeof(uds_buf));
        if (n > 0) {
            string cmd(uds_buf);
            cout << "📥 [UDS 收到指令]: " << cmd << endl;
            
            // 逻辑分支：如果是抓拍指令
            if (cmd.find("capture") != string::npos) {
                string filename ="../run_output/snap_" + to_string(time(NULL)) + ".jpg";
                imwrite(filename, frame);
                cout << "📸 已执行抓拍保存: " << filename << endl;
            }
        } else if (n == 0) {
            cerr << "❌ 主网关断开，视觉引擎进入紧急待机..." << endl;
            // 此处可以加入重新连接逻辑，或者直接 exit 由 systemd 重启
            break; 
        }

        usleep(10000); // 100fps 的上限限制
    }

    // 清理资源
    ioctl(v_fd, VIDIOC_STREAMOFF, &type);
    munmap(buffer_start, buf.length);
    close(v_fd);
    shmdt(shm_addr);
    return 0;
}
