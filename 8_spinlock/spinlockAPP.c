#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

static char usrdata[] = {"usr data!"};

int main(int argc, char *argv[]) 
{
    int fd, ret;
    unsigned char cnt = 0;
    char *filename;
    char databuf[2];
    if(argc != 4)
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
    databuf[0] = atoi(argv[2]);
    databuf[1] = atoi(argv[3]);
    ret = write(fd, databuf, 2);
    if(ret < 0)
    {
        printf("close file %s failed!\r\n", filename);
        return -1; 
    } 
    /* 模拟占用 25S LED */
    while(1) 
    {
        sleep(5);
        cnt++;
        printf("App running times:%d\r\n", cnt);
        if(cnt >= 5) break;
    }
    
    printf("App running finished!");
    ret = close(fd); /* 关闭文件 */
    if(ret < 0){
    printf("file %s close failed!\r\n", argv[1]);
    return -1;
    }
    return 0;
}

