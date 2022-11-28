/**
 * @file mjpgrecord.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 录制MJPG格式视频
 * @version 0.1
 * @date 2022-11-27
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#pragma once

#include "v4l2uvc.h"
#include "avilib.h"

#include <iostream>
#include <thread>

class MjpgRecord
{
public:
    MjpgRecord(std::string device);
    ~MjpgRecord();

    /**
     * @brief 初始化
     * @return true 
     * @return false 
     */
    bool Init();

    /**
     * @brief 停止录制
     */
    void StopCap();

private:
    /**
     * @brief 抓取并保存视频
     * @return true 
     * @return false 
     */
    bool CapAndSaveVideo();

    /**
     * @brief 视频获取线程
     */
    void VideoCapThread();

    /**
     * @brief 时间戳
     * @return std::string 
     */
    std::string getCurrentTime8();

private:
    struct vdIn *video_;
    std::string v4l2_device_;
    V4l2Video *mjpg_cap_;
    AviLib *avi_lib_;
    std::thread cat_avi_thread_;
    bool capturing_;
};
