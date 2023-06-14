#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"

// 格式化文件名
char * fmtname(char* path);
// 在某个路径下，查找对应的文件
void find(char* path, const char* filename);

int main(int argc, char* argv[])
{
    if (argc < 3)
        printf("usage : find path filename\n");
    for (int i = 2; i < argc; ++i)
        find(argv[1], argv[i]);
    exit(0);
}

char * fmtname(char* path)
{
    char* p;
    // 对字符串从后往前找，找到倒数一个/，那么它后面就是具体的文件名
    for (p = path+strlen(path); p >= path && *p != '/'; p--);
    return p+1; // 返回倒数第一个斜杠后的首字母，就是改文件名的首字母
}

void find(char* path, const char* filename)
{
    char buf[512], *p;
    int fd;    // 用于打开这个文件
    struct dirent de;  // inode结构体
    struct stat st;    // 文件状态,大小类型等

    if ((fd = open(path, O_RDONLY)) < 0)
    {  // 打开查询的文件失败
        // 这里打开的是一个目录文件
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    switch (st.type)
    {
        case T_FILE:    // 没有路径，直接就是文件
            if (strcmp(filename, fmtname(path)) == 0)
            {
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            if (strlen(path)+1+DIRSIZ+1 > sizeof(buf))
            {
                printf("find: path too long\n");
                return;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *(p++) = '/';
            // 遍历当前目录
            while (read(fd, &de, sizeof(de)) == sizeof(de))
            {
                if (de.inum == 0 || strcmp(de.name, ".") == 0 || 
                    strcmp(de.name, "..") == 0)
                    {
                        continue;
                    }
                    memmove(p, de.name, DIRSIZ);
                    p[DIRSIZ] = 0;
                    if (stat(buf, &st) < 0)
                    {
                        printf("find: cannot stat %s\n", buf);
                        continue;
                    }
                    find(buf, filename);
            }
            break;
    }
    close(fd);
}

