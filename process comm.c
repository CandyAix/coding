#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_SIZE 5
#define MAX_SIZE1 100

struct Queue
{
    int data[MAX_SIZE];
    int front;
    int rear;
    int size;
};

void initQueue(struct Queue *q)
{
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

int isFull(struct Queue *q)
{
    return (q->size == MAX_SIZE);
}

int isEmpty(struct Queue *q)
{
    return (q->size == 0);
}

void enqueue(struct Queue *q, int value)
{
    if (isFull(q))
    {
        printf("队列已满，无法插入数据\n");
        exit(1);
    }

    q->rear = (q->rear + 1) % MAX_SIZE;
    q->data[q->rear] = value;
    q->size++;
}

int dequeue(struct Queue *q)
{
    if (isEmpty(q))
    {
        printf("队列为空，无法出队\n");
        exit(1);
    }

    int value = q->data[q->front];
    q->front = (q->front + 1) % MAX_SIZE;
    q->size--;

    return value;
}

void fun(struct Queue *q, int i)
{
    enqueue(q, 1 * i);
    enqueue(q, 2 * i);
    enqueue(q, 3 * i);
}

int main()
{
    struct Queue Q;
    initQueue(&Q);

    int i;
    pid_t pid;

    for (i = 0; i < 2; i++)
    {
        pid = fork(); // 创建子进程

        if (pid == -1)
        {
            printf("Fork failed");
            exit(1);
        }
        else if (pid == 0)
        {
            // 子进程调用fun函数向队列中添加数据
            fun(&Q, i + 1);
            exit(0);
        }
        else
        {
            // 父进程等待子进程结束
            wait(NULL);
        }
    }

    printf("%d\n", Q.size);      // 6
    printf("%d\n", dequeue(&Q)); // 1
    printf("%d\n", dequeue(&Q)); // 2
    printf("%d\n", dequeue(&Q)); // 3
    printf("%d\n", dequeue(&Q)); // 2
    printf("%d\n", dequeue(&Q)); // 4
    printf("%d\n", dequeue(&Q)); // 6
    printf("%d\n", Q.size);      // 0

    return 0;
}

struct Pipe
{
    int fd[2];
};

void sendMessage(struct Pipe *pipe, const char *message)
{
    close(pipe->fd[0]); // 关闭读端
    write(pipe->fd[1], message, strlen(message) + 1);
    close(pipe->fd[1]); // 关闭写端
}

void receiveMessage(struct Pipe *pipe, char *buffer)
{
    close(pipe->fd[1]); // 关闭写端
    read(pipe->fd[0], buffer, MAX_SIZE1);
    close(pipe->fd[0]); // 关闭读端
}

void f(struct Pipe *pipe)
{
    sleep(1);
    receiveMessage(pipe, message);
    printf("来自父亲的问候：%s\n", message);
    sendMessage(pipe, "嗯");
}

int main()
{
    struct Pipe parentPipe, childPipe;
    if (pipe(parentPipe.fd) == -1 || pipe(childPipe.fd) == -1)
    {
        perror("管道创建失败");
        exit(1);
    }

    pid_t pid = fork(); // 创建子进程

    if (pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        // 子进程
        close(parentPipe.fd[1]); // 关闭父进程写端
        close(childPipe.fd[0]);  // 关闭子进程读端

        sendMessage(&childPipe, "吃了吗");

        char message[MAX_SIZE1];
        receiveMessage(&childPipe, message);
        printf("来自儿子的问候：%s\n", message);

        close(parentPipe.fd[0]); // 关闭父进程读端
        close(childPipe.fd[1]);  // 关闭子进程写端
    }
    else
    {
        // 父进程
        close(parentPipe.fd[0]); // 关闭父进程读端
        close(childPipe.fd[1]);  // 关闭子进程写端

        char message[MAX_SIZE1];
        receiveMessage(&parentPipe, message);
        printf("来自父亲的问候：%s\n", message);

        sendMessage(&parentPipe, "吃了吗");

        close(parentPipe.fd[1]); // 关闭父进程写端
        close(childPipe.fd[0]);  // 关闭子进程读端

        // 等待子进程结束
        wait(NULL);
    }

    return 0;
}

#include <sys/mman.h> // for mmap
#include <fcntl.h>    // for shared memory

struct Namespace
{
    int a;
};

void f(double *x, int *arr, char **l, struct Namespace *n)
{
    *x = 3.14;
    arr[0] = 5;
    *l = "Hello";
    n->a = 10;
}

int main()
{
    int shm_x = shm_open("/shared_x", O_CREAT | O_RDWR, 0666);
    if (shm_x == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_x, sizeof(double));
    double *x = (double *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, shm_x, 0);
    *x = 0.0;

    int shm_arr = shm_open("/shared_arr", O_CREAT | O_RDWR, 0666);
    if (shm_arr == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_arr, sizeof(int) * 10);
    int *arr = (int *)mmap(NULL, sizeof(int) * 10, PROT_READ | PROT_WRITE, MAP_SHARED, shm_arr, 0);
    for (int i = 0; i < 10; ++i)
    {
        arr[i] = i;
    }

    int shm_l = shm_open("/shared_l", O_CREAT | O_RDWR, 0666);
    if (shm_l == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_l, sizeof(char) * 10);
    char *l = (char *)mmap(NULL, sizeof(char) * 10, PROT_READ | PROT_WRITE, MAP_SHARED, shm_l, 0);
    *l = '\0';

    int shm_n = shm_open("/shared_n", O_CREAT | O_RDWR, 0666);
    if (shm_n == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_n, sizeof(struct Namespace));
    struct Namespace *n = (struct Namespace *)mmap(NULL, sizeof(struct Namespace), PROT_READ | PROT_WRITE, MAP_SHARED, shm_n, 0);
    n->a = 0;

    pid_t pid = fork(); // 创建子进程

    if (pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        // 子进程
        f(x, arr, &l, n);
        exit(0);
    }
    else
    {
        // 父进程
        wait(NULL);

        printf("%lf\n", *x);
        printf("arr: ");
        for (int i = 0; i < 10; ++i)
        {
            printf("%d ", arr[i]);
        }
        printf("\n");
        printf("l: %s\n", l);
        printf("n.a: %d\n", n->a);

        munmap(x, sizeof(double));
        munmap(arr, sizeof(int) * 10);
        munmap(l, sizeof(char) * 10);
        munmap(n, sizeof(struct Namespace));

        shm_unlink("/shared_x");
        shm_unlink("/shared_arr");
        shm_unlink("/shared_l");
        shm_unlink("/shared_n");
    }

    return 0;
}

#include <sys/mman.h> // for mmap
#include <fcntl.h>    // for shared memory
#include <string.h>

struct IpConnectionPool
{
    // 定义IpConnectionPool结构体
    // TODO: 根据实际情况定义IpConnectionPool的成员变量
};

void batchInsert(int index, struct IpConnectionPool *pool)
{
    // 实现batchInsert函数
    // TODO: 根据实际情况编写batchInsert函数的逻辑
}

void call_back(int result)
{
    // 实现call_back函数
    // TODO: 根据实际情况编写call_back函数的逻辑
}

void err_call_back(int error)
{
    // 实现err_call_back函数
    // TODO: 根据实际情况编写err_call_back函数的逻辑
}

int main()
{
    int shm_pool = shm_open("/shared_pool", O_CREAT | O_RDWR, 0666);
    if (shm_pool == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_pool, sizeof(struct IpConnectionPool));
    struct IpConnectionPool *pool = (struct IpConnectionPool *)mmap(NULL, sizeof(struct IpConnectionPool), PROT_READ | PROT_WRITE, MAP_SHARED, shm_pool, 0);

    pid_t pid = fork(); // 创建子进程

    if (pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        // 子进程
        for (int i = 2; i < 3423; ++i)
        {
            batchInsert(i, pool);
            sleep(5);
        }
        exit(0);
    }
    else
    {
        // 父进程
        wait(NULL);
    }

    munmap(pool, sizeof(struct IpConnectionPool));
    shm_unlink("/shared_pool");

    return 0;
}

void f(double *n, int *a)
{
    *n = 3.14;
    a[0] = 5;
}

int main()
{
    int shm_n = shm_open("/shared_n", O_CREAT | O_RDWR, 0666);
    if (shm_n == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_n, sizeof(double));
    double *n = (double *)mmap(NULL, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, shm_n, 0);
    *n = 0.0;

    int shm_a = shm_open("/shared_a", O_CREAT | O_RDWR, 0666);
    if (shm_a == -1)
    {
        perror("shm_open failed");
        exit(1);
    }
    ftruncate(shm_a, sizeof(int) * 10);
    int *a = (int *)mmap(NULL, sizeof(int) * 10, PROT_READ | PROT_WRITE, MAP_SHARED, shm_a, 0);
    for (int i = 0; i < 10; ++i)
    {
        a[i] = i;
    }

    pid_t pid = fork(); // 创建子进程

    if (pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        // 子进程
        f(n, a);
        exit(0);
    }
    else
    {
        // 父进程
        wait(NULL);

        printf("%lf\n", *n);
        printf("arr: ");
        for (int i = 0; i < 10; ++i)
        {
            printf("%d ", a[i]);
        }
        printf("\n");

        munmap(n, sizeof(double));
        munmap(a, sizeof(int) * 10);

        shm_unlink("/shared_n");
        shm_unlink("/shared_a");
    }

    return 0;
}
