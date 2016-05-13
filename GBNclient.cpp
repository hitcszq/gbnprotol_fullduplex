// GBNclient.cpp : �������̨Ӧ�ó������ڵ㡣
//

// GBN_client.cpp : �������̨Ӧ�ó������ڵ㡣 
#include "stdafx.h" 
#include <stdlib.h> 
#include <WinSock2.h> 
#include <time.h>  
#include <cstdio>
#include <fstream> 
#pragma comment(lib,"ws2_32.lib")  
#define SERVER_PORT 12340 //�������ݵĶ˿ں� 
#define SERVER_IP  "127.0.0.1" // �������� IP ��ַ  
const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//���ն����кŸ�����Ϊ 1~20  
BOOL cl_ack[SEQ_SIZE];//�յ� ack �������Ӧ 0~19 �� ack 
int cl_curSeq;//��ǰ���ݰ��� seq 
int cl_curAck;//��ǰ�ȴ�ȷ�ϵ� ack 
int cl_totalSeq;//�յ��İ������� 
int cl_totalPacket;//��Ҫ���͵İ����� 
const int SEND_WIND_SIZE = 10;
//************************************ 
// Method:    timeoutHandler 
// FullName:  timeoutHandler 
// Access:    public  
// Returns:   void 
// Qualifier: ��ʱ�ش������������������ڵ�����֡��Ҫ�ش� 
//************************************ 

void timeoutHandler()//����ûʲô��
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
// Qualifier: �յ� ack���ۻ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ� 
//���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ �ˣ��˴���Ҫ��һ��ԭ 
// Parameter: char c 
//************************************ 
void ackHandler(char c)
{
	if (c == 0)
	{
		printf("client:::server's first package do not have ack!\n");
		return;
	}
	unsigned char index = (unsigned char)c - 1; //���кż�һ  
	printf("client:::Recv a ack of %d\n", index + 1);//��ŷ��ͷ��ͽ��շ�Ӧ��ͳһ�����͵���+1���ģ����շ�Ack+1���ģ��������ӡû��Ҫ��ԭ
	if (cl_curAck <= index){
		for (int i = cl_curAck; i <= index; ++i){
			cl_ack[i] = TRUE;
		}
		cl_curAck = (index + 1) % SEQ_SIZE;
	}
	else{   //ack ���������ֵ���ص��� curAck �����   
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
	step = step >= 0 ? step : step + SEQ_SIZE;  //���к��Ƿ��ڵ�ǰ���ʹ���֮��  
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
-time �ӷ������˻�ȡ��ǰʱ��
-quit �˳��ͻ���
-testgbn [X] ���� GBN Э��ʵ�ֿɿ����ݴ���
[X] [0,1] ģ�����ݰ���ʧ�ĸ���
[Y] [0,1] ģ�� ACK ��ʧ�ĸ��� */
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
// Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻� TRUE�����򷵻� FALSE 
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
	//�����׽��ֿ⣨���룩  
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ  
	int err;
	//�汾 2.2  
	wVersionRequested = MAKEWORD(2, 2);  //���� dll �ļ� Scoket ��   
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0){
		//�Ҳ��� winsock.dll   
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
	addrServer.sin_port = htons(SERVER_PORT);  //���ջ�����  
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�õ�ǰ ʱ��  
	//ʹ�� -testgbn [X] [Y] ���� GBN ����[X]��ʾ���ݰ���ʧ����  
	//          [Y]��ʾ ACK ��������  
	printTips();  int ret;
	int interval = 1;
	//�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ���� ���� ack��0 ���߸�������ʾ���еĶ������� ack  
	char cmd[128];
	float packetLossRatio = 0.2;
	//Ĭ�ϰ���ʧ�� 0.2  
	float ackLossRatio = 0.2;
	//Ĭ�� ACK ��ʧ�� 0.2  
	//��ʱ����Ϊ������ӣ�����ѭ����������  

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
		//��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ�����   
		if (!strcmp(cmd, "-testgbn")){
			printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
			printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
			int waitCount = 0;
			int stage = 0;
			BOOL b;
			unsigned char u_code;//״̬��    
			unsigned short seq;//�������к�    
			unsigned short recvSeq;//���մ��ڴ�СΪ 1����ȷ�ϵ����к�    
			unsigned short waitSeq;//�ȴ������к� 
			sendto(socketClient, "-testgbn", strlen(" - testgbn") + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			while (true){
				//�ȴ� server �ظ����� UDP Ϊ����ģʽ     
				recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
				switch (stage){
				case 0://�ȴ����ֽ׶�      
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
				case 1://�ȴ��������ݽ׶�
					seq = (unsigned short)buffer[0];
					//�����ģ����Ƿ�ʧ      
					b = lossInLossRatio(packetLossRatio);
					if (b){
						printf("The packet with a seq of %d loss\n", seq);
						ackHandler((unsigned short)buffer[1]);
						continue;
					}
					printf("client:::recv a packet with a seq of %d\n", seq);
					//������ڴ��İ�����ȷ���գ�����ȷ�ϼ���  ����ʱͬʱ���͹������ݲ�����
					if (waitSeq - seq==0){
						++waitSeq;
						if (waitSeq == 21){
							waitSeq = 1;
						}
						//�������       
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
						//�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 ���� �ݰ�������ѭ���������� ACK����Ϊ��û����һ����ȷ�� ACK��       
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
	}  //�ر��׽���  
	closesocket(socketClient);
	WSACleanup();
	return 0;
}
