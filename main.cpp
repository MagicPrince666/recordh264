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
#include <execinfo.h>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define PRINT_SIZE_ 100
const char *g_exe_name;

void Execute(std::string cmdline, std::string &recv)
{
#if defined(__unix__)
    FILE *stream = NULL;
    char buff[1024];
    char recv_buff[256]      = {0};

    memset(recv_buff, 0, sizeof(recv_buff));

    if ((stream = popen(cmdline.c_str(), "r")) != NULL) {
        while (fgets(buff, 1024, stream)) {
            strcat(recv_buff, buff);
        }
    }
    recv = recv_buff;
    pclose(stream);
#endif
}

void SuperExecute(std::string cmdline, std::string passwd)
{
#if defined(__unix__)
    char cmd[256] = {0};
    int len = snprintf(cmd, sizeof(cmd), "echo %s | sudo -S %s", passwd.c_str(), cmdline.c_str());
    cmd[len] = 0;
    system(cmd);
#endif
}

void Addr2Line(std::string exe, std::vector<std::string>& strs)
{
#if defined(__unix__)
    char str[1024] = {0};
    for (uint32_t i = 0; i < strs.size(); i++) {
        std::string line = strs[i];
        std::string::size_type index = line.find("(+"); // found start stuck
        line = line.substr(index + 1, line.size() - index - 1);
        if (index != std::string::npos) {
            index = line.find(")"); // foud end
            if (index != std::string::npos) {
                line = line.substr(0, index);
                int len = snprintf(str, sizeof(str), "addr2line -e %s %s", exe.c_str(), line.c_str());
                str[len] = 0;
                // std::cout << "Run " << str << std::endl;
                std::string recv;
                Execute(str, recv);
                std::ofstream outfile;
                if (recv.find("??") == std::string::npos) {
                    outfile.open("coredump.log", std::ios::out | std::ios::app);
                    if (outfile.is_open()) {
                        outfile << recv;
                        outfile.close();
                    }
                }
            }
        }
    }
#endif
}

static void _signal_handler(int signum)
{
    void *array[PRINT_SIZE_];
    char **strings;

    size_t size = backtrace(array, PRINT_SIZE_);
    strings     = backtrace_symbols(array, size);

    if (strings == nullptr) {
        fprintf(stderr, "backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    switch (signum) {
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
#ifdef BACKTRACE_DEBUG
    std::vector<std::string> strs;
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%ld %s \n", i, strings[i]);
        strs.push_back(strings[i]);
    }
    Addr2Line(g_exe_name, strs);
#else
    std::string path = std::string(g_exe_name) + ".log";
    std::ofstream outfile(path, std::ios::out | std::ios::app);
    if (outfile.is_open()) {
        outfile << "Commit ID: " << GIT_VERSION << std::endl;
        outfile << "Git path: " << GIT_PATH << std::endl;
        outfile << "Compile time: " << __TIME__ << " " << __DATE__ << std::endl;
    }
    for (size_t i = 0; i < size; i++) {
        fprintf(stderr, "%ld %s \n", i, strings[i]);
        if (outfile.is_open()) {
            outfile << strings[i] << std::endl;
        }
    }
    if (outfile.is_open()) {
        outfile.close();
    }
#endif
    free(strings);
    signal(signum, SIG_DFL); /* 还原默认的信号处理handler */
    fprintf(stderr, "Quit execute now\n");
    fflush(stderr);
    exit(-1);
}

int main(int argc, char **argv)
{
    g_exe_name = argv[0];
    signal(SIGPIPE, _signal_handler); // SIGPIPE，管道破裂。
    signal(SIGSEGV, _signal_handler); // SIGSEGV，非法内存访问
    signal(SIGFPE, _signal_handler);  // SIGFPE，数学相关的异常，如被0除，浮点溢出，等等
    signal(SIGABRT, _signal_handler); // SIGABRT，由调用abort函数产生，进程非正常退出

    spdlog::info("chip hardware concurrency {} !", std::thread::hardware_concurrency());

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
#if defined(USE_RK_HW_ENCODER)
        video_stream_factory = std::make_shared<MppCamera>();
#else
        video_stream_factory = std::make_shared<UvcH264Camera>();
#endif
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
#ifdef USE_LIBFAAC
    std::unique_ptr<Recorder> record(new Recorder());
#endif

    while (true) {
#if !defined(USE_RK_HW_ENCODER)
#ifdef USE_LIBFAAC
        if (record) {
            record->recodeAAC();
        }
#endif
#endif
        usleep(100000);
    }

    return 0;
}
