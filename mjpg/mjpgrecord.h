/**
 * @file mjpgrecord.h
 * @author 黄李全 (846863428@qq.com)
 * @brief 录制MJPG格式视频
 * @version 0.1
 * @date 2022-11-27
 * @copyright Copyright (c) {2021} 个人版权所有
 */
#pragma once

class MjpgRecord
{
public:
    MjpgRecord();
    ~MjpgRecord();

private:
    

private:
    struct vdIn *video_;
};
