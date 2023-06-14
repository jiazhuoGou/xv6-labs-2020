#include "kernel/types.h"
#include "user/user.h"

// pipe 里面下标0是read，1是write
#define RAEDNED 0
#define WRITEEND 1

int main(void)
{
    int p1[2];  // 父写子读
    int p2[2];  // 父读子写
    
    pipe(p1);
    pipe(p2);
    int pid = fork();
    if (pid > 0)
    {   // 父进程,写ping，读pong
        // 限定操作的文件描述符
        close(p1[RAEDNED]);
        close(p2[WRITEEND]);
        char recv_buf[10];
        write(p1[WRITEEND], "ping", 4);
        if (read(p2[RAEDNED], recv_buf, 4) < 0)
        {
            fprintf(2, "read error\n");
        }
        printf("parentpid : %d: received %s\n", getpid(), recv_buf);
        // 用完关闭
        close(p1[WRITEEND]);
        close(p2[RAEDNED]);
    }
    else if (pid == 0)
    {   // 子进程，写pong,读ping
        close(p1[WRITEEND]);
        close(p2[RAEDNED]);
        char recv_buf[5];
        if (read(p1[RAEDNED], recv_buf, 4) < 0)
        {
            fprintf(2, "read error\n");
        }
        printf("childpid : %d: received %s\n", getpid(), recv_buf);
        write(p2[WRITEEND], "pong", 4);
        close(p1[RAEDNED]);
        close(p2[WRITEEND]);
    }
    else
    {
        fprintf(2, "fork error\n");
    }
    exit(0);
}
