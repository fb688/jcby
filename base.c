#include <stdio.h>          // 标准输入输出
#include <string.h>         // 字符串处理
#include <stdlib.h>         // 标准库函数
#include <sys/types.h>      // 系统数据类型
#include <sys/stat.h>       // 文件状态
#include <fcntl.h>          // 文件控制
#include <unistd.h>         // Unix 标准函数
#include <linux/input.h>    // 输入设备
#include <sys/mman.h>       // 内存映射
#include <signal.h>         // 信号处理
#include <pthread.h>        // 线程
#include <mqueue.h>         // POSIX 消息队列

#define MAX_QUEUE_SIZE 10

//---------------- 系统状态定义 ----------------
typedef enum {
    SYS_MAIN_MENU,          // 主菜单
    SYS_PHOTO_ALBUM,        // 相册
    SYS_AUDIO_PLAYER,       // 音乐播放器
    SYS_VIDEO_PLAYER,       // 视频播放器
    SYS_GARAGE_CAM,         // 车库摄像头
    SYS_LICENSE_PLATE       // 车牌识别
} SystemState;

// 线程安全的环形队列
typedef struct {
    SystemMessage messages[MAX_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty; // 队列非空条件
    pthread_cond_t not_full;  // 队列未满条件
} ThreadSafeQueue;

//---------------- 消息类型标识 ----------------
typedef enum {
    MSG_TOUCH_EVENT,        // 触摸事件
    MSG_KEY_EVENT,          // 按键事件
    MSG_RFID_EVENT,         // RFID刷卡事件
    MSG_CAMERA_EVENT,       // 摄像头事件
    MSG_SYSTEM_CMD          // 系统控制命令（线程切换指令）
} MessageType;

//---------------- 触摸事件结构体 ----------------
typedef struct {
    int x;                  // 触摸点X坐标
    int y;                  // 触摸点Y坐标
    int gesture;            // 手势类型 0:点击 1:上滑 2:下滑 3:左滑 4:右滑
} TouchEvent;

//---------------- 消息存放结构体 ----------------
typedef struct {
    MessageType type;       // 消息类型标识
    union {                 // 联合体承载实际数据
        TouchEvent touch;           // 触摸事件数据
        char rfid_data[32];         // RFID数据
        char license_plate[16];     // 车牌号
        int system_cmd;             // 系统命令
    } data;
} SystemMessage;

//---------------- 进度条参数结构体 ----------------
typedef struct {
    int bar_y;              // 进度条Y坐标
    int bar_h;              // 进度条高度
    int bar_w;              // 进度条宽度
    int bar_color;          // 进度条颜色
} ProgressBar;

//---------------- 系统设备路径结构体 ----------------
typedef struct {
    const char *touch_dev;  // 触摸设备路径"/dev/input/event0"
    const char *show_dev;  // 显示屏路径"/dev/fb0"
} SystemPaths;

//---------------- 显示屏结构体 ----------------
struct Dev_init {
    int lcd_fd;
    int *fd;
};

//---------------- 定义视频播放上下文结构体 ----------------
typedef struct {
    FILE *mplayer_fp;
    int fifo_fd;
    int play;
    int is_paused;
    int video_index;
    int video_total;
    pthread_t tid;
    int thread_running;
    struct Dev_init dev;
} VideoContext;

//---------------- 视频路径结构体 ----------------
typedef struct {
    const char *video_list[4];  // 视频文件路径数组
    int video_count;            // 视频数量
} VideoDir;

//---------------- 全局变量与函数声明 ----------------
volatile SystemState current_state = SYS_MAIN_MENU; // 当前系统状态
ThreadSafeQueue system_queue;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER; // 状态互斥锁
ProgressBar bar = {401, 15, 800, 0x00FF69B4};// 进度条对象
const SystemPaths sys_paths = // 全局设备路径
{
    .touch_dev = "/dev/input/event0",
    .show_dev  = "/dev/fb0"
};
const VideoDir video_dir = // 视频路径
{
    .video_list = 
    {
        "blackpink.avi",
        "ladygaga.avi",
        "PineappleStar1.avi",
        "PineappleStar2.avi"
    },
    .video_count = 4
};

// 模块线程函数声明
void* main_menu(void* arg);
void* photo_album(void* arg);
void* audio_player(void* arg);
void* video_player(void* arg);
void* garage_camera(void* arg);
void* license_plate_recognition(void* arg);
void* touch_monitor_thread(void* arg);
//进度条刷新线程
void *progress_thread(void *arg);


// 状态切换函数
void change_state(SystemState new_state);
//初始化消息队列
void queue_init();
// 销毁队列资源
void queue_destroy();
// 消息发送函数
void send_message(SystemMessage msg);
//消息接收
int receive_message(SystemMessage *msg, int timeout_ms);
//初始化屏幕
int dev_init(struct Dev_init *dev);
//关闭屏幕
void dev_close(struct Dev_init *dev);
//显示bmp图片
int show_bmp(char *bmp_name,struct Dev_init *dev);
//获取点击和滑动
int read_Touch(int touch_fd, int *x ,int *y);
//---------------- 视频播放相关模块 ----------------
//画进度条
void draw_progress_bar(int *fbp, int percent);
//获取进度百分比
float get_mplayer_value(VideoContext *ctx, const char *cmd);
//播放选定视频
void start_video(VideoContext *ctx, int index);
// 修改视频切换函数
void switch_video(VideoContext *ctx, int direction);

int main()
{
    // 初始化队列
    queue_init();
    // 创建触摸监测线程
    pthread_t touch_thread;
    pthread_create(&touch_thread, NULL, touch_monitor_thread, NULL);
    pthread_detach(touch_thread); // 分离线程，无需join
    
    pthread_t current_module;
    int ret;
    
    while(1) 
    {
        switch(current_state) 
        {
            case SYS_MAIN_MENU:
                printf("进入主菜单...\n");
                ret = pthread_create(&current_module, NULL, main_menu, NULL);
                break;
                
            case SYS_PHOTO_ALBUM:
                printf("进入电子相册...\n");
                ret = pthread_create(&current_module, NULL, photo_album, NULL);
                break;
                
            case SYS_AUDIO_PLAYER:
                printf("进入音频播放器...\n");
                ret = pthread_create(&current_module, NULL, audio_player, NULL);
                break;
                
            // 其他模块类似处理...
            default:
                printf("未知状态: %d\n", current_state);
                change_state(SYS_MAIN_MENU);
                continue;
        }
        
        if(ret != 0) 
        {
            perror("线程创建失败");
            break;
        }
        
        // 等待模块结束
        pthread_join(current_module, NULL);
        printf("模块退出\n");
        
        // 检查状态变化 - 通过消息队列接收状态切换命令
        SystemMessage msg;
        if (receive_message(&msg, 100)) 
        { 
            if (msg.type == MSG_SYSTEM_CMD) 
            {
                change_state(msg.data.system_cmd);
            }
        }
    }
    // 销毁队列资源
    queue_destroy();
    return 0;
}

//---------------- 函数实现 ----------------
//初始化消息队列
void queue_init() 
{
    system_queue.head = 0;
    system_queue.tail = 0;
    system_queue.count = 0;
    pthread_mutex_init(&system_queue.lock, NULL);
    pthread_cond_init(&system_queue.not_empty, NULL);
    pthread_cond_init(&system_queue.not_full, NULL);
}
// 发送消息 (替代 mq_send)
void send_message(SystemMessage msg) 
{
    pthread_mutex_lock(&system_queue.lock);
    
    // 等待队列有空间
    while (system_queue.count >= MAX_QUEUE_SIZE) {
        pthread_cond_wait(&system_queue.not_full, &system_queue.lock);
    }
    
    system_queue.messages[system_queue.tail] = msg;
    system_queue.tail = (system_queue.tail + 1) % MAX_QUEUE_SIZE;
    system_queue.count++;
    
    // 通知消费者
    pthread_cond_signal(&system_queue.not_empty);
    pthread_mutex_unlock(&system_queue.lock);
}

// 接收消息 (替代 mq_receive)
int receive_message(SystemMessage *msg, int timeout_ms) 
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += timeout_ms * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec %= 1000000000;
    }
    
    pthread_mutex_lock(&system_queue.lock);
    
    int ret = 0;
    while (system_queue.count == 0) {
        if (pthread_cond_timedwait(&system_queue.not_empty, &system_queue.lock, &ts) != 0) {
            // 超时
            pthread_mutex_unlock(&system_queue.lock);
            return 0;
        }
    }
    
    *msg = system_queue.messages[system_queue.head];
    system_queue.head = (system_queue.head + 1) % MAX_QUEUE_SIZE;
    system_queue.count--;
    
    // 通知生产者
    pthread_cond_signal(&system_queue.not_full);
    pthread_mutex_unlock(&system_queue.lock);
    return 1;
}
// 销毁队列资源
void queue_destroy() 
{
    pthread_mutex_destroy(&system_queue.lock);
    pthread_cond_destroy(&system_queue.not_empty);
    pthread_cond_destroy(&system_queue.not_full);
}
// 状态切换函数
void change_state(SystemState new_state) 
{
    pthread_mutex_lock(&state_mutex);
    printf("状态切换: %d -> %d\n", current_state, new_state);
    current_state = new_state;
    pthread_mutex_unlock(&state_mutex);
}

