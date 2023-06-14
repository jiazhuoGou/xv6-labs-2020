#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {   // 第0号参数默认是程序名，所以不输入参数就会有一个
        fprintf(2, "too few argument, e.g. sleep num\n");
        exit(1);
    }
    // 执行系统调用
    //uint num = atoi(argv[1]);
    sleep(atoi(argv[1]));
    fprintf(2, "sleep success\n");
    exit(0);
}