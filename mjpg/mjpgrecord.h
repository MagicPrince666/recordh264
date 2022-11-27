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
    MjpgRecord(std::string file_name);
    ~MjpgRecord();

    bool Init();

    void StopCap();

private:
    bool CapAndSaveVideo();

    void VideoCapThread();

private:
    struct vdIn *video_;
    V4l2Video *mjpg_cap_;
    FILE *avi_file_;
    std::string file_name_;
    AviLib *avi_lib_;
    std::thread cat_avi_thread_;
    bool capturing_;
};
