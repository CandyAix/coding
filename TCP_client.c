#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define case 128
int clifd;

// TCP Client
void signal_handler(int sig)
{
    printf("shutdown\n");
    shutdown(clifd, SHUT_RDWR);
    exit(0);
}
int main()
{
    signal(SIGINT, signal_handler);
    clifd = socket(PF_INET, PF_UNIX, 0); // 设定socket套接字
    if (clifd < 0)
    {
        return -1;
    }
    printf("socket success\n");
    struct sockaddr_in ser_addr;                           // 定义结构体
    memset(&ser_addr, 0, sizeof(ser_addr));                // 清空结构体，全为0
    ser_addr.sin_family = PF_INET;                         // 设置地址族，因为当前所用为TCP/IPv4,对应的地址族为AF_INET
    ser_addr.sin_port = htons(1234);                       // 端口号，需要将short类型的主机字节序转化为网络字节序
    ser_addr.sin_addr.s_addr = inet_addr("192.168.1.241"); // 将点分十进制IPv4转换为网络字节序整数标的IPv4地址，此地址为回环测试地址，也可以输入本机IP
    char data[case] = {0};
    int number;
    char buff[case] = {0};
    int reply;
    int res = -1;
    while (1)
    {
        res = connect(clifd, (struct sockaddr *)&ser_addr, sizeof(ser_addr));
        if (res > 0)
        {
            printf("linking\n");
            break;
        }

        sleep(1);
    }

    printf("connect success\n");

    while (1)
    {
        printf("input the message\n");

        memset(data, 0, sizeof(data));
        memset(buff, 0, sizeof(buff));

        fgets(data, sizeof(data) - 1, stdin);
        if (strncmp(data, "end", 3) == 0)
        {
            printf("send over\n");
            return -1;
        }

        number = send(clifd, data, strlen(data), 0);
        if (number < 0)
        {
            printf("send error\n");
            break;
        }
        else if (number == 0)
        {
            printf("length error\n"); // 长度为0
            break;
        }

        reply = recv(clifd, buff, sizeof(buff) - 1, 0);
        if (reply < 1)
        {
            printf("recv error\n");
            break;
        }
        else
        {
            printf("recv success\n");
        }
        printf("%s\n", buff);
    }
    close(clifd);
}
