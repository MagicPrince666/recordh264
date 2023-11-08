/**
 * @file video_source.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 视频抽象类
 * @version 0.1
 * @date 2023-03-21
 * @copyright Copyright (c) {2023} 个人版权所有,仅供学习
 */
#ifndef __VIDEO_SOURCE_H__
#define __VIDEO_SOURCE_H__

#include <iostream>
#include <memory>

// 实际使用到的视频流
class VideoStream {
public:
    VideoStream(std::string dev, uint32_t width, uint32_t height, uint32_t fps) 
    :dev_name_(dev),
    video_width_(width),
    video_height_(height),
    video_fps_(fps) {}

    virtual ~VideoStream() {}
    virtual int32_t getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes) = 0;
    virtual void Init() = 0;

protected:
	std::string dev_name_;
    uint32_t video_width_;
    uint32_t video_height_;
    uint32_t video_fps_;
};

// 视频工厂基类
class VideoFactory
{
public:
	virtual VideoStream* createVideoStream(std::string dev, uint32_t width, uint32_t height, uint32_t fps) = 0;
};

#endif
