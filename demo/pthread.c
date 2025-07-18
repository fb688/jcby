#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *show(void *arg)
{
    //td1线程所需要完成的一个事
    while(1){
        printf("2022101161刘凡博\n");
        sleep(1);
    }
}

int main()
{

    pthread_t td1;
    pthread_create(&td1,NULL,show,NULL);
    while(1){
        printf("5\n");
        sleep(1);
    }
    pthread_cancel(td1);
}