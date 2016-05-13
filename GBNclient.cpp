// GBNclient.cpp : 定义控制台应用程序的入口点。
//

// GBN_client.cpp : 定义控制台应用程序的入口点。 
#include "stdafx.h" 
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h>  
#include <cstdio>
#include <fstream> 
#pragma comment(lib,"ws2_32.lib")  
#define SERVER_PORT 12340 //接收数据的端口号 
#define SERVER_IP  "127.0.0.1" // 服务器的 IP 地址  
const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//接收端序列号个数，为 1~20  
BOOL cl_ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack 
int cl_curSeq;//当前数据包的 seq 
int cl_curAck;//当前等待确认的 ack 
int cl_totalSeq;//收到的包的总数 
int cl_totalPacket;//需要发送的包总数 
const int SEND_WIND_SIZE = 10;
//************************************ 
// Method:    timeoutHandler 
// FullName:  timeoutHandler 
// Access:    public  
// Returns:   void 
// Qualifier: 超时重传处理函数，滑动窗口内的数据帧都要重传 
//************************************ 

void timeoutHandler()//好像没什么用
{
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i< SEND_WIND_SIZE; ++i){
		index = (i + cl_curAck) % SEQ_SIZE;
		cl_ack[index] = TRUE;
	}
	cl_totalSeq -= SEND_WIND_SIZE;
	cl_curSeq = cl_curAck;
}
//************************************ 
// Method:    ackHandler 
// FullName:  ackHandler 
// Access:    public  
// Returns:   void 
// Qualifier: 收到 ack，累积确认，取数据帧的第一个字节 
//由于发送数据时，第一个字节（序列号）为 0（ASCII）时发送失败，因此加一 了，此处需要减一还原 
// Parameter: char c 
//************************************ 
void ackHandler(char c)
{
	if (c == 0)
	{
		printf("client:::server's first package do not have ack!\n");
		return;
	}
	unsigned char index = (unsigned char)c - 1; //序列号减一  
	printf("client:::Recv a ack of %d\n", index + 1);//序号发送方和接收方应该统一，发送的是+1过的，接收方Ack+1过的，在这里打印没必要还原
	if (cl_curAck <= index){
		for (int i = cl_curAck; i <= index; ++i){
			cl_ack[i] = TRUE;
		}
		cl_curAck = (index + 1) % SEQ_SIZE;
	}
	else{   //ack 超过了最大值，回到了 curAck 的左边   
		for (int i = cl_curAck; i< SEQ_SIZE; ++i){
			cl_ack[i] = TRUE;
		}
		for (int i = 0; i <= index; ++i){
			cl_ack[i] = TRUE;
		}
		cl_curAck = index + 1;
	}
}
bool seqIsAvailable()
{
	int step;
	step = cl_curSeq - cl_curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;  //序列号是否在当前发送窗口之内  
	if (step >= SEND_WIND_SIZE){
		return false;
	}
	if (cl_ack[cl_curSeq]){
		return true;
	}
	return false;
}

