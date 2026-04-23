#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
int main() {
    std::cout << "=== Orion Gateway: 边缘视觉引擎初始化测试 ===" << std::endl;

    // 1. 我们没有摄像头，就让 OpenCV 自己在内存里“无中生有”画一张图
    // 创建一个 800x600 的黑色画板 (CV_8UC3 代表 8位无符号整型，3通道RGB)
    cv::Mat image = cv::Mat::zeros(600, 800, CV_8UC3);

    // 2. 在画面中间画一个绿色的矩形框 (模拟目标检测的 Bounding Box)
    cv::Rect targetBox(200, 150, 400, 300);
    cv::rectangle(image, targetBox, cv::Scalar(0, 255, 0), 3); // BGR颜色空间，绿色

    // 3. 在图片上写一行字
    cv::putText(image, "Orion Vision Engine V2.0", cv::Point(220, 130), 
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);

    // 4. 将这张在内存中渲染出来的图片，保存到你的 Linux 文件系统里
    std::string outputPath = "vision_output.jpg";
    bool success = cv::imwrite(outputPath, image);

    if (success) {
        std::cout << "[成功] 图像已渲染并保存到: " << outputPath << std::endl;
        std::cout << "请使用 MobaXterm 或 SCP 将图片拉到 Windows 上查看！" << std::endl;
    } else {
        std::cerr << "[失败] OpenCV 图像保存失败！" << std::endl;
    }

    return 0;
}
