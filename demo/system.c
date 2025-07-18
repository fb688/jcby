#include <stdio.h>
#include <stdlib.h>

int main()
{
    char str[256];
    char *file_name[2] = {"1.mp3","2.mp3"};
    int i = 0;
    //进行字符串的拼接
    sprintf(str,"madplay %s -a %d&",file_name[i],18);

    //系统进行播放
    system(str);
    i++;
    sleep(10);
    //暂停播放音乐
    system("killall -19 madplay");
    sleep(3);
    //继续播放音乐
    system("killall -18 madplay");
    sleep(10);
    //杀死播放音乐（退出)
    system("killall -9 madplay");
    bzero(str, sizeof(str));
    sprintf(str,"madplay %s -a %d&",file_name[i],18);
    system(str);
    sleep(10);
    system("killall -9 madplay");
        
    return 0;
}