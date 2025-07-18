
/*
    LCD液晶屏显示图片步骤
    1、打开LCD液晶屏文件
    2、打开bmp图片
    3、偏移54个字节     //lseek：read、write会使文件光标自动偏移
    4、读取图片的数据
    5、给LCD像素点一一赋值
    6、写入到LCD显示屏文件中
    7、释放内存映射关闭LCD和BMP
*/
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

int main()
{

    //打开LCD液晶屏文件
    int lcd_fd = open("/dev/fb0",O_RDWR);
    //判断LCD液晶屏文件是否成功打开
    if(lcd_fd < 0){
        printf("open lcd error\n");
        return -1;
    }

    //2.打开bmp图片
    int bmp_fd = open("1.bmp",O_RDWR);
    if(bmp_fd<0)
    {
        perror("bmp open error\n");
        return -2;
    }

    //3.进行内存映射
    int *p = mmap(NULL,800*480*4,PROT_READ | PROT_WRITE,MAP_SHARED,lcd_fd,0);
    //判断内存映射是否创建成功
    if(p == MAP_FAILED){
        printf("creat mmap error\n");
        return -3;
    }

    //创建图片的一个数组来进行保存图片信息
    char bmp_buf[800*480*3] = {0};

    //测试
    char bmp_information[54]={0};
    read(bmp_fd,bmp_information,54);
    printf("bmp文件的信息是:%s\n",bmp_information);

    //读取图片
    lseek(bmp_fd,54,SEEK_SET);
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
            *(p+(479-y)*800+x) = color;
            
        }
    }
    munmap(p,800*480*4);
    close(bmp_fd);
    close(lcd_fd);
    return 0;


}