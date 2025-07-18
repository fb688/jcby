#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/mman.h>

#define TOUCH_DEV       "/dev/input/event0"

// #define TOUCH    0x0
// #define LEFT     0x1

//初始化屏幕
struct Dev_init{
    int lcd_fd;
    int *fd;
};
int dev_init(struct Dev_init *dev)
{
    //打开LCD液晶屏文件
    dev->lcd_fd = open("/dev/fb0",O_RDWR);
    //判断LCD液晶屏文件是否成功打开
    if(dev->lcd_fd < 0){
        printf("open lcd error\n");
        return -1;
    }

    //进行内存映射
    dev->fd = mmap(NULL,800*480*4,PROT_READ | PROT_WRITE,MAP_SHARED,dev->lcd_fd,0);
    //判断内存映射是否创建成功
    if(dev->fd == MAP_FAILED){
        printf("creat mmap error\n");
        return -3;
    }
}

//关闭屏幕
void dev_close(struct Dev_init *dev)
{
    munmap(dev->fd,800*480*4);
    close(dev->lcd_fd);
}

//显示bmp图片
int show_bmp(char *bmp_name,struct Dev_init *dev)
{
     //打开bmp图片
    int bmp_fd = open(bmp_name,O_RDWR);
    if(bmp_fd<0)
    {
        perror("bmp open error\n");
        return -2;
    }

    
    //测试
    char bmp_information[54]={0};
    read(bmp_fd,bmp_information,54);
    // printf("bmp文件的信息是:%s\n",bmp_information);
    lseek(bmp_fd,54,SEEK_SET);
    //读取图片
    //创建图片的一个数组来进行保存图片信息
    char bmp_buf[800*480*3] = {0};
    read(bmp_fd,bmp_buf,sizeof(bmp_buf));

    int r,g,b,color;
    int i=0;
    for(int y = 0;y<480;y++)
    {
        for(int x = 0;x<800;x++)
        {
            b = bmp_buf[i++];
            g = bmp_buf[i++];
            r = bmp_buf[i++];
            color = b | g<<8 | r<<16;
            *(dev->fd+(479-y)*800+x) = color;
            
        }
    }
    
    close(bmp_fd);
}

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

enum  {UP , DOWN , LEFT ,RIGHT , CLICK};

//获取点击和滑动
int read_Touch(int touch_fd, int *x ,int *y)
{
    *x = *y = -1;
    int start_x,start_y,end_x,end_y;
    //定义输入事件结构体
    struct input_event buf;

    while (1)
    {
        read(touch_fd , &buf , sizeof buf );

        if(buf.type == EV_ABS) //是触摸屏事件
        {
            if(buf.code == ABS_X)   //横坐标
            {
                *x = buf.value;
                *x = *x *800/1024;  //黑色屏幕
            }
            else if (buf.code == ABS_Y) //纵坐标
            {
                *y = buf.value;
                *y = *y*480/600;   //黑色屏幕
            }
        }
        if(buf.type == EV_KEY && buf.code == BTN_TOUCH)
        {
            if(buf.value == 0)//松开
            {
                //记录终点坐标
                end_x = *x;
                end_y = *y;
                break;
            }
            else if (buf.value == 1)    //按下
            {
                //记录起点坐标
                start_x = *x;
                start_y = *y;
            }
        }
    }

    //终点坐标 减 起点坐标
    int temp_x = end_x - start_x;
    int temp_y = end_y - start_y;

    //区分是点击还是滑动
    if(abs(temp_x) < 20 && abs(temp_y) <20)
    {
        //点击
        return CLICK;
    }
    else
    {
        if(abs(temp_x) > abs(temp_y))
        {
            //左右滑动
            if(temp_x > 0)
            {
                return RIGHT;
            }
            else
            {
                return LEFT;
            }
            
        }
        else
        {
            //上下滑动
            if(temp_y > 0)
                return DOWN;
            else
                return UP;
        }
    }
    
}

void chose_bmp(struct Dev_init *dev, int slide_count)
{
    switch (slide_count)
    {
        case 0:
            show_bmp("1.bmp",dev);
            break;
        case 1:
            show_bmp("2.bmp",dev);
            break;
        case 2:
            show_bmp("3.bmp",dev);
            break;
        case 3:
            show_bmp("4.bmp",dev);
            break;
        case 4:
            show_bmp("5.bmp",dev);
            break;
        case 5:
            show_bmp("6.bmp",dev);
            break;
        case 6:
            show_bmp("7.bmp",dev);
            break;
        case 7:
            show_bmp("8.bmp",dev);
            break;
        case 8:
            show_bmp("9.bmp",dev);
            break;
    }
}

int main(void)
{
    int x, y;
    int touch_fd;
    struct Dev_init dev;
    int slide, slide_count = 0;
    
    dev_init(&dev);
    touch_init(&touch_fd);
    
    while(1)
    {
        chose_bmp(&dev, slide_count);
        slide = read_Touch(touch_fd, &x, &y);
        if (slide == LEFT)
        {
            ++slide_count;
            if (slide_count > 8)
                slide_count = 0;
            chose_bmp(&dev, slide_count);
        }
        else if (slide == RIGHT)
        {
            --slide_count;
            if (slide_count < 0)
                slide_count = 8;
            chose_bmp(&dev, slide_count);
        }
    }
    
    touch_close(touch_fd);
    dev_close(&dev);
    return 0;
}

