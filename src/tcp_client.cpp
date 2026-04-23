#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// --- 武器库：读取开发板真实的 CPU 温度 ---
float getRealCpuTemperature(){
	std::ifstream tempFile("/sys/class/thermal/thermal_zone0/temp");
	if (!tempFile.is_open()){
		return -1.0f;
	}
	int raw_temp;
	tempFile >> raw_temp;
	tempFile.close();
	return raw_temp / 1000.0f; //换算成摄氏度
}

int main(){
	std::cout << "=== Orion Edge Gateway -TCP Assault Mode  ===" << std::endl;
	
	// 1. 申请 TCP 文件描述符
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0){
		std::cerr << "Socket creation failed!"<< std::endl;
		return -1;
	}

	// 2. 填目标地址 （字节序转换和 IP 转换）
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr)); // 清空结构体，养成好习惯
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8080); // 转换为大端口网络字节序
	inet_pton(AF_INET, "192.168.0.129", &serv_addr.sin_addr); // 转换为 32 位二进制

	// 3. 跨越局域网冲锋
	std::cout << "[Target] 192.168.0.129:8080" << std::endl;
	std::cout << "Connecting to Ubuntu..." << std::endl;
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		std::cerr << "Connection Failed! Is 'nc -lk 8080' running on Ubuntu?" << std::endl;
		close(sock);
		return -1;
	}

	// 4. 疯狂输出物理数据 （连续发送 20 次， 每秒 1 次
	for (int i = 0; i < 20; ++i){
		float curr_temp = getRealCpuTemperature();

		// 使用 snprintf 完美控制格式
		char buffer[128];
		snprintf(buffer, sizeof(buffer), "[Gateway Orion] Real Cpu Temp: %.1f C\n", curr_temp);
		std::string payload = buffer;

		// 顺着 TCP 隧道发送
		send(sock, payload.c_str(), payload.length(), 0);
		std::cout << "Data sent: " << curr_temp << " C" << std::endl;

		sleep(1);
	}

	// 5. 任务结束,销毁隧道
	close(sock);
	std::cout << "Mission Accomplished.Connection closed." << std::endl;
	return 0;
}
