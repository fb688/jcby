//标准输入输出
#include <stdio.h>
//触摸屏结构体
#include <linux/input.h>
//open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//close
#include <unistd.h>

int main(void)
{
    
    //打开触摸屏文件
    int ts_fd = open("/dev/input/event0",O_RDWR);
    if(ts_fd < 0)
    {
        printf("触摸屏打开失败\n");
        return -1;
    }

    int x,y;
    //获取触摸屏的数据
    struct input_event buf;
    while(1)
    {
        while(1)
        {
            read(ts_fd, &buf, sizeof(buf));
            //判定是否为触摸屏事件
            if(buf.type == EV_ABS)
            {
                if(buf.code == ABS_X)//X轴事件
                {
                    x = buf.value*800/1024;//黑色开发板
                    //x = buf.value;//蓝色开发板
                }
                if(buf.code == ABS_Y)//y轴事件
                {
                    y = buf.value*480/600;//黑色开发板
                    //y = buf.value;//蓝色开发板
                }
            }
            //判定手指是否松开,触摸屏也是类似与按键事件，松开就是
            if(buf.type == EV_KEY && buf.code == BTN_TOUCH && buf.value == 0)
            {
                printf("(%d,%d)\n",x,y);
                break;
            }
        }
    }
    
    return 0;
}