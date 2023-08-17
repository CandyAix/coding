#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

int UART0_Open(int fd, char *port)
{
    fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0)
    {
        printf("openserial port failed\n");
        return -1;
    }
    if (fcntl(fd, F_SETFL, 0) < 0)
    {
        printf("fcntl failed\n");
        return -1;
    }
    else
    {
        printf("fcntl=%d\n", fcntl(fd, F_SETFL, 0));
    }

    if (0 == isatty(STDIN_FILENO))
    {
        printf("not terminal device\n");
        return -1;
    }
    else
    {
        printf("isatty success\n");
    }
    printf("fd->open=%d\n", fd);
    return fd;
}

int UART0_Close(int fd)
{
    close(fd);
}

int UART0_Set(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{

    int i;
    int status;
    int speed_arr[] = {B115200, B19200, B9600, B4800, B2400, B1200, B300};
    int name_arr[] = {115200, 19200, 9600, 4800, 2400, 1200, 300};

    struct termios options;

    if (tcgetattr(fd, &options) != 0)
    {
        perror("SetupSerial 1");
        return -1;
    }

    for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++)
    {
        if (speed == name_arr[i])
        {
            cfsetispeed(&options, speed_arr[i]);
            cfsetospeed(&options, speed_arr[i]);
        }
    }

    options.c_cflag |= CLOCAL;

    options.c_cflag |= CREAD;

    // 数据流控制
    switch (flow_ctrl)
    {

    case 0:
        options.c_cflag &= ~CRTSCTS;
        break;

    case 1:
        options.c_cflag |= CRTSCTS;
        break;
    case 2:
        options.c_cflag |= IXON | IXOFF | IXANY;
        break;
    }

    options.c_cflag &= ~CSIZE;
    // 设置数据位
    switch (databits)
    {
    case 5:
        options.c_cflag |= CS5;
        break;
    case 6:
        options.c_cflag |= CS6;
        break;
    case 7:
        options.c_cflag |= CS7;
        break;
    case 8:
        options.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr, "unsupported data size\n");
        return -1;
    }
    // 设置校验位
    switch (parity)
    {
    case 'n':
    case 'N':
        options.c_cflag &= ~PARENB;
        options.c_iflag &= ~INPCK;
        break;
    case 'o':
    case 'O':
        options.c_cflag |= (PARODD | PARENB);
        options.c_iflag |= INPCK;
        break;
    case 'e':
    case 'E':
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_iflag |= INPCK;
        break;
    case 's':
    case 'S':
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        break;
    default:
        fprintf(stderr, "unsupported parity\n");
        return -1;
    }
    // 设置停止位
    switch (stopbits)
    {
    case 1:
        options.c_cflag &= ~CSTOPB;
        break;
    case 2:
        options.c_cflag |= CSTOPB;
        break;
    default:
        fprintf(stderr, "Unsupported stop bits\n");
        return -1;
    }

    options.c_oflag &= ~OPOST;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // 设置等待时间和最小接收字符
    options.c_cc[VTIME] = 1; /* 读取一个字符等待1*(1/10)s */
    options.c_cc[VMIN] = 1;  /* 读取字符的最少个数为1 */

    // 数据溢出接收但不读取
    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, &options) != 0)
    {
        perror("com set error\n");
        return -1;
    }
    return 0;
}

int UART0_Init(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{
    int err;
    if (UART0_Set(fd, 19200, 0, 8, 1, 'N') == FALSE)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int UART0_Recv(int fd, char *rcv_buf, int data_len)
{
    int len, fs_sel;
    fd_set fs_read;

    struct timeval time;

    FD_ZERO(&fs_read);
    FD_SET(fd, &fs_read);

    time.tv_sec = 10;
    time.tv_usec = 0;

    fs_sel = select(fd + 1, &fs_read, NULL, NULL, &time);
    if (fs_sel)
    {
        len = read(fd, rcv_buf, data_len);
        printf("len = %d fs_sel = %d\n", len, fs_sel);
        return len;
    }
    else
    {
        printf("recv failed\n");
        return -1;
    }
}

int UART0_Send(int fd, char *send_buf, int data_len)
{
    int len = 0;

    len = write(fd, send_buf, data_len);
    if (len == data_len)
    {
        return len;
    }
    else
    {

        tcflush(fd, TCOFLUSH);
        return FALSE;
    }
}

int main(int argc, char **argv)
{
    int fd;  // 文件描述符
    int err; // 返回调用函数的状态
    int len;
    int i;
    char rcv_buf[100];
    char send_buf[20] = "tiger john";
    if (argc != 3)
    {
        printf("Usage: %s /dev/ttySn 0(send data)/1 (receive data) \n", argv[0]);
        return FALSE;
    }
    fd = UART0_Open(fd, argv[1]); // 打开串口，返回文件描述符
    do
    {
        err = UART0_Init(fd, 19200, 0, 8, 1, 'N');
        printf("Set Port Exactly\n");
    } while (FALSE == err || FALSE == fd);

    if (0 == strcmp(argv[2], "0"))
    {
        for (i = 0; i < 10; i++)
        {
            len = UART0_Send(fd, send_buf, 10);
            if (len > 0)
                printf(" %d send data successful\n", i);
            else
                printf("send data failed\n");

            sleep(2);
        }
        UART0_Close(fd);
    }
    else
    {

        while (1)
        {
            len = UART0_Recv(fd, rcv_buf, 9);
            if (len > 0)
            {
                rcv_buf[len] = '\0';
                printf("receive data is %s\n", rcv_buf);
                printf("len = %d\n", len);
            }
            else
            {
                printf("cannot receive data\n");
            }
            sleep(2);
        }
        UART0_Close(fd);
    }
}