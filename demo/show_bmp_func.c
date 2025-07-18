//标准输入输出头文件
#include <stdio.h>
//open头文件
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//read、write、close头文件
#include <unistd.h>
//内存映射头文件
#include <sys/mman.h>

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

void dev_close(struct Dev_init *dev)
{
    munmap(dev->fd,800*480*4);
    close(dev->lcd_fd);
}

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

int main()
{

    struct Dev_init dev;
    
    dev_init(&dev);
    show_bmp("3.bmp",&dev);
    dev_close(&dev);

   
    
    return 0;


}