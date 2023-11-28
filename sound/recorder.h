/*****************************************************************************************

 * 文件名  recorder.h
 * 描述    ：从PCM接口获取音频并压缩成AAC格式
 * 音频格式 :音频源 UAC接口 格式是 PCM SND_PCM_FORMAT_S16_LE 32KHz 单声道
 * 平台    ：linux
 * 版本    ：V1.0.0
 * 作者    ：Leo Huang  QQ：846863428
 * 邮箱    ：Leo.huang@junchentech.cn
 * 修改时间  ：2017-06-28

*****************************************************************************************/
#ifndef RECORDER_H
#define RECORDER_H

/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <faac.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long ULONG;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef char _TCHAR;

#define RATE 32000 // PCM采样频率
#define SIZE 16    // PCM量化位数
#define CHANNELS 1 // PCM声道数目

class Recorder
{

public:
    explicit Recorder(int rate = RATE, int formate = SND_PCM_FORMAT_S16_LE, int channels = CHANNELS, snd_pcm_uframes_t frames = (snd_pcm_uframes_t)48);
    ~Recorder();

    void initPcm();
    void initAAC();
    int getSize() { return size; }
    int getFactor() { return factor; }
    snd_pcm_uframes_t getFrames() { return pcm_frames; }
    int recodePcm(char *&buffer, snd_pcm_uframes_t frame); //执行录音操作：参数1:采集到的音频的存放数据，参数2：大小
    int recodeAAC(unsigned char *&bufferOut);
    int recodeAAC();

    FILE *fpOut;
    FILE *fpPcm;

private:
    snd_pcm_t *handle;
    int factor;
    int size;
    snd_pcm_uframes_t pcm_frames; // set frames of each period
    int pcm_rate;                 //采样率
    int pcm_format;               // pcm采样格式
    int pcm_channels;             // pcm声道
    faacEncHandle hEncoder;

    int sizeOfperiod;
    int looptimes;
    int loopmode;

    char *buffer;

    ULONG nInputSamples;
    int nPCMBufferSize;
    BYTE *pbPCMBuffer;
    BYTE *pbAACBuffer;
    ULONG nMaxOutputBytes;

    bool bThreadRunFlag;
};

#endif // RECORDER_H
