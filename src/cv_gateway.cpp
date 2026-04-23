#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>      
#include <sys/wait.h>     
#include <linux/videodev2.h>
#include <string.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// 【拼图 1】：我们刚刚跑通的共享内存蓝图
struct SharedVisionData {
    volatile int is_ready;              
    int frame_size;                     
    unsigned char payload[512 * 1024];  
};

int main() {
    std::cout << "========== V2.0 双核高并发视觉网关启动 ==========" << std::endl;

    // 【拼图 2】：向内核批地、铺路、拿指针
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedVisionData), IPC_CREAT | 0666);
    SharedVisionData* shm_data = (SharedVisionData*)shmat(shmid, NULL, 0);
    shm_data->is_ready = 0; 

    // 【拼图 3】：裂变出父子双进程！
    pid_t pid = fork();

    if (pid > 0) { 
        // ==========================================
        // 👷 父进程：极其冷酷的高速采集员 (只抓图，不解码)
        // ==========================================
        
        // 【拼图 4】：你烂熟于心的 V4L2 初始化与内存映射 (mmap)
        int fd = open("/dev/video1", O_RDWR);
        struct v4l2_format fmt = {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = 640; fmt.fmt.pix.height = 480;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        ioctl(fd, VIDIOC_S_FMT, &fmt);

        struct v4l2_requestbuffers req = {};
        req.count = 1; req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory = V4L2_MEMORY_MMAP;
        ioctl(fd, VIDIOC_REQBUFS, &req);

        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory = V4L2_MEMORY_MMAP; buf.index = 0;
        ioctl(fd, VIDIOC_QUERYBUF, &buf);

        void* buffer_start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        
        ioctl(fd, VIDIOC_QBUF, &buf);
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd, VIDIOC_STREAMON, &type);

        int grab_count = 0;
        while (grab_count < 10) { 
            ioctl(fd, VIDIOC_DQBUF, &buf); // 接水
            
            // 【神来之笔】：你刚才自己加的强制刷缓存！
            msync(buffer_start, buf.bytesused, MS_SYNC | MS_INVALIDATE); 

            // 核心并发逻辑：把真实画面塞进刚才跑通的共享内存里！
            if (shm_data->is_ready == 0) {
                memcpy(shm_data->payload, buffer_start, buf.bytesused); 
                shm_data->frame_size = buf.bytesused;
                shm_data->is_ready = 1; // 亮红灯，通知儿子来读
		// ✅ 【终极修复】只有成功送达，才算数！
                grab_count++; 
                std::cout << "[父进程] 成功送达第 " << grab_count << "/10 帧！" << std::endl;
            } else {
                // 儿子来不及吃，丢弃此帧，但绝不增加 grab_count！
                std::cerr << "[父进程] 儿子太慢了，直接丢弃一帧以保命！" << std::endl;
            }

            ioctl(fd, VIDIOC_QBUF, &buf); 
            usleep(30000); 
        }

        // 父进程收尾 (关水管，拆路，炸毁内存)
        ioctl(fd, VIDIOC_STREAMOFF, &type);
        munmap(buffer_start, buf.length);
        close(fd);
        wait(NULL); 
        shmdt(shm_data); 
        shmctl(shmid, IPC_RMID, NULL); 
        std::cout << "[父进程] V4L2 引擎关闭，完美收工！" << std::endl;
    } 
    else if (pid == 0) { 
        // ==========================================
        // 🧠 子进程：慢条斯理的视觉处理中心
        // ==========================================
        int process_count = 0;
        while (process_count < 10) {
            // 死死盯住信号灯
            if (shm_data->is_ready == 1) {
                // 有货了！赶紧拷贝到本地，然后立刻给父亲放行 (恢复 0)
                unsigned char local_buffer[512 * 1024];
                int local_size = shm_data->frame_size;
                memcpy(local_buffer, shm_data->payload, local_size);
                shm_data->is_ready = 0; 

                // 【拼图 5】：你的 OpenCV 内存直读解码逻辑！
                cv::Mat raw_data(1, local_size, CV_8UC1, local_buffer);
                cv::Mat frame = cv::imdecode(raw_data, 1);

                if (!frame.empty()) {
                    std::string filename = "gateway_img_" + std::to_string(process_count) + ".jpg";
                    cv::imwrite(filename, frame);
                    std::cout << "  ---> [子进程] 成功解码并保存: " << filename << std::endl;
                }
                process_count++;
            } else {
                usleep(5000); // 没货的时候休眠，不抢占 CPU
            }
        }
        shmdt(shm_data); // 儿子拆路
        std::cout << "  ---> [子进程] OpenCV 引擎关闭，光荣退役！" << std::endl;
    }

    return 0;
}
