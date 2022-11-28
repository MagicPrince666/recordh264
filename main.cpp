/**
 * @file main.cpp
 * @author 黄李全 (846863428@qq.com)
 * @brief 
 * @version 0.1
 * @date 2022-11-25
 * @copyright Copyright (c) {2021} 个人版权所有
 */

#include <iostream>
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"  // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h" // support for user defined types

#include "mjpgrecord.h"
#include "epoll.h"

int main (int argc, char **argv) 
{
    std::string dev = "/dev/video0";
    if(argc > 1) {
        dev = (char *)argv[1];
    }

    spdlog::info("Use device {}", dev);

    MjpgRecord mjpg(dev, "test.avi");
    mjpg.Init();

    MY_EPOLL.EpollLoop();

    return 0; 
}

