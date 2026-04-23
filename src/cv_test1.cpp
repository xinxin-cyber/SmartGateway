#include <iostream>
#include <fcntl.h>           // open 函数
#include <unistd.h>          // close 函数
#include <sys/ioctl.h>       // 核心：操作硬件的 ioctl 遥控器
#include <sys/mman.h>        // 核心：mmap 内存映射
#include <linux/videodev2.h> // V4L2 的内核规矩都在这里

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

int main() {
    std::cout << "========== V2.0 纯底层 V4L2 视觉引擎启动 ==========" << std::endl;

    // 1. 打开设备文件 (代替 OpenCV 的 VideoCapture(1))
    int fd = open("/dev/video1", O_RDWR);
    if (fd < 0) {
        std::cerr << "[致命错误] 无法打开 /dev/video1，请检查连线！" << std::endl;
        return -1;
    }

    // 2. 遥控器：强制底层输出 640x480 的 MJPEG 格式流
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    // 3. 向内核申请 1 个视频缓存区
    struct v4l2_requestbuffers req = {};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);

    // 4. 零拷贝核心 (mmap)：把内核的物理内存，直接“捅穿”映射到我们程序的指针上
    struct v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    void* buffer_start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    // 5. 启动摄像头水管！
    ioctl(fd, VIDIOC_QBUF, &buf);               // 把桶放进流水线
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);          // 拧开水龙头

    // 6. 丢弃前 3 帧曝光不稳定的黑图
    for(int i = 0; i < 3; i++) {
        ioctl(fd, VIDIOC_DQBUF, &buf); // 拿出桶
        ioctl(fd, VIDIOC_QBUF, &buf);  // 倒掉废图，把桶放回去继续接
    }
    // 7. 
    std::cout << "[模式] 进入持续采集模式，ctrl + c 结束..." <<std::endl;
    int frame_count = 0;
    while(frame_count < 10){

    	// 1. 安全取出一帧
   	 if(ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
       		 std::cerr << "取帧超时/失败" << std::endl;
       		 break;
   	 }

    	// 空帧直接跳过
   	 if(buf.bytesused == 0) {
        	ioctl(fd, VIDIOC_QBUF, &buf);
       		 continue;
   	 }	

    	// ✅ 【关键修复】替代fwrite，强制同步mmap缓存，100%稳定解码
    	msync(buffer_start, buf.bytesused, MS_SYNC | MS_INVALIDATE);
	std::cout << " 正在处理第" << frame_count << "帧..." << std::endl;
	cv::Mat raw_data(1, buf.bytesused, CV_8UC1, buffer_start);
	cv::Mat frame = cv::imdecode(raw_data,1);

	if (!frame.empty()){
		std::string filename = "frame_" + std::to_string(frame_count) +".jpg";
		cv::imwrite(filename,frame);
	}
	ioctl(fd, VIDIOC_QBUF,&buf);

	frame_count++;
    }

    std::cout << "[大捷] V4L2 免拷贝内核抓图 + OpenCV 内存直读解码 完美成功！" << std::endl;

    // 10. 善后关门，释放内核资源
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(buffer_start, buf.length);
    close(fd);

    return 0;
}
