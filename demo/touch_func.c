#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

#define TOUCH_DEV       "/dev/input/event0"

#define TOUCH    0x0
#define LEFT     0x1


//初始化触摸屏
void touch_init(int *fd)
{
    //1. 打开触摸屏
    *fd = open(TOUCH_DEV, O_RDONLY);
    if(-1 == *fd)
    {
        perror("open touch fail");
        exit(1);
    }
}

//关闭触摸屏
void touch_close(int fd)
{
    // 3. 关闭触摸屏
    close(fd);
}

//获取坐标
void getxy(int fd, int *x, int *y)
{
    // 2. 读取触摸屏数据
    struct input_event buf;
    
    while(1)
    {
        read(fd, &buf, sizeof(buf));
        
        if(buf.type == EV_ABS)  //绝对事件
        {
            if(buf.code == ABS_X)   //x轴
            {
                *x = buf.value; //蓝色边框屏幕
                // *x = buf.value * 800 / 1024; //黑色边框屏幕
            }
            else if(buf.code == ABS_Y)  //y轴
            {
                *y = buf.value; //蓝色边框屏幕
                // *y = buf.value * 480 / 600;  //黑色边框屏幕
            }
            else
            {
                /* empty */;
            }
        }
        else if(buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if(buf.value == 0)
            {
                printf("松开\n");
                break;
            }
            else
            {
                printf("按下\n");
            }
        }
        else
        {
            /* empty */;
        }
        
        // printf("(%d,%d)\n", *x, *y);
    }   //while(1)
}


int main(void)
{
    int x, y;
    int touch_fd;
    
    touch_init(&touch_fd);
    
    while(1)
    {
        getxy(touch_fd, &x, &y);
        
        // if(x? y?)
        // {
            
        // }
    }
    
    
    touch_close(touch_fd);

    return 0;
}