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
    char *filename;
    char databuf[2];
    if(argc != 3)
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
    ret = write(fd, databuf, 1);
    if(ret < 0)
    {
        printf("write file %s failed!\r\n", filename);
        return -1; 
    } 

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s failed!\r\n", filename);
        return -1; 
    } 
    return 0;
}

