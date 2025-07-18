#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int i = 0;
	
	//创建管道文件
	int ret = mkfifo("/tmp/myfifo", 777);
	if(ret == -1)
	{
		perror("创建管道文件失败");
	}
	
	//打开管道文件
	int fifo_fd = open("/tmp/myfifo", O_RDWR);
	if(fifo_fd == -1)
	{
		perror("打开管道文件失败");
		return -1;
	}
	
	//播放视频
	system("mplayer -input file=/tmp/myfifo -zoom -x 800 -y 400 ../VIDEO/2.mp4 &");
	
	while(1)
	{
		for(i = 0; i < 30; i++)
		{
			//减音量
			write(fifo_fd, "volume -50\n", 11);
			sleep(1);
		}
		for(i = 0; i < 30; i++)
		{
			//加音量
			write(fifo_fd, "volume 50\n", 10);
			sleep(1);
		}
	}
	
	//关闭管道文件
	close(fifo_fd);
	
	return 0;
}
