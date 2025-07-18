#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <signal.h>
#include <pthread.h>

#define TOUCH_DEV       "/dev/input/event0"
#define BAR_Y 401
#define BAR_H 15
#define BAR_W 800
#define BAR_COLOR 0x00FF69B4
// 视频路径数组
char *video_list[] = 
{
    "blackpink.avi",
    "ladygaga.avi",
    "PineappleStar1.avi",
    "PineappleStar2.avi"
};
//初始化屏幕
struct Dev_init 
{
    int lcd_fd;
    int *fd;
};

int video_index = 0;
FILE *mplayer_fp = NULL;
int fifo_fd = -1;
int video_total = sizeof(video_list) / sizeof(video_list[0]);
int play = 1;
// 全局添加暂停状态标志0=播放, 1=暂停
int is_paused = 0;
//线程相关
pthread_t tid;
int thread_running = 0;

struct Dev_init dev;

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
    munmap(dev->fd,800*80*4);
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

    
    // //测试
    // char bmp_information[54]={0};
    // read(bmp_fd,bmp_information,54);
    // // printf("bmp文件的信息是:%s\n",bmp_information);
    lseek(bmp_fd,54,SEEK_SET);
    //读取图片
    //创建图片的一个数组来进行保存图片信息
    char bmp_buf[800*80*3] = {0};
    read(bmp_fd,bmp_buf,sizeof(bmp_buf));

    int r,g,b,color;
    int i=0;
    for(int y = 400;y<479;y++)
    {
        for(int x = 0;x<800;x++)
        {
            b = bmp_buf[i++];
            g = bmp_buf[i++];
            r = bmp_buf[i++];
            color = b | g<<8 | r<<16;
            *(dev->fd+(y)*800+x) = color;
            
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

enum {UP , DOWN , LEFT ,RIGHT , CLICK};

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

//画进度条
void draw_progress_bar(int *fbp, int percent) 
{
    int bar_y = BAR_Y;
    int bar_h = BAR_H;
    int bar_w = (percent * BAR_W) / 100;
    int color = BAR_COLOR;

    for (int y = bar_y; y < bar_y + bar_h; y++) 
    {
        for (int x = 0; x < BAR_W; x++) 
        {
            if (x < bar_w)
                fbp[y * 800 + x] = color;
        }
    }
}

//获取进度百分比
float get_mplayer_value(FILE *fp, int fifo_fd, const char *cmd) 
{
    if (!fp || fifo_fd < 0) return -1;
    
    char buf[128];
    write(fifo_fd, cmd, strlen(cmd));
    usleep(200000);
    
    // 添加超时机制
    time_t start = time(NULL);
    while (time(NULL) - start < 2) { // 2秒超时
        if (fgets(buf, sizeof(buf), fp)) {
            if (strncmp(buf, "ANS_", 4) == 0) {
                float val = 0;
                sscanf(buf, "ANS_%*[^=]=%f", &val);
                return val;
            }
        }
        usleep(10000);
    }
    return -1;
}

//进度条刷新线程
void *progress_thread(void *arg) 
{
    struct Dev_init *dev = (struct Dev_init *)arg;
    while (1) 
    {
        // 仅在播放状态更新进度条
        if (!is_paused) {
            float pos = get_mplayer_value(mplayer_fp, fifo_fd, "get_time_pos\n");
            float len = get_mplayer_value(mplayer_fp, fifo_fd, "get_time_length\n");
            if (pos >= 0 && len > 0) 
            {
                int percent = (int)((pos / len) * 100.0f);
                draw_progress_bar(dev->fd, percent);
            }
        }
        usleep(500000); // 降低查询频率到0.5秒
    }
    return NULL;
}

//播放选定视频
void start_video(int index) 
{
    if (mplayer_fp) 
    {
        // 先发送退出命令
        write(fifo_fd, "quit\n", 5);
        usleep(500000);  // 等待退出完成
        
        pclose(mplayer_fp);
        mplayer_fp = NULL;
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
            "mplayer -slave -quiet -input file=/tmp/myfifo -vo fbdev2 -zoom -x 800 -y 400 %s", 
            video_list[index]);
    
    mplayer_fp = popen(cmd, "r");
    if (!mplayer_fp) {
        perror("启动MPlayer失败");
        return;
    }
    
    // 重置播放状态
    play = 1;
    is_paused = 0;
    
    // 确保MPlayer准备就绪
    usleep(1000000);  // 等待1秒
}

// 修改视频切换函数
void switch_video(int direction) 
{
    // 停止当前进度条线程
    if (thread_running) 
    {
        pthread_cancel(tid);
        pthread_join(tid, NULL);
        thread_running = 0;
    }
    
    // 停止当前播放
    if (mplayer_fp) 
    {
        write(fifo_fd, "quit\n", 5);
        usleep(300000); // 等待退出完成
        pclose(mplayer_fp);
        mplayer_fp = NULL;
    }
    
    // 切换视频索引
    video_index = (video_index + direction + video_total) % video_total;
    
    // 启动新视频
    start_video(video_index);
    
    // 重置状态
    play = 1;
    is_paused = 0;
    show_bmp("start.bmp", &dev);
    
    // 创建新进度条线程
    pthread_create(&tid, NULL, progress_thread, &dev);
    thread_running = 1;
}

int main(void)
{
	int x, y, touch_fd, slide;
    
    
    touch_init(&touch_fd);
    dev_init(&dev);

	//创建管道文件
	int ret = mkfifo("/tmp/myfifo", 777);
	if(ret == -1)
	{
		perror("创建管道文件失败");
	}
	
	//打开管道文件
	fifo_fd = open("/tmp/myfifo", O_RDWR);
	if(fifo_fd == -1)
	{
		perror("打开管道文件失败");
		return -1;
	}
	
	//播放视频
    start_video(video_index);
	show_bmp("start.bmp",&dev);

    pthread_create(&tid, NULL, progress_thread, &dev);
	while(1)
	{
        slide = read_Touch(touch_fd, &x, &y);
        //定点
        // if (slide == CLICK)
        //     printf("(%d,%d)\n",x,y);
        if (slide == LEFT)
        {
            write(fifo_fd,"seek -5\n",strlen("seek -5\n"));//快退
            // write(fd,"volume +5\n",strlen("volume +5\n"));//加声音
            // write(fd,"volume -5\n",strlen("volume -5\n"));//减声音
            // write(fd,"seek -5\n",strlen("seek -5\n"));//快退。
            // write(fd,"seek +5\n",strlen("seek +5\n"));//快进。
            // write(fd,"pause\n",strlen("pause\n"));//暂停播放
            // write(fd,"seek 100 1\n",strlen("seek 100 1\n"));//进度跳转
        }
        else if (slide == RIGHT)
        {
            printf("(%d,%d)\n",x,y);
            write(fifo_fd,"seek +5\n",strlen("seek +5\n"));//快进
        }
        else if (slide == UP)
        {
            printf("(%d,%d)\n",x,y);
            write(fifo_fd,"volume +5\n",strlen("volume +5\n"));//加声音
        }
        else if (slide == DOWN)
        {
            printf("(%d,%d)\n",x,y);
            write(fifo_fd,"volume -5\n",strlen("volume -5\n"));//减声音
        }
/********************暂停、开始************************/
        else if (slide == CLICK && x > 350 && x < 450 && y > 458 && y < 480)
        {
            // 确保命令正确发送
            int n = write(fifo_fd, "pause\n", 6);  //长度是6
            
            if (n != 6) 
            {
                perror("pause命令发送失败");
            } 
            else 
            {
                // 切换状态
                if (play == 1) 
                {
                    play = 0;
                    is_paused = 1;
                    show_bmp("pause.bmp", &dev);
                    printf("已暂停\n");
                } 
                else 
                {
                    play = 1;
                    is_paused = 0;
                    show_bmp("start.bmp", &dev);
                    printf("已播放\n");
                }
            }
            
            // 增加足够的延迟确保MPlayer处理命令
            usleep(300000);  // 300ms延迟
        }

/******************视频切换**************************/
        else if (slide == CLICK && x > 240 && x < 260 && y > 458 && y < 480)//上一个
        {
            switch_video(-1); // 上一个视频
        }
        else if (slide == CLICK && x > 505 && x < 560 && y > 458 && y < 480)//下一个
        {
            switch_video(1); // 下一个视频
        }
        else if (slide == CLICK && y >= BAR_Y && y < BAR_Y + BAR_H) 
        {
            // 确保在播放状态
            if (!is_paused) {
                float len = get_mplayer_value(mplayer_fp, fifo_fd, "get_time_length\n");
                if (len > 0) 
                {
                    float ratio = (float)x / BAR_W;
                    int seek_sec = (int)(ratio * len);
                    char cmd[64];
                    snprintf(cmd, sizeof(cmd), "seek %d 2\n", seek_sec);
                    write(fifo_fd, cmd, strlen(cmd));
                    
                    // 立即更新进度条
                    int percent = (int)(ratio * 100);
                    draw_progress_bar(dev.fd, percent);
                }
            }
        }
	}
	
	//关闭管道文件
	close(fifo_fd);
	touch_close(touch_fd);
    dev_close(&dev);
    if (mplayer_fp) pclose(mplayer_fp);

    // 在main函数返回前
    if (thread_running) 
    {
        pthread_cancel(tid);
        pthread_join(tid, NULL);
    }
	return 0;
}