//初始化屏幕
int dev_init(struct Dev_init *dev)
{
    //打开LCD液晶屏文件
    dev->lcd_fd = open(sys_paths.show_dev,O_RDWR);
    //判断LCD液晶屏文件是否成功打开
    if(dev->lcd_fd < 0)
    {
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
    //创建图片的一个数组来进行保存图片信息
    char bmp_buf[800*80*3] = {0};
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
            *(dev->fd+(y)*800+x) = color;
            
        }
    }
    
    close(bmp_fd);
}

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
        return 0;
    }
    else
    {
        if(abs(temp_x) > abs(temp_y))
        {
            //左右滑动
            if(temp_x > 0)
            {
                return 4;//右滑
            }
            else
            {
                return 3;//左滑
            }
            
        }
        else
        {
            //上下滑动
            if(temp_y > 0)
                return 2;//下滑
            else
                return 1;//上滑
        }
    }
    
}

//画进度条
void draw_progress_bar(int *fbp, int percent)
{
    int pos = (percent * bar.bar_w) / 100;

    for (int y = bar.bar_y; y < bar.bar_y + bar.bar_h; y++) 
    {
        for (int x = 0; x < bar.bar_w; x++) 
        {
            if (x < pos)
                fbp[y * 800 + x] = bar.bar_color;
        }
    }
}

