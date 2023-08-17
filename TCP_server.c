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
char buff[case] = {0};
int serverfd;
// TCP server

void signal_handler(int sig)
{
    printf("shutdown\n");
    shutdown(serverfd, SHUT_RDWR);
    exit(0);
}

static void *thread(void *arg)
{
    int confd = (int)arg;
    printf("confd: %d\n", confd);
    while (1)
    {
        int number;
        memset(buff, 0, sizeof(buff));
        number = recv(confd, buff, sizeof(buff), 0);
        if (number < 0)
        {
            printf("recv error\n");
            break;
        }
        else if (number == 0)
        {
            printf("link error\n"); // 客户端关闭，连接失败

            break;
        }
        printf("The data is %s\n", buff);
        printf("%d", sizeof(buff));

        number = send(confd, buff, number, 0);
        if (number < 0)
        {
            printf("send error\n");
            break;
        }
        else
        {
            printf("send success\n");
        }

        close(confd);
        return 0;
    }
}

int main()
{
    signal(SIGINT, signal_handler);

    int confd;
    char buff[case] = {0};
    struct sockaddr_in cli_addr;            // 定义保存客户端socket地址的结构体
    socklen_t len = sizeof(cli_addr);       // 得到长度
    serverfd = socket(PF_INET, PF_UNIX, 0); // 设定socket套接字
    if (serverfd < 0)
    {
        return -1;
    }
    printf("socket success\n");
    struct sockaddr_in ser_addr;                                              // 定义结构体
    memset(&ser_addr, 0, sizeof(ser_addr));                                   // 清空结构体，全为0
    ser_addr.sin_family = PF_INET;                                            // 设置地址族，因为当前所用为TCP/IPv4,对应的地址族为AF_INET
    ser_addr.sin_port = htons(1234);                                          // 端口号，需要将short类型的主机字节序转化为网络字节序
    ser_addr.sin_addr.s_addr = inet_addr("192.168.1.241");                    // 将点分十进制IPv4转换为网络字节序整数标的IPv4地址，此地址为回环测试地址，也可以输入本机IP
    int res = bind(serverfd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)); // bind命名绑定
    printf("%d", res);

    if (res < 0)
    {
        return -1;
    }
    printf("bind success\n");

    res = listen(serverfd, 5);
    if (res < 0)
    {
        printf("linsten error\n");
        return -1;
    }
    printf("listen success\n");
    pthread_t tid;
    while (1)
    {
        printf("accept ...\n");
        confd = accept(serverfd, (struct sockaddr *)&cli_addr, &len); // 传入，执行成功后，cli_addr保存客户端的socket地址信息
        if (confd < 0)
        {
            printf("link error\n");
            continue;
        }
        else
        {
            printf("accept success\n");
        }
        printf("%s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        printf("confd: %d\n", confd);
        pthread_create(&tid, NULL, thread, confd);
    }

    pthread_join(&tid, NULL);
    close(serverfd);
    exit(0);
}
