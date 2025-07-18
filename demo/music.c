#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>

char mp3[2][20] = 
{
    "1.mp3",
    "2.mp3"
};


int main(void)
{
    //1、播放音乐
    // system("madplay faded.mp3 &"); //&表示后台运行进程
    int yl = 0;   //音量控制变量  -175 ~ +18  -50声音已经最小了
    int jd = 0;     //音乐进度控制  时间为秒数  不能超过音乐本身的时间长
    char cmd[30] = {0};
    
    sprintf(cmd, "madplay %s -s%d -a%d &", mp3[0], jd, yl);        //-s控制进度        -a控制音量        
    printf("cmd=%s\n", cmd);
    system(cmd);
    sleep(7);
    
    yl = 10;   
    jd = 15;
    bzero(cmd, sizeof(cmd));//清空数组，赋值为0
    sprintf(cmd, "madplay %s -s%d -a%d &", mp3[0], jd, yl);
    system("killall -9 madplay");
    system(cmd);
    sleep(7);
    
    yl = -20;  
    jd = 40;     
    bzero(cmd, sizeof(cmd));//清空数组，赋值为0
    sprintf(cmd, "madplay %s -s%d -a%d &", mp3[0], jd, yl);
    system("killall -9 madplay");
    system(cmd);
    sleep(7);
    
    system("killall -9 madplay");
    //2、暂停播放
    // system("killall -19 madplay");
    // sleep(4);
    
    //3、继续播放
    // system("killall -18 madplay");
    // sleep(7);
    
    
    //4、关闭音乐
    // system("killall -9 madplay");
    
    return 0;
}