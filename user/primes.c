/*
大概意思是用递归的方式筛选素数，理解这道题的意思用了很长时间
*/
#include "kernel/types.h"
#include "user/user.h"

// pipe 里面下标0是read，1是write
#define RAEDEND 0
#define WRITEEND 1

void recursion(int* pipeleft);

int main(int argc, char *argv[])
{
    int pipeleft[2];
    pipe(pipeleft);

    int pid = fork();
    if (pid > 0)
    { // 父进程
        close(pipeleft[RAEDEND]);
        for (int i = 2; i <= 35; ++i)
        {
            write(pipeleft[WRITEEND], &i, sizeof(int));
        }
        close(pipeleft[WRITEEND]);
        wait(0);    // 等待所有子进程结束
    }
    else if (pid == 0)
    {
        recursion(pipeleft);
    }
    else
    {
        fprintf(2, "fork error\n");
        exit(1);
    }

    exit(0);
}

void recursion(int* pipeleft)
{
    // 子进程从管道取出最小的数，这个数就是素数，然后打印
    // 再用继续读出来的数，去判断能不能整除，再重新放入新的管道
    // 不断地递归
    int piperight[2];
    int n, temp;
    pipe(piperight);
    close(pipeleft[WRITEEND]);
    if( read(pipeleft[RAEDEND], &n, sizeof(int)) == 0)
    {
        close(pipeleft[RAEDEND]);
        exit(0);
    }
    printf("prime : %d\n", n);
    int pid = fork();
    if (pid == 0)
    {   
        recursion(piperight);
    }
    else if (pid > 0)
    {   // 父进程就是输数据
        close(piperight[RAEDEND]);
        while (read(pipeleft[RAEDEND], &temp, sizeof(int)) != 0)
        {
            if (temp % n != 0)
                write(piperight[WRITEEND], &temp, sizeof(int));
        }
        close(piperight[WRITEEND]);
        wait(0);
        exit(0);
    }
}

