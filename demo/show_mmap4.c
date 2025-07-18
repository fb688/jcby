/*
打开液晶屏文件
给液晶屏文件映射一块内存
通过映射起始地址来显示颜色
解除映射
关闭液晶屏文件
*/

#include <stdio.h>
//open头文件
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//close、read、write头文件
#include <unistd.h>
//mmap头文件
#include <sys/mman.h>

int main()
{
    //1.打开液晶屏文件
    int lcd_fd = open("/dev/fb0",O_RDWR);
    if(lcd_fd < 0){
        printf("open lcd error\n");
        return -1;
    }

    //2.给液晶屏文件映射一块内存
    int *p = mmap(NULL,800*480*4,PROT_READ | PROT_WRITE,MAP_SHARED,lcd_fd,0);
    if(p == MAP_FAILED)
    {
        perror("mmap is error\n");
        return -2;
    }

    for(int x = 0;x<800;x++)
    {
        for(int y = 0;y<480;y++)
        {
            if(y > x && y < (-x + 379))
            {
                *(p+y*800+x) = 0x00FF69B4;
            }
            else if (y < x && y < (-x + 379))
            {
                *(p+y*800+x) = 0x00BA55D3;
            }
            else if (y > x && y > (-x + 379))
            {
                *(p+y*800+x) = 0x00000080;
            }
            else if (y < x && y > (-x + 379))
            {
                *(p+y*800+x) = 0x003CB371;
            }
        }
    }

    //4.解除映射
    munmap(p,800*480*4);

    //5.关闭液晶屏文件
    close(lcd_fd);

    return 0;
}