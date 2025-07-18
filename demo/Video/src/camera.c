#include <stdio.h>           // 标准输入输出库
#include <sys/types.h>       // 系统类型定义
#include <sys/stat.h>        // 文件状态信息
#include <fcntl.h>           // 文件控制操作
#include <unistd.h>           // UNIX标准函数
#include <sys/mman.h>        // 内存映射支持
#include <linux/input.h>     // Linux输入子系统
#include <string.h>          // 字符串处理函数
#include "jpeglib.h"         // JPEG图像处理库
#include "api_v4l2.h"        // V4L2摄像头API
#include <stdlib.h>          // 标准库函数
#include <pthread.h>         // 线程支持
#include <poll.h>            // I/O多路复用
#include <errno.h>           // 错误号定义
#include <time.h>            // 时间处理函数
#include <sys/stat.h>        // 文件状态操作

#define LCD_WIDTH 800        // LCD屏幕宽度
#define LCD_HEIGHT 480       // LCD屏幕高度

// 全局变量声明
int *p = NULL;               // LCD帧缓冲映射指针
int lcd_fd = -1;             // LCD设备文件描述符
int is_open_camera = 0;      // 摄像头开启状态标志
int is_video_play = 0;       // 视频播放状态标志
int is_vedio_record = 0;     // 视频录制状态标志
int take_photo = 0;          // 拍照触发标志
int video_count = 0;         // 视频帧计数器
FrameBuffer camera_buf;      // 摄像头帧缓冲区结构体
int video_fd;                // 视频文件描述符
char bmp[3][10] =            // BMP图片文件名数组
{
    "1.bmp",
    "2.bmp",
    "girl.bmp"
};

// 触摸屏相关变量
int ts_fd = -1;                              // 触摸屏设备文件描述符
pthread_mutex_t touch_mutex = PTHREAD_MUTEX_INITIALIZER;  // 触摸事件互斥锁
struct TouchEvent {                          // 触摸事件结构体
    int x;                                   // X坐标
    int y;                                   // Y坐标
    int valid;                               // 事件有效性标志
} last_touch = {0, 0, 0};                   // 最后触摸事件记录

// LCD初始化函数
int lcd_open(void) {
    lcd_fd = open("/dev/fb0", O_RDWR);       // 打开LCD帧缓冲设备
    if (lcd_fd < 0) {
        perror("lcd open failed");           // 错误处理
        return -1;
    }
    // 内存映射LCD帧缓冲
    p = mmap(NULL, LCD_WIDTH * LCD_HEIGHT * 4,
             PROT_READ | PROT_WRITE,         // 读写权限
             MAP_SHARED,                     // 共享映射
             lcd_fd,                         // 目标文件描述符
             0);                             // 偏移量
    if (NULL == p) {
        perror("lcd mmap error");            // 映射错误处理
        close(lcd_fd);
        return -1;
    }
    return 0;
}

// 在LCD上绘制单个像素点
int lcd_draw_point(int x, int y, int color) {
    // 坐标边界检查
    if (x >= LCD_WIDTH || x < 0) {
        return -1;
    }
    if (y >= LCD_HEIGHT || y < 0) {
        return -1;
    }
    // 计算像素位置并设置颜色
    *(p + x + y * LCD_WIDTH) = color;
    return 0;
}

// 在LCD上绘制BMP图像
void lcd_draw_bmp(const char *file_name, int x0, int y0) {
    int fd;                                  // 文件描述符
    int bmp_width, bmp_height;               // BMP图像宽高
    char buf[54];                            // BMP文件头缓冲区
    
    // 打开BMP文件
    fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        perror("open file error");
        return;
    }
    
    /* 读取位图头部信息 */
    if (read(fd, buf, 54) != 54) {           // 读取54字节文件头
        perror("read bmp header error");
        close(fd);
        return;
    }

    /* 解析图像宽度 */
    bmp_width = buf[18];                     // 宽度低位字节
    bmp_width |= buf[19] << 8;                // 宽度高位字节
    
    /* 解析图像高度 */
    bmp_height = buf[22];                    // 高度低位字节
    bmp_height |= buf[23] << 8;               // 高度高位字节

    // 分配像素数据缓冲区
    int len = bmp_width * bmp_height * 3;    // 计算像素数据大小
    char *bmp_buf = malloc(len);
    if (!bmp_buf) {
        perror("malloc bmp buffer error");
        close(fd);
        return;
    }

    // 跳过文件头
    lseek(fd, 54, SEEK_SET);
    
    // 读取像素数据
    if (read(fd, bmp_buf, len) != len) {
        perror("read bmp data error");
        free(bmp_buf);
        close(fd);
        return;
    }
    close(fd);  // 关闭文件
    
    // 解码并绘制像素
    int color, x, y, i = 0;
    unsigned char r, g, b;
    for (y = 0; y < bmp_height; y++) {
        for (x = 0; x < bmp_width; x++) {
            b = bmp_buf[i++];                // 蓝色分量
            g = bmp_buf[i++];                // 绿色分量
            r = bmp_buf[i++];                // 红色分量
            color = (r << 16) | (g << 8) | b; // 组合为RGB颜色
            // 绘制像素点（BMP是倒序存储）
            lcd_draw_point(x + x0, bmp_height - y + y0 - 1, color);
        }
    }
    free(bmp_buf);  // 释放缓冲区
}

