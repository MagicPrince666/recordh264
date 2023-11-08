/**
 * @file mjpgrecord.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 录制MJPG格式视频
 * @version 0.1
 * @date 2022-11-27
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#pragma once

#include "avilib.h"
#include "v4l2uvc.h"

#include "video_source.h"
#include <iostream>
#include <thread>

class MjpgRecord : public VideoStream
{
public:
    MjpgRecord(std::string dev, uint32_t width, uint32_t height, uint32_t fps);
    virtual ~MjpgRecord();

    /**
     * @brief 初始化
     * @return true
     * @return false
     */
    void Init();

    /**
     * @brief 给live555用
     * @param fTo
     * @param fMaxSize
     * @param fFrameSize
     * @param fNumTruncatedBytes
     * @return int32_t
     */
    int32_t getData(void *fTo, unsigned fMaxSize, unsigned &fFrameSize, unsigned &fNumTruncatedBytes);

private:
    /**
     * @brief 抓取并保存视频
     * @return true
     * @return false
     */
    bool CapAndSaveVideo();

    /**
     * @brief 时间戳
     * @return std::string
     */
    std::string getCurrentTime8();

    /**
     * @brief 停止录制
     */
    void StopCap();

private:
    struct vdIn *video_;
    std::shared_ptr<V4l2Video> mjpg_cap_;
    std::shared_ptr<AviLib> avi_lib_;
};

// 生产mpjg摄像头的工厂
class MjpgCamera : public VideoFactory
{
public:
    VideoStream *createVideoStream(std::string dev, uint32_t width, uint32_t height, uint32_t fps)
    {
        return new MjpgRecord(dev, width, height, fps);
    }
};
