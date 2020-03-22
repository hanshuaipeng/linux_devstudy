#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "linux/ioctl.h"
/* 命令值 */
 #define CLOSE_CMD      (_IO(0XEF, 0x1)) /* 关闭定时器 */
 #define OPEN_CMD       (_IO(0XEF, 0x2)) /* 打开定时器 */
 #define SETPERIOD_CMD  (_IO(0XEF, 0x3)) /* 设置定时器周期命令 */
 static char usrdata[] = {"usr data!"};

int main(int argc, char *argv[]) 
{
    int fd, ret;
    unsigned int cmd;
    unsigned int arg;
    unsigned char cnt = 0;
    char *filename;
    char databuf[2];
    unsigned char str[100];
    if(argc != 2)
    {
        printf("Error Usage!\r\n");
        return -1;
    }
    filename = argv[1];
    fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        printf("Can't Open file %s!\r\n", filename);
        return -1;
    }
    while(1)
    {
        printf("Input CMD:");
        ret = scanf("%d",&cmd);
        if (ret != 1) { /* 参数输入错误 */
            gets(str); /* 防止卡死 */
        }
        if(cmd == 1) /* 关闭 LED 灯 */
            cmd = CLOSE_CMD;
        else if(cmd == 2) /* 打开 LED 灯 */
            cmd = OPEN_CMD;
        else if(cmd == 3) 
        {
            cmd = SETPERIOD_CMD; /* 设置周期值 */
            printf("Input Timer Period:");
            ret = scanf("%d", &arg);
            if (ret != 1) { /* 参数输入错误 */
                gets(str); /* 防止卡死 */
            }
        }
        ioctl(fd, cmd, arg); /* 控制定时器的打开和关闭 */
    }
    close(fd);
    return 0;
}