/* 获取文件大小 */
unsigned long file_size_get(const char *pfile_path) {
    unsigned long filesize = -1;              // 文件大小变量
    struct stat statbuff;                     // 文件状态结构
    
    // 获取文件状态
    if (stat(pfile_path, &statbuff) < 0) {
        return filesize;
    } else {
        filesize = statbuff.st_size;         // 返回文件大小
    }
    
    return filesize;
}

/* 显示JPEG图片 */
int lcd_draw_jpg(unsigned int x, unsigned int y, const char *pjpg_path, char *pjpg_buf, unsigned int jpg_buf_size) {
    struct jpeg_decompress_struct cinfo;      // JPEG解压缩结构
    struct jpeg_error_mgr jerr;               // JPEG错误处理
    // 分配颜色缓冲区
    char *g_color_buf = malloc(LCD_WIDTH * LCD_HEIGHT * 3);
    if (!g_color_buf) {
        perror("malloc color buffer failed");
        return -1;
    }
    
    char *pcolor_buf;                         // 当前行颜色缓冲区
    char *pjpg;                               // JPEG数据指针
    
    unsigned int i = 0;                       // 循环索引
    unsigned int color = 0;                   // 颜色值
    
    unsigned int x_s = x;                     // 起始X坐标
    unsigned int x_e;                         // 结束X坐标
    unsigned int y_e;                         // 结束Y坐标
    
    int jpg_fd;                               // JPEG文件描述符
    unsigned int jpg_size;                    // JPEG文件大小
    unsigned int jpg_width;                   // JPEG图像宽度
    unsigned int jpg_height;                  // JPEG图像高度
    
    // 根据输入参数获取JPEG数据
    if (pjpg_path != NULL) {                  // 文件路径模式
        jpg_fd = open(pjpg_path, O_RDONLY);
        if (jpg_fd == -1) {
            printf("open %s error\n", pjpg_path);
            free(g_color_buf);
            return -1;
        }
        
        jpg_size = file_size_get(pjpg_path);  // 获取文件大小
        pjpg = malloc(jpg_size);              // 分配缓冲区
        if (!pjpg) {
            perror("malloc jpg buffer failed");
            close(jpg_fd);
            free(g_color_buf);
            return -1;
        }
        
        // 读取整个JPEG文件
        if (read(jpg_fd, pjpg, jpg_size) != jpg_size) {
            perror("read jpg file failed");
            free(pjpg);
            close(jpg_fd);
            free(g_color_buf);
            return -1;
        }
        close(jpg_fd);  // 关闭文件
    } else {                                  // 内存缓冲区模式
        jpg_size = jpg_buf_size;
        pjpg = pjpg_buf;
    }
    
    // 初始化JPEG解压缩
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, pjpg, jpg_size);     // 设置内存数据源
    jpeg_read_header(&cinfo, TRUE);           // 读取JPEG头
    jpeg_start_decompress(&cinfo);            // 开始解压缩
    
    // 获取图像尺寸
    jpg_width = cinfo.output_width;
    jpg_height = cinfo.output_height;
    
    // 计算绘制区域
    x_e = x_s + jpg_width;
    y_e = y + jpg_height;
    
    // 检查图像是否超出屏幕范围
    if (x_e > LCD_WIDTH || y_e > LCD_HEIGHT) {
        printf("Image too large: %dx%d at (%d,%d)\n", jpg_width, jpg_height, x, y);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        if (pjpg_path != NULL) free(pjpg);
        free(g_color_buf);
        return -1;
    }
    
    // 逐行解码并绘制
    while (cinfo.output_scanline < cinfo.output_height) {
        pcolor_buf = g_color_buf;
        // 读取一行扫描线
        jpeg_read_scanlines(&cinfo, (JSAMPARRAY)&pcolor_buf, 1);
        
        // 处理行内每个像素
        for (i = 0; i < cinfo.output_width; i++) {
            // 组合RGB颜色 (JPEG存储顺序为RGB)
            color = pcolor_buf[2];           // R分量
            color |= pcolor_buf[1] << 8;      // G分量
            color |= pcolor_buf[0] << 16;     // B分量
            // 绘制像素点
            lcd_draw_point(x, y, color);
            pcolor_buf += 3;                  // 移动到下一个像素
            x++;                              // X坐标递增
        }
        y++;                                  // Y坐标递增（下一行）
        x = x_s;                             // 重置X坐标
    }
    
    // 清理JPEG资源
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    
    // 释放资源
    if (pjpg_path != NULL) {
        free(pjpg);                           // 释放文件数据缓冲区
    }
    free(g_color_buf);                        // 释放颜色缓冲区
    return 0;
}

