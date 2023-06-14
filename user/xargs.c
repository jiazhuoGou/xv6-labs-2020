#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAX_LEN 100

// xargs前面的都是stdin里面的输入，xargs 后面的是执行程序，和另外的参数
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(2, "usage: xarg [command] \n");
        exit(1);
    }
    char* command = argv[1];
    char bf;    // 一次从stdin里面一个字符的读
    char param_vec[MAXARG][MAX_LEN];    // 列数是一个参数的最大长度
    char* m[MAXARG];    // exec函数里面的参数

    while (1)
    {
        int count = argc - 1;   // 实际的参数个数，不是stdin里面的
        memset(param_vec, 0, MAXARG * MAX_LEN);
        for (int i = 1; i < argc; ++i)
            strcpy(param_vec[i-1], argv[i]);    // 这是实际的参数

        int cursor = 0;
        int flag = 0;   // 判断是否有前置空格,判断是否是新的参数
        int read_flag;

        while ( (read_flag = read(0, &bf, 1)) > 0 && bf !='\n' )
        {   // 从stdin里面需要转换的参数
            if (bf == ' ' && flag == 1)
            {   // 说明有一个新的参数
                ++count;
                cursor = 0;
                flag = 0;
            }
            else if (bf != ' ')
            {
                param_vec[count][cursor++] = bf;
                flag = 1;
            }
        }
        if (read_flag <= 0)
            break;
        for (int i = 0; i < MAXARG-1; ++i)
            m[i] = param_vec[i];
        m[MAXARG-1] = 0;
        if (fork() == 0)
        {
            exec(command, m);
            exit(0);
        }
        else
        {
            wait(0);
        }
    }
    exit(0);
}