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
    char readbuf[100], writebuf[100];
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

    if(atoi(argv[2]) == 1)
    {
        ret = read(fd, readbuf, 50);
        if(ret < 0)
        {
            printf("Read file %s failed!\r\n", filename);
            return -1; 
        }
        else
        {
            printf("read data: %s\r\n", readbuf);
        }
        
    }
    if(atoi(argv[2]) == 2)
    {
        memcpy(writebuf, usrdata, sizeof(usrdata));
        ret = write(fd, writebuf, 50);
        if(ret < 0)
        {
            printf("write file %s failed!\r\n", filename);
            return -1; 
        } 
    }

    ret = close(fd);
    if(ret < 0)
    {
        printf("close file %s failed!\r\n", filename);
        return -1; 
    } 
    return 0;
}