/* 触摸屏处理线程函数 */
void *touch_thread(void *arg) {
    struct input_event ev;                     // 输入事件结构
    int touch_x = 0, touch_y = 0;             // 触摸坐标
    int pressure = -1;                        // 压力值
    
    while (1) {
        // 读取触摸事件
        int ret = read(ts_fd, &ev, sizeof(ev));
        if (ret < 0) {
            if (errno == EAGAIN) {            // 无数据时短暂休眠
                usleep(10000);                // 10ms
                continue;
            }
            perror("read touch event error");
            break;
        }
        
        // 处理绝对坐标事件
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X) {           // X坐标
                touch_x = ev.value;
            } else if (ev.code == ABS_Y) {    // Y坐标
                touch_y = ev.value;
            } else if (ev.code == ABS_PRESSURE) { // 压力值
                pressure = ev.value;
            }
        }
        
        // 检测手指松开事件
        if (ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0) {
            pressure = 0;                     // 标记为释放状态
        }
        
        // 当检测到手指松开时记录触摸位置
        if (pressure == 0) {
            pthread_mutex_lock(&touch_mutex); // 加锁保护共享数据
            last_touch.x = touch_x;           // 记录X坐标
            last_touch.y = touch_y;           // 记录Y坐标
            last_touch.valid = 1;             // 设置有效标志
            pthread_mutex_unlock(&touch_mutex); // 解锁
            pressure = -1;                    // 重置压力状态
        }
    }
    return NULL;
}

