#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>

// 假设这是一帧 1080p 图像的大小 （约2MB）
#define IMAGE_SIZE (1920 * 1080 * 3)

int main(){
	std::cout << "系统 正在向内核申请 2MB 的共享内存..." <<std::endl;

	// 1. 创建共享内存段 （权限 0666 保证可读可写）
	int shm_id = shmget(IPC_PRIVATE, IMAGE_SIZE, IPC_CREAT | 0666);
	if (shm_id < 0){
		std::cerr << "申请共享内存失败！" << std::endl;
		return -1;
	}

	std::cout << "系统 准备执行 fork() 裂变..." << std::endl;

	// 2. 分裂！
	pid_t pid = fork();

	if (pid == 0){
		// ===========================
		// 这里是【子进程】()
		// ===========================
		std::cout << " [子进程-视觉] 启动成功！" << std::endl;

		// 将共享内存挂载到子进程的地址空间
		char* shared_mem = (char*)shmat(shm_id, NULL, 0);

		std::cout<< " [子进程-视觉] 等待从主进程写入数据..." << std::endl;
		sleep(2);

		// 读取共享内存里的内容
		std::cout << " [子进程-视觉] 从共享内存读到指令：" << shared_mem << std::endl;

		// 卸载内存并退出
		shmdt(shared_mem);
		std::cout << " [子进程-视觉] 工作完成，下班。" << std::endl;
	}else if(pid > 0){
		// 父进程
		std::cout << "[父进程-主控] 启动成功！正在挂载内存..." << std::endl;

		char* shared_mem = (char*)shmat(shm_id, NULL,0);

		const char* fake_image_data = "【这是一帧伪造的 2MB 高清图像字节流数据，准备进行 OpenCV 处理】";
		strncpy(shared_mem, fake_image_data, IMAGE_SIZE);

		std::cout << "[父进程-主控] 数据已写入共享内存！" << std::endl;

		shmdt(shared_mem);
		wait(NULL);

		// 销毁共享内存
		shmctl(shm_id, IPC_RMID, NULL);
		std::cout << "[系统] 子进程已回收， 共享内存已销毁，完美退出！" << std::endl;
	}else{
		std::cerr << "fork 失败！" << std::endl;
	}

	
	
	return 0;
}
