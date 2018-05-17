/*****************************************************************************************

 * 文件名  thread.cpp
 * 描述    ：多线程控制
 * 平台    ：linux
 * 版本    ：V1.0.0
 * 作者    ：Leo Huang  QQ：846863428
 * 邮箱    ：Leo.huang@junchentech.cn
 * 修改时间  ：2017-06-28

*****************************************************************************************/

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <pthread.h> 
#include <assert.h> 
#include <signal.h>
#include <faac.h>

#include "H264_UVC_TestAP.h"
#include "recorder.h"

int record_aac = 1;

void * recordaac (void *arg) 
{
    Recorder  recorder;
    unsigned  char* buf;

    while(record_aac)
    {
        int r = recorder.recodeAAC(buf);
        printf("RET : %d\n",r);
    }

    pthread_exit(NULL);
}


pthread_mutex_t mut;//声明互斥变量 

static void sigint_handler(int sig)
{
    capturing = 0;//停止视频
    record_aac = 0;//停止录音
    printf("-----@@@@@ sigint_handler  is over !\n");
    
}


int main (int argc, char **argv) 
{ 
    pthread_t pthread_id[3];//线程ID

    Init_264camera();
     
    pthread_mutex_init(&mut,NULL);

    signal(SIGINT, sigint_handler);//信号处理

    if (pthread_create(&pthread_id[0], NULL, recordaac, NULL))//开启录音
        printf("Create recordaac error!\n");

    if (pthread_create(&pthread_id[1], NULL, cap_video, NULL))//开启视频录制
        printf("Create cap_video error!\n");

    if(pthread_id[0] !=0) {                   
            pthread_join(pthread_id[0],NULL);
            printf("record %ld exit!\n",pthread_id[0]);
    }

    if(pthread_id[1] !=0) {                   
            pthread_join(pthread_id[1],NULL);
            printf("cap_video %ld exit!\n",pthread_id[1]);
    }

    return 0; 
}