/* 摄像头捕获线程函数 */
void *capture_thread(void *arg) {
    char video_name_buf[128];                  // 视频帧文件名缓冲区
    
    while (1) {
        // 摄像头采集处理
        if (is_open_camera) {
            // 获取一帧图像
            if (linux_v4l2_get_fream(&camera_buf) == 0) {
                // 在LCD上显示JPEG图像
                lcd_draw_jpg(80, 0, NULL, camera_buf.buf, camera_buf.length);
                
                // 拍照功能
                if (take_photo) {
                    char photo_name[128];
                    // time_t now = time(NULL);   // 获取当前时间
                    // 生成唯一文件名
                    snprintf(photo_name, sizeof(photo_name), "./a.jpg");
                    int fd = open(photo_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd >= 0) {
                        write(fd, camera_buf.buf, camera_buf.length); // 保存图像数据
                        close(fd);
                        printf("Photo saved: %s\n", photo_name);
                    } else {
                        perror("Failed to save photo");
                    }
                    take_photo = 0;            // 重置拍照标志
                }
                
                // 录像功能
                if (is_vedio_record) {
                    // 生成视频帧文件名
                    snprintf(video_name_buf, sizeof(video_name_buf), "./myvideo/video%d.jpg", video_count);
                    int vfd = open(video_name_buf, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (vfd >= 0) {
                        write(vfd, camera_buf.buf, camera_buf.length); // 保存帧数据
                        close(vfd);
                        video_count++;          // 帧计数器递增
                        printf("Frame saved: %s\n", video_name_buf);
                    } else {
                        perror("Failed to save video frame");
                    }
                }
            }
        }
        
        // 视频播放功能
        if (is_video_play) {
            is_open_camera = 0;                // 暂停摄像头采集
            // 按顺序播放所有视频帧
            for (int i = 0; i < video_count && is_video_play; i++) {
                snprintf(video_name_buf, sizeof(video_name_buf), "./myvideo/video%d.jpg", i);
                // 在LCD上显示帧
                if (lcd_draw_jpg(80, 0, video_name_buf, NULL, 0) != 0) {
                    printf("Failed to display frame %d\n", i);
                }
                usleep(30000);                 // 帧间延迟 (~33fps)
            }
            is_video_play = 0;                 // 停止播放
            printf("Video playback finished\n");
        }
        
        usleep(30000);                         // 主循环延迟 (~33fps)
    }
    return NULL;
}

/* 主摄像头功能 */
void camera(void) {
    pthread_t capture_tid, touch_tid;          // 线程ID
    
    // 初始化触摸屏设备
    ts_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (ts_fd < 0) {
        perror("open touch screen failed");
        return;
    }
    
    // 创建触摸屏处理线程
    if (pthread_create(&touch_tid, NULL, touch_thread, NULL) != 0) {
        perror("create touch thread failed");
        close(ts_fd);
        return;
    }
    
    // 创建摄像头捕获线程
    if (pthread_create(&capture_tid, NULL, capture_thread, NULL) != 0) {
        perror("create capture thread failed");
        close(ts_fd);
        return;
    }
    
    // 显示初始界面
    lcd_draw_bmp("camera.bmp", 0, 0);
    
    // 主触摸事件处理循环
    while (1) {
        int x = 0, y = 0;                     // 触摸坐标
        int valid = 0;                        // 事件有效性标志
        
        // 检查是否有有效的触摸事件
        pthread_mutex_lock(&touch_mutex);      // 加锁
        if (last_touch.valid) {               // 存在有效事件
            // 坐标转换 (黑色触摸屏分辨率1024x600 -> LCD 800x480)
            x = last_touch.x * 800 / 1024;
            y = last_touch.y * 480 / 600;
            // 坐标转换 (蓝色触摸屏分辨率800*480 -> LCD 800x480)
            // x = last_touch.x;
            // y = last_touch.y;
            valid = 1;                        // 标记有效
            last_touch.valid = 0;             // 重置事件标志
        }
        pthread_mutex_unlock(&touch_mutex);    // 解锁
        
        // 处理有效触摸事件
        if (valid) {
            printf("Touch at (%d, %d)\n", x, y);
            
            // 按钮区域检测 (根据坐标判断按下的按钮)
            
            // 打开摄像头按钮 (左上区域)
            if (x > 0 && x < 80 && y > 0 && y < 180) {
                printf("Opening camera\n");
                is_open_camera = 1;           // 开启摄像头
            }
            // 关闭摄像头按钮 (右上区域)
            else if (x > 720 && x < 800 && y > 0 && y < 180) {
                printf("Closing camera\n");
                is_open_camera = 0;           // 关闭摄像头
            }
            // 录像按钮 (左下区域)
            else if (x > 0 && x < 80 && y > 280 && y < 480) {
                printf("%s video recording\n", is_vedio_record ? "Stopping" : "Starting");
                is_vedio_record = !is_vedio_record; // 切换录像状态
            }
            // 拍照按钮 (中下区域)
            else if (x > 350 && x < 450 && y > 280 && y < 480) {
                printf("Taking photo\n");
                take_photo = 1;               // 触发拍照
            }
            // 播放按钮 (右下区域)
            else if (x > 720 && x < 800 && y > 280 && y < 480) {
                printf("Playing video\n");
                is_video_play = 1;            // 触发播放
            }
            // 退出按钮 (中上区域)
            else if(x > 350 && x < 450 && y > 0 && y < 100) {
                printf("欢迎使用摄像头，下次见\n");
                sleep(2);                     // 显示消息
                break;                        // 退出循环
            }
        }
        
        usleep(50000);                        // 主循环延迟 (50ms)
    }
    
    // 清理资源
    is_open_camera = 0;                       // 重置摄像头标志
    is_vedio_record = 0;                      // 重置录像标志
    is_video_play = 0;                        // 重置播放标志
    
    // 终止线程
    pthread_cancel(touch_tid);
    pthread_cancel(capture_tid);
    close(ts_fd);                             // 关闭触摸屏
    
    // 显示欢送界面
    lcd_draw_bmp("welcome.bmp", 0, 0);
}

int main(int argc, char **argv) {
    // 创建存储目录
    mkdir("./myvideo", 0777);                 // 视频存储目录
    mkdir("./photo", 0777);                   // 照片存储目录
    
    // 初始化LCD
    if (lcd_open() != 0) {
        fprintf(stderr, "LCD initialization failed\n");
        return 1;
    }
    
    // 初始化摄像头设备
    if (linux_v4l2_device_init("/dev/video7") != 0) {
        fprintf(stderr, "Camera initialization failed\n");
        munmap(p, LCD_WIDTH * LCD_HEIGHT * 4);
        close(lcd_fd);
        return 1;
    }
    
    usleep(10000);                            // 初始化延迟
    linux_v4l2_start_capturing();             // 开始摄像头采集
    
    // 运行主摄像头功能
    camera();
    
    // 清理资源
    linux_v4l2_stop_capturing();              // 停止摄像头采集
    // linux_v4l2_device_uninit();            // 设备反初始化 (注释掉)
    munmap(p, LCD_WIDTH * LCD_HEIGHT * 4);    // 解除内存映射
    close(lcd_fd);                            // 关闭LCD设备
    
    return 0;
}