//获取进度百分比
float get_mplayer_value(VideoContext *ctx, const char *cmd)
{
    if (!ctx->mplayer_fp || ctx->fifo_fd < 0) return -1;
    
    char buf[128];
    write(ctx->fifo_fd, cmd, strlen(cmd));
    usleep(200000);
    
    // 添加超时机制
    time_t start = time(NULL);
    while (time(NULL) - start < 2) { // 2秒超时
        if (fgets(buf, sizeof(buf), ctx->mplayer_fp)) 
        {
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

//播放选定视频
void start_video(VideoContext *ctx, int index)
{
    if (ctx->mplayer_fp) 
    {
        // 先发送退出命令
        write(ctx->fifo_fd, "quit\n", 5);
        usleep(500000);  // 等待退出完成
        
        pclose(ctx->mplayer_fp);
        ctx->mplayer_fp = NULL;
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
            "mplayer -slave -quiet -input file=/tmp/myfifo -vo fbdev2 -zoom -x 800 -y 400 %s", 
            video_dir.video_list[index]);
    
    ctx->mplayer_fp = popen(cmd, "r");
    if (!ctx->mplayer_fp) {
        perror("启动MPlayer失败");
        return;
    }
    
    // 重置播放状态
    ctx->play = 1;
    ctx->is_paused = 0;
    
    // 确保MPlayer准备就绪
    usleep(1000000);  // 等待1秒
}

// 修改视频切换函数
void switch_video(VideoContext *ctx, int direction)
{
    // 停止当前进度条线程
    if (ctx->thread_running) 
    {
        pthread_cancel(ctx->tid);
        pthread_join(ctx->tid, NULL);
        ctx->thread_running = 0;
    }
    
    // 停止当前播放
    if (ctx->mplayer_fp) 
    {
        write(ctx->fifo_fd, "quit\n", 5);
        usleep(300000); // 等待退出完成
        pclose(ctx->mplayer_fp);
        ctx->mplayer_fp = NULL;
    }
    
    // 切换视频索引
    ctx->video_index = (ctx->video_index + direction + ctx->video_total) % ctx->video_total;
    
    // 启动新视频
    start_video(ctx, ctx->video_index);
    
    // 重置状态
    ctx->play = 1;
    ctx->is_paused = 0;
    show_bmp("start.bmp", &(ctx->dev));
    
    // 创建新进度条线程
    pthread_create(&(ctx->tid), NULL, progress_thread, &(ctx->dev));
    ctx->thread_running = 1;
}

void* main_menu(void* arg)
{

}
void* photo_album(void* arg)
{

}
void* audio_player(void* arg)
{

}
void* video_player(void* arg)
{

}
void* garage_camera(void* arg)
{

}
void* license_plate_recognition(void* arg)
{

}
void* touch_monitor_thread(void* arg)
{
    int touch_fd = open(sys_paths.touch_dev, O_RDONLY);
    if(touch_fd < 0) 
    {
        perror("open touch failed");
        return NULL;
    }
    
    TouchEvent touch_event = {0};
    while(1)
    {
        touch_event.gesture = read_Touch(touch_fd, &touch_event.x, &touch_event.y);
        // 发送触摸消息
        SystemMessage msg = 
        {
            .type = MSG_TOUCH_EVENT,
            .data.touch = touch_event
        };
        mq_send(system_queue, (char*)&msg, sizeof(msg), 0);
    }
    close(touch_fd);
    return NULL;
}
void *progress_thread(void *arg)
{
    VideoContext *ctx = (VideoContext *)arg;
    while (1) 
    {
        // 仅在播放状态更新进度条
        if (!ctx->is_paused) 
        {
            float pos = get_mplayer_value(ctx, "get_time_pos\n");
            float len = get_mplayer_value(ctx, "get_time_length\n");
            if (pos >= 0 && len > 0) 
            {
                int percent = (int)((pos / len) * 100.0f);
                draw_progress_bar(ctx->dev.fd, percent);
            }
        }
        usleep(500000); // 降低查询频率到0.5秒
    }
    return NULL;
}