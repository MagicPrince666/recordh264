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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define FOURCC_FORMAT "%c%c%c%c"
#define FOURCC_ARGS(c) (c) & 0xFF, ((c) >> 8) & 0xFF, ((c) >> 16) & 0xFF, ((c) >> 24) & 0xFF

struct vdIn {
    int32_t fd      = -1;
    uint32_t width  = 640;
    uint32_t height = 480;
    uint32_t fps    = 30;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    uint8_t *tmpbuffer   = nullptr;
    uint8_t *framebuffer = nullptr;
    int32_t isstreaming;
    int32_t grabmethod;
    uint32_t formatIn;
    uint32_t framesizeIn;
    int32_t framecount;
};

class V4l2Video
{
public:
    V4l2Video(std::string device, int32_t width, int32_t height, int32_t fps, int32_t format, int32_t grabmethod);
    ~V4l2Video();

    /**
     * @brief 初始化v4l2设备
     * @return int32_t
     */
    int32_t InitVideoIn();

    /**
     * @brief 获取设备信息
     * @return struct vdIn*
     */
    struct vdIn *GetV4l2Info();

    /**
     * @brief 关闭v4l2设备
     * @return int32_t
     */
    int32_t CloseV4l2();

    /**
     * @brief 开始视频
     * @return int32_t
     */
    int32_t VideoEnable();

    /**
     * @brief 关闭视频
     * @return int32_t
     */
    int32_t VideoDisable();

private:
    /**
     * @brief 初始化v4l2
     * @return int32_t
     */
    int32_t InitV4l2();

    /**
     * @brief 解除mmap映射
     * @return true
     * @return false
     */
    bool UninitMmap();
    int32_t EnumFrameIntervals(int32_t dev, uint32_t pixfmt, uint32_t width, uint32_t height);
    int32_t EnumFrameSizes(int32_t dev, uint32_t pixfmt);
    int32_t EnumFrameFormats(int32_t dev, uint32_t *supported_formats, uint32_t max_formats);

private:
    struct vdIn *video_;
    std::string v4l2_device_;
    int32_t debug_;
};
