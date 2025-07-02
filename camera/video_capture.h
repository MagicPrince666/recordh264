/**
 * @file video_capture.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 获取v4l2视频
 * @version 0.1
 * @date 2022-11-18
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#ifndef __VIDEO_CAPTURE_H__
#define __VIDEO_CAPTURE_H__

#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <functional>

#if defined(__linux__)
#include <linux/videodev2.h>
struct Buffer {
    void *start;
    size_t length;
};

struct Camera {
    int32_t fd     = -1;
    int32_t width  = 640;
    int32_t height = 480;
    int32_t fps    = 30;
    struct v4l2_capability v4l2_cap;
    struct v4l2_cropcap v4l2_cropcap;
    struct v4l2_format v4l2_fmt;
    struct v4l2_crop crop;
    struct Buffer *buffers;
};

class V4l2VideoCapture
{
public:
    V4l2VideoCapture(std::string dev = "/dev/video0", uint32_t width = 640, uint32_t height = 480, uint32_t fps = 30);
    ~V4l2VideoCapture();

    /**
     * @brief 枚举摄像头信息
     * @return true 
     * @return false 
     */
    bool EnumV4l2Format();

    /**
     * @brief 初始化
     */
    bool Init(uint32_t pixelformat);

    /**
     * @brief 获取一帧数据
     * @param data 拷贝到此地址
     * @param offset 地址偏移
     * @param maxsize 最大长度
     * @return uint64_t
     */
    uint64_t BuffOneFrame(uint8_t *data);

    /**
     * 获取帧长度
     */
    int32_t GetFrameLength();

    /**
     * 获取视频格式
     */
    struct Camera *GetFormat();

    /**
     * @brief 回调注册
     * @param handler 
     */
    void AddCallback(std::function<bool(const uint8_t *, const uint32_t)> handler);

    /**
     * @brief 打开摄像头
     */
    bool OpenCamera();

    /**
     * @brief 关闭摄像头
     */
    bool CloseCamera();

    /**
     * @brief 是否支持MJPG
     * @return true 
     * @return false 
     */
    bool CanOutputMjpg() {
        return can_ouput_mjpg_;
    }

    /**
     * @brief 
     * @return true 
     * @return false 
     */
    bool CanOutputStream() {
        return can_ouput_stream_;
    }

    /**
     * @brief 保存jpg图像
     * @param buffer 
     * @param size 
     * @param name 
     */
    void SaveJpeg(void* buffer, uint64_t size, std::string name);

private:
    /**
     * @brief 关闭v4l2资源
     */
    bool V4l2Close();

    /**
     * @brief 开始录制
     */
    bool StartPreviewing();

    /**
     * @brief 停止录制
     */
    bool StopPreviewing();

    /**
     * @brief 初始化摄像头参数
     */
    bool InitCamera();

    /**
     * @brief 退出设置
     */
    bool UninitCamera();

    /**
     * @brief 初始化mmap
     */
    bool InitMmap();

    void NV12_to_YUYV(int width, int height, void *src, void *dst);

    void yuyv422ToYuv420p(int inWidth, int inHeight, uint8_t *pSrc, uint8_t *pDest);

    const char *V4l2FormatToString(uint32_t pixelformat);
    void ErrnoExit(const char *s);
    int32_t xioctl(int32_t fd, int32_t request, void *arg);

    void ReadBuffOneFrame();

private:
    std::string v4l2_device_;
    uint32_t n_buffers_;
    struct Camera camera_;
    /*call back function*/
    std::function<bool(const uint8_t *, const uint32_t)> process_image_;
    bool can_ouput_mjpg_;
    bool can_ouput_stream_;
    uint32_t pixelformat_;
};

#endif
#endif
