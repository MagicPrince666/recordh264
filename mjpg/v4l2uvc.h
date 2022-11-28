/**
 * @file v4l2uvc.h
 * @author 黄李全 (846863428@qq.com)
 * @brief v4l2
 * @version 0.1
 * @date 2022-11-27
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <iostream>

#include "avilib.h"

#define NB_BUFFER 4
#define DHT_SIZE 432
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define FOURCC_FORMAT "%c%c%c%c"
#define FOURCC_ARGS(c) (c) & 0xFF, ((c) >> 8) & 0xFF, ((c) >> 16) & 0xFF, ((c) >> 24) & 0xFF

struct vdIn {
    int32_t fd;
    char *videodevice;
    int8_t *status;
    int8_t *pictName;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    uint8_t *tmpbuffer;
    uint8_t *framebuffer;
    int32_t isstreaming;
    int32_t grabmethod;
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t formatIn;
    uint32_t formatOut;
    uint32_t framesizeIn;
    uint32_t signalquit;
    int32_t framecount;
    uint32_t getPict;
};

class V4l2Video
{
public:
    V4l2Video(std::string device, int width, int height, int fps, int format, int grabmethod);
    ~V4l2Video();

    int InitVideoIn();
    int CloseV4l2();
    int VideoEnable();
    int VideoDisable();

private:
    int InitV4l2();
    /**
     * @brief 解除mmap映射
     * @return true
     * @return false
     */
    bool UninitMmap();
    int EnumFrameIntervals(int dev, __u32 pixfmt, __u32 width, __u32 height);
    int EnumFrameSizes(int dev, __u32 pixfmt);
    int EnumFrameFormats(int dev, unsigned int *supported_formats, unsigned int max_formats);

private:
    struct vdIn *video_;
    std::string v4l2_device_;
};
