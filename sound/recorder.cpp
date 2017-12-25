/*****************************************************************************************

 * 文件名  recorder.cpp
 * 描述    ：从PCM接口获取音频并压缩成AAC格式
 * 音频格式 :音频源 UAC接口 格式是 PCM SND_PCM_FORMAT_S16_LE 32KHz 单声道
 * 平台    ：linux
 * 版本    ：V1.0.0
 * 作者    ：Leo Huang  QQ：846863428
 * 邮箱    ：Leo.huang@junchentech.cn
 * 修改时间  ：2017-06-28

*****************************************************************************************/
#include "recorder.h"

#define ALSA_PCM_NEW_HW_PARAMS_API  

Recorder::Recorder(int rate, int formate, int channels, snd_pcm_uframes_t frames)
{
    bThreadRunFlag = true;
    pcm_rate = rate;
    pcm_format = formate;
    pcm_channels = channels;
    pcm_frames = frames;
 
    size = 1024;

    buffer = new char[10240];

    remove("Record.aac");
    //remove("Record.pcm");

    fpOut = fopen("Record.aac", "wb");
    //fpPcm = fopen("Record.pcm", "wb");

    initPcm();
    initAAC();   
}

Recorder::~Recorder()
{
    bThreadRunFlag = false;
    faacEncClose(hEncoder);
    snd_pcm_close(handle);
    delete[] buffer;
    delete[] pbAACBuffer;
    delete[] pbPCMBuffer;
    fclose(fpOut);
//    fclose(fpPcm);
}

void Recorder::initPcm()
{
    int rc;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;

    /* open PCM device for recording (capture) */
    rc = snd_pcm_open(&handle, "hw:1,0", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* alloc a hardware params object */
    snd_pcm_hw_params_alloca(&params);

    /* fill it with default values */
    snd_pcm_hw_params_any(handle, params);

    /* interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    /* signed 16 bit little ending format */
    snd_pcm_hw_params_set_format(handle, params, (snd_pcm_format_t)pcm_format);

    /* two channels */
    snd_pcm_hw_params_set_channels(handle, params, pcm_channels);

    /* 44100 bits/second sampling rate (CD quality) */
    val = pcm_rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

    /* set period size to 32 frames */
    snd_pcm_uframes_t frames = pcm_frames;//设置每个周期多少帧、一个周期就是一个完整的中断;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    printf("frames: %lu  pcm_frames:%lu \n", frames, pcm_frames);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw params: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    printf("frames: %lu  pcm_frames:%lu \n", frames, pcm_frames);
    pcm_frames = frames;


    factor = pcm_channels;
    factor = factor * (pcm_format == SND_PCM_FORMAT_S16_LE ? 2:1);
    //factor 每一帧的字节
    
    size = frames * factor; //一个周期的字节数
    printf("factor_: %d  pcm_channels:%d size:%d frames:%lu\n", factor, pcm_channels,size,frames);

}

void Recorder::initAAC()
{
    ULONG nSampleRate = pcm_rate;  // 采样率
    UINT nChannels = pcm_channels;         // 声道数
    UINT nPCMBitSize = pcm_format == SND_PCM_FORMAT_S16_LE ?16:8;      // 单样本位数

    faacEncConfigurationPtr pConfiguration;

    // (1) Open FAAC engine
    //nInputSamples 帧的长度
    hEncoder = faacEncOpen(nSampleRate, nChannels, &nInputSamples, &nMaxOutputBytes);
    if(hEncoder == NULL)
    {
        printf("[ERROR] Failed to call faacEncOpen()\n");
        return;
    }

    //帧长度 ，每一帧 * 2  (16/8)
    nPCMBufferSize = nInputSamples * nPCMBitSize / 8;
    pbPCMBuffer = new BYTE [nPCMBufferSize];
    pbAACBuffer = new BYTE [nMaxOutputBytes];

    // (2.1) Get current encoding configuration
    pConfiguration = faacEncGetCurrentConfiguration(hEncoder);
    pConfiguration->inputFormat = FAAC_INPUT_16BIT;
    pConfiguration->aacObjectType = LOW;    //LC编码


    // (2.2) Set encoding configuration
    //nRet = faacEncSetConfiguration(hEncoder, pConfiguration);
    faacEncSetConfiguration(hEncoder, pConfiguration);

    snd_pcm_uframes_t framesofperod = getFrames();
    int factor = getFactor();

    sizeOfperiod = framesofperod * factor;
    looptimes = nPCMBufferSize/sizeOfperiod;
    loopmode = nPCMBufferSize%sizeOfperiod;
    loopmode = loopmode / factor;
  
}


int Recorder::recodeAAC(unsigned char* &bufferOut)
{
    if(!bThreadRunFlag) return 0;
    //int rc = 0;
    //printf("recodeAAC 1 %d\n",looptimes);
    for(int j=0; j < looptimes; j++)
    {
        //rc = recodePcm(buffer,getFrames());
        recodePcm(buffer,getFrames());
        memcpy(pbPCMBuffer+j*sizeOfperiod, buffer, getSize());
    }
    loopmode = 16;
    //printf("recodeAAC 2 %d\n",loopmode);

    //rc = recodePcm(buffer,loopmode);
    recodePcm(buffer,loopmode);//卡在这里 loopmode = 0 导致卡住
    //printf("recodeAAC 3\n");

    memcpy(pbPCMBuffer+looptimes*sizeOfperiod, buffer, loopmode * getFactor());

    //fwrite(pbPCMBuffer, 1, nPCMBufferSize, fpPcm);

    int nRet = faacEncEncode(hEncoder, (int*) pbPCMBuffer, nInputSamples, pbAACBuffer, nMaxOutputBytes);
    //printf("recodeAAC 4 %d\n",nRet);

    // printf("faacEncEncode nRet:%d\n ",nRet);
    fwrite(pbAACBuffer, 1, nRet, fpOut);

    bufferOut = (unsigned char*)pbAACBuffer;

    return nRet;
}

int Recorder::recodePcm(char* &buffer,snd_pcm_uframes_t frame)
{
    // printf("Recorder::recode 1 handle:%d bufsize:%d\n",handle,bufsize);

    if(!bThreadRunFlag) return 0;

    int rc;
    do
    {
        rc = snd_pcm_readi(handle, buffer, frame);
             
        if (rc == -EPIPE)
        {
        /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");snd_pcm_prepare(handle);
            usleep(100);
        }
        else if (rc < 0)
        {
            buffer = NULL;
            fprintf(stderr,"error from read: %s\n",snd_strerror(rc));
        }
        else if (rc != (int)frame)
        {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }

    }while(rc <= 0);


    return rc;
}


