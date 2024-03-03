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
#if defined(USE_RK_HW_ENCODER)
#include "rk_mpp/rk_mpp_encoder.h"
#endif

#ifdef BACKTRACE_DEBUG
#include <signal.h>
#include <execinfo.h>

#define PRINT_SIZE_ 100

static void _signal_handler(int signum)
{
    void *array[PRINT_SIZE_];
    char **strings;

    size_t size = backtrace(array, PRINT_SIZE_);
    strings = backtrace_symbols(array, size);

    if (strings == nullptr) {
	   fprintf(stderr, "backtrace_symbols");
	   exit(EXIT_FAILURE);
    }

    switch(signum) {
        case SIGSEGV:
        fprintf(stderr, "widebright received SIGSEGV! Stack trace:\n");
        break;

        case SIGPIPE:
        fprintf(stderr, "widebright received SIGPIPE! Stack trace:\n");
        break;

        case SIGFPE:
        fprintf(stderr, "widebright received SIGFPE! Stack trace:\n");
        break;

        case SIGABRT:
        fprintf(stderr, "widebright received SIGABRT! Stack trace:\n");
        break;

        default:
        break;
    }

    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%zu %s \n", i, strings[i]);
    }

    free(strings);
    signal(signum, SIG_DFL); /* 还原默认的信号处理handler */

    exit(1);
}
#endif

int main(int argc, char **argv)
{
#ifdef BACKTRACE_DEBUG
    signal(SIGPIPE, _signal_handler);  // SIGPIPE，管道破裂。
    signal(SIGSEGV, _signal_handler);  // SIGSEGV，非法内存访问
    signal(SIGFPE, _signal_handler);  // SIGFPE，数学相关的异常，如被0除，浮点溢出，等等
    signal(SIGABRT, _signal_handler);  // SIGABRT，由调用abort函数产生，进程非正常退出
#endif

    spdlog::info("chip hardware concurrency {} !", std::thread::hardware_concurrency());

#if defined(USE_RK_HW_ENCODER)
    std::shared_ptr<RkMppEncoder> rk_encoder = std::make_shared<RkMppEncoder>();
#else

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

#endif
    while (true) {
#if !defined(USE_RK_HW_ENCODER)
        if (record) {
            record->recodeAAC();
        }
#endif
        usleep(100000);
    }

    return 0;
}
