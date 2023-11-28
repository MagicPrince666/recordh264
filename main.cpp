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

#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types
#include "spdlog/spdlog.h"

#include "H264_UVC_Cap.h"
#include "epoll.h"
#include "h264_camera.h"
#include "mjpgrecord.h"
#include "recorder.h"

int main(int argc, char **argv)
{
    std::string format = "h264";
    std::string dev    = "/dev/video0";

    if (argc > 2) {
        dev    = (char *)argv[1];
        format = (char *)argv[2];
    } else {
        spdlog::error("Please Set video farmat");
        spdlog::info("Eg:{} /dev/video0 h264", argv[0]);
        return 1;
    }

    spdlog::info("{} farmat = {}", dev, format);

    spdlog::info("chip hardware concurrency {} !", std::thread::hardware_concurrency());

    std::shared_ptr<VideoFactory> video_stream_factory;
    if (format == "h264") {
        // 硬件编码H264
        video_stream_factory = std::make_shared<UvcH264Camera>();
    } else if (format == "mjpg") {
        // 硬件编码MJPG
        video_stream_factory = std::make_shared<MjpgCamera>();
    } else if (format == "sh264") {
        // 软件编码H264
        video_stream_factory = std::make_shared<UvcYuyvCamera>();
    } else {
        spdlog::error("Not support farmat");
    }
    if (video_stream_factory) {
        std::shared_ptr<VideoStream> video(video_stream_factory->createVideoStream(dev, 1280, 720, 30));
        video->Init();
    }
    std::unique_ptr<Recorder> record(new Recorder());

    while (true) {
        if (record) {
            record->recodeAAC();
        }
        usleep(100000);
    }

    return 0;
}
