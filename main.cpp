/**
 * @file main.cpp
 * @author 黄李全 (846863428@qq.com)
 * @brief 
 * @version 0.1
 * @date 2022-11-25
 * @copyright Copyright (c) {2021} 个人版权所有
 */

#include <iostream>
#include <thread>

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types

#include "H264_UVC_Cap.h"
#include "mjpgrecord.h"
#include "h264_camera.h"
#include "epoll.h"

int main (int argc, char **argv) 
{
    std::string format = "h264";
    std::string dev = "/dev/video0";

    if(argc > 2) {
        dev = (char *)argv[1];
        format = (char *)argv[2];
    } else {
        spdlog::error("Please Set video farmat");
        spdlog::info("Eg:{} /dev/video0 h264", argv[0]);
        return 1;
    }

    spdlog::info("{} farmat = {}", dev, format);

    spdlog::info("chip hardware concurrency {} !", std::thread::hardware_concurrency());

    std::thread video_cap_loop([]() { MY_EPOLL.EpollLoop(); });
    video_cap_loop.detach();

    if(format == "h264") {
        // 硬件编码H264
        H264UvcCap h264(dev, 1280, 720);
        h264.Init();
    } else if(format == "mjpg") {
        // 硬件编码MJPG
        MjpgRecord mjpg(dev);
        mjpg.Init();
    } else if(format == "sh264") {
        // 软件编码H264
        V4l2H264hData sh264(dev);
        sh264.Init();
    } else {
        spdlog::error("Not support farmat");
    }

    while (true) {
        sleep(1);
    }

    return 0; 
}