/****************************************************************/
/*
-time 从服务器端获取当前时间
-quit 退出客户端
-testgbn [X] 测试 GBN 协议实现可靠数据传输
[X] [0,1] 模拟数据包丢失的概率
[Y] [0,1] 模拟 ACK 丢失的概率 */
/****************************************************************/
void printTips(){
	printf("*****************************************\n");
	printf("|     -time to get current time         |\n");
	printf("|     -quit to exit client              |\n");
	printf("|     -testgbn [X] [Y] to test the gbn  |\n");
	printf("*****************************************\n");
}
//************************************ 
// Method:    lossInLossRatio 
// FullName:  lossInLossRatio 
// Access:    public  
// Returns:   BOOL 
// Qualifier: 根据丢失率随机生成一个数字，判断是否丢失,丢失则返回 TRUE，否则返回 FALSE 
// Parameter: float lossRatio [0,1] 
//************************************ 
BOOL lossInLossRatio(float lossRatio){
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound){
		return TRUE;
	}
	return FALSE;
}
int main(int argc, char* argv[]) {
	//加载套接字库（必须）  
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示  
	int err;
	//版本 2.2  
	wVersionRequested = MAKEWORD(2, 2);  //加载 dll 文件 Scoket 库   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0){
		//找不到 winsock.dll   
		printf("WSAStartup failed with error: %d\n", err);
		return 1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else{
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;
	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);  //接收缓冲区  
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//为了测试与服务器的连接，可以使用 -time 命令从服务器端获得当前 时间  
	//使用 -testgbn [X] [Y] 测试 GBN 其中[X]表示数据包丢失概率  
	//          [Y]表示 ACK 丢包概率  
	printTips();  int ret;
	int interval = 1;
	//收到数据包之后返回 ack 的间隔，默认为 1 表示每个都 返回 ack，0 或者负数均表示所有的都不返回 ack  
	char cmd[128];
	float packetLossRatio = 0.2;
	//默认包丢失率 0.2  
	float ackLossRatio = 0.2;
	//默认 ACK 丢失率 0.2  
	//用时间作为随机种子，放在循环的最外面  

	std::ifstream icin;  icin.open("test.txt");
	char data[1024 * 113];  ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * 113);
	icin.close();
	cl_totalPacket = sizeof(data) / 1024;
	for (int i = 0; i < SEQ_SIZE; ++i){
		cl_ack[i] = TRUE;
	}

	srand((unsigned)time(NULL));

	while (true){
		gets_s(buffer);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		//开始 GBN 测试，使用 GBN 协议实现 UDP 可靠文件传输   
		if (!strcmp(cmd, "-testgbn")){
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//状态码    
			unsigned short seq;//包的序列号    
			unsigned short recvSeq;//接收窗口大小为 1，已确认的序列号    
			unsigned short waitSeq;//等待的序列号 
			sendto(socketClient, "-testgbn", strlen(" - testgbn") + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			while (true){
				//等待 server 回复设置 UDP 为阻塞模式     
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage){
				case 0://等待握手阶段      
					u_code = (unsigned char)buffer[0];
					if ((unsigned char)buffer[0] == 205){
						printf("Ready for file transmission\n");
						buffer[0] = 200;
						buffer[1] = '\0';
						sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
						stage = 1;
						recvSeq = 0;
						waitSeq = 1;
						cl_curSeq = 0;
						cl_curAck = 0;
						cl_totalSeq = 0;
					}
					break;
				case 1://等待接收数据阶段
					seq = (unsigned short)buffer[0];
					//随机法模拟包是否丢失      
					b = lossInLossRatio(packetLossRatio);
					if (b){
						printf("The packet with a seq of %d loss\n", seq);
						ackHandler((unsigned short)buffer[1]);
						continue;
					}
					printf("client:::recv a packet with a seq of %d\n", seq);
					//如果是期待的包，正确接收，正常确认即可  接收时同时发送构造数据并发送
					if (waitSeq - seq==0){
						++waitSeq;
						if (waitSeq == 21){
							waitSeq = 1;
						}
						//输出数据       
						//printf("%s\n",&buffer[1]);
						
						if (seqIsAvailable())
						{
							buffer[0] = seq;
							buffer[1] = cl_curSeq;
							cl_ack[cl_curSeq] = FALSE;
							memcpy(&buffer[2], data + 1024 * cl_totalSeq, 1024);
							recvSeq = seq;
							++cl_curSeq;
							cl_curSeq %= SEQ_SIZE;
							++cl_totalSeq;
							Sleep(500);
						}
					}
					else{
						//如果当前一个包都没有收到，则等待 Seq 为 1 的数 据包，跳出循环，不返回 ACK（因为并没有上一个正确的 ACK）       
						if (recvSeq==0){
							continue;
						}
						if (seqIsAvailable())
						{
							buffer[0] = seq;
							buffer[1] = cl_curSeq;
							cl_ack[cl_curSeq] = FALSE;
							memcpy(&buffer[2], data + 1024 * cl_totalSeq, 1024);
							recvSeq = seq;
							++cl_curSeq;
							cl_curSeq %= SEQ_SIZE;
						    ++cl_totalSeq;
						}
					}
					b = lossInLossRatio(ackLossRatio);
					if (b){
						printf("The ack of %d loss\n", (unsigned char)buffer[0]);
						if (cl_curSeq == 0){
							cl_curSeq = 19;
							cl_ack[cl_curSeq] = TRUE;
						}
						else
						{
							cl_curSeq--;
							cl_ack[cl_curSeq] = TRUE;
						}
						continue;
					}
					sendto(socketClient, buffer, 2, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
					printf("client:::send a ack of %d,seq of %d\n", (unsigned char)buffer[0],(unsigned char)buffer[1]);
					break;
				}
				Sleep(500);
			}
		}
		sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
		ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
		printf("%s\n", buffer);
		if (!strcmp(buffer, "Good bye!")){
			break;
		}
		printTips();
	}  //关闭套接字  
	closesocket(socketClient);
	WSACleanup();
	return 0;
}
