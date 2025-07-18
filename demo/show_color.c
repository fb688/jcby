/*
    在液晶屏显示颜色的步骤
    1、打开液晶屏文件
    2、准备颜色数据
    3、将颜色数据写入到液晶屏中
    4、关闭液晶屏文件
*/
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{

    //1.打开液晶屏文件
    int lcd_fd = open("/dev/fb0",O_RDWR);
    if(lcd_fd < 0)
    {
        perror("Open lcd_fd error\n");
        return -1;
    }

    //2.准备颜色的数据
    int color_buf[800*480] = {0};
    int x,y;
    for(x = 0;x<800;x++)
    {
        for(y = 0;y < 480;y++)
        {
            if (y >= 0 && y < 159)
            {
                color_buf[y*800+x] = 0x00FFB6C1;
            }
            else if (y >= 159 && y < 319)
            {
                color_buf[y*800+x] = 0x00FFA500;
            }
            else if (y >= 319 && y < 479)
            {
                color_buf[y*800+x] = 0x007FFFAA;
            }
        }
    }
    //3.将准备好的颜色写入到lcd屏幕中
    write(lcd_fd,color_buf,sizeof(color_buf));

    //4.关闭lcd屏幕，防止内存刷爆
    close(lcd_fd);
}