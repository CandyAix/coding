#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

pthread_mutex_t lock;

void *fun(void *arg)
{
    int i = *(int *)arg;
    pthread_mutex_lock(&lock); // 获取锁
    printf("正在运行线程: %d\n", i);
    sleep(2);
    pthread_mutex_unlock(&lock); // 使用完后释放锁
    return NULL;
}

int main()
{
    pthread_t threads[5];
    int thread_args[5];

    pthread_mutex_init(&lock, NULL); // 初始化锁

    for (int i = 0; i < 5; ++i)
    {
        thread_args[i] = i;
        pthread_create(&threads[i], NULL, fun, &thread_args[i]); // 创建线程
    }

    for (int i = 0; i < 5; ++i)
    {
        pthread_join(threads[i], NULL); // 等待线程结束
    }

    pthread_mutex_destroy(&lock); // 销毁锁

    return 0;
}

// pthread_mutexattr_settype

// pthread_mutexattr_settype

// pthread_mutexattr_settype

pthread_mutex_t lock;

int abce = 0;

void *thread_func(void *arg)
{
    pthread_mutex_lock(&lock); // 加锁
    abce = abce + 1;
    pthread_mutex_lock(&lock); // 再次加锁
    abce = abce + 2;
    pthread_mutex_unlock(&lock); // 释放里面的锁
    pthread_mutex_unlock(&lock); // 释放外面的锁
    return NULL;
}

int main()
{
    pthread_t thread;
    pthread_mutex_init(&lock, NULL); // 初始化锁

    pthread_create(&thread, NULL, thread_func, NULL); // 创建线程

    pthread_join(thread, NULL); // 等待线程结束

    printf("%d\n", abce);

    pthread_mutex_destroy(&lock); // 销毁锁

    return 0;
